#include "kitty.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <zlib.h>

#if defined(HAVE_LIBPNG)
 #include <png.h>
#endif

#include "base64.h"
#include "debug.h"
#include "grid.h"
#include "image.h"
#include "log.h"
#include "util.h"
#include "xmalloc.h"
#include "xsnprintf.h"

#define LOG_MODULE "kitty"
#define LOG_ENABLE_DBG 0

#define KITTY_MAX_DATA (256u * 1024u * 1024u)
#define KITTY_DEFAULT_QUOTA (320u * 1024u * 1024u)
#define KITTY_MAX_DIM 10000u

static bool
str_has_prefix(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

static uint32_t
new_image_id(struct terminal *term)
{
    struct kitty_graphics *kg = &term->kitty_graphics;

    for (;;) {
        uint32_t id = kg->next_id++;
        if (id == 0)
            continue;

        bool in_use = false;
        tll_foreach(kg->images, it) {
            if (it->item.id == id) {
                in_use = true;
                break;
            }
        }

        if (!in_use)
            return id;
    }
}

static struct image *
find_image_by_id(struct terminal *term, uint32_t id)
{
    tll_foreach(term->kitty_graphics.images, it) {
        if (it->item.id == id)
            return &it->item;
    }
    return NULL;
}

static struct image *
find_image_by_number(struct terminal *term, uint32_t number)
{
    struct image *ret = NULL;
    tll_foreach(term->kitty_graphics.images, it) {
        if (it->item.number == number)
            ret = &it->item;
    }
    return ret;
}

static bool
image_has_placement(struct terminal *term, uint32_t id)
{
    tll_foreach(term->normal.image_placements, it) {
        if (it->item.image_id == id)
            return true;
    }
    tll_foreach(term->alt.image_placements, it) {
        if (it->item.image_id == id)
            return true;
    }
    return false;
}

static void
free_image(struct image *img)
{
    if (img->pix != NULL)
        pixman_image_unref(img->pix);
    free(img->data);
}

static void
remove_image_by_id(struct terminal *term, uint32_t id)
{
    tll_foreach(term->kitty_graphics.images, it) {
        if (it->item.id != id)
            continue;

        term->kitty_graphics.total_bytes -= it->item.bytes;
        free_image(&it->item);
        tll_remove(term->kitty_graphics.images, it);
        return;
    }
}

static void
remove_placements_by_id(struct terminal *term, uint32_t id, uint32_t placement_id)
{
    for (size_t i = 0; i < 2; i++) {
        struct grid *grid = i == 0 ? &term->normal : &term->alt;
        struct grid *saved = term->grid;
        term->grid = grid;

        tll_foreach(grid->image_placements, it) {
            struct image_placement *img = &it->item;
            if (img->image_id != id)
                continue;
            if (placement_id != 0 && img->placement_id != placement_id)
                continue;

            image_placement_destroy(img);
            tll_remove(grid->image_placements, it);
        }

        term->grid = saved;
    }
}

static void
gc_images(struct terminal *term)
{
    tll_foreach(term->kitty_graphics.images, it) {
        if (image_has_placement(term, it->item.id))
            continue;

        term->kitty_graphics.total_bytes -= it->item.bytes;
        free_image(&it->item);
        tll_remove(term->kitty_graphics.images, it);
    }
}

static void
enforce_quota(struct terminal *term, uint32_t keep_id)
{
    struct kitty_graphics *kg = &term->kitty_graphics;
    if (kg->quota == 0)
        return;

    while (kg->total_bytes > kg->quota) {
        struct image *victim = NULL;

        /* Prefer images without placements */
        tll_foreach(kg->images, it) {
            if (it->item.id != keep_id &&
                !image_has_placement(term, it->item.id))
            {
                victim = &it->item;
                break;
            }
        }

        if (victim == NULL) {
            tll_foreach(kg->images, it) {
                if (it->item.id != keep_id) {
                    victim = &it->item;
                    break;
                }
            }
        }

        if (victim == NULL)
            break;

        uint32_t id = victim->id;
        remove_placements_by_id(term, id, 0);
        remove_image_by_id(term, id);
    }
}

static void
store_image(struct terminal *term, struct image *img)
{
    tll_push_back(term->kitty_graphics.images, *img);
    term->kitty_graphics.total_bytes += img->bytes;
    enforce_quota(term, img->id);
}

static void
respond(struct terminal *term, const struct kitty_cmd *cmd,
        uint32_t id, uint32_t number, const char *msg)
{
    if (cmd->quiet >= 2)
        return;
    if (cmd->quiet >= 1 && streq(msg, "OK"))
        return;

    char reply[512];
    size_t len = 0;

    len += xsnprintf(reply + len, sizeof(reply) - len, "\033_G");

    if (id != 0)
        len += xsnprintf(reply + len, sizeof(reply) - len, "i=%u", id);
    if (number != 0)
        len += xsnprintf(reply + len, sizeof(reply) - len,
                         "%sI=%u", id != 0 ? "," : "", number);
    if (cmd->placement_id != 0 && id != 0)
        len += xsnprintf(reply + len, sizeof(reply) - len,
                         ",p=%u", cmd->placement_id);

    len += xsnprintf(reply + len, sizeof(reply) - len, ";%s\033\\", msg);
    term_to_slave(term, reply, len);
}

static void
error_response(struct terminal *term, const struct kitty_cmd *cmd,
               const char *fmt, ...)
{
    char msg[256];
    va_list va;
    va_start(va, fmt);
    vsnprintf(msg, sizeof(msg), fmt, va);
    va_end(va);

    respond(term, cmd, cmd->id, cmd->number, msg);
}

static bool
parse_int(const char *s, size_t len, long long *out)
{
    if (len == 0)
        return false;

    char buf[32];
    if (len >= sizeof(buf))
        return false;

    memcpy(buf, s, len);
    buf[len] = '\0';

    char *end;
    errno = 0;
    long long value = strtoll(buf, &end, 10);
    if (errno != 0 || *end != '\0')
        return false;

    *out = value;
    return true;
}

static void
parse_control(const char *control, size_t len, struct kitty_cmd *cmd)
{
    *cmd = (struct kitty_cmd){
        .action = 't',
        .quiet = 0,
        .format = 32,
        .medium = 'd',
        .compression = 0,
        .more = 0,
        .delete = 'a',
    };

    const char *p = control;
    const char *end = control + len;

    while (p < end) {
        if (*p == ',') {
            p++;
            continue;
        }

        char key = *p++;

        if (p >= end || *p != '=') {
            /* Malformed - skip to next key */
            while (p < end && *p != ',')
                p++;
            continue;
        }

        p++;  /* '=' */
        const char *value = p;
        while (p < end && *p != ',')
            p++;
        size_t value_len = p - value;

        long long v = 0;

        switch (key) {
        case 'a':
            if (value_len == 1)
                cmd->action = value[0];
            break;

        case 'q':
            if (parse_int(value, value_len, &v))
                cmd->quiet = v;
            break;

        case 'f':
            if (parse_int(value, value_len, &v))
                cmd->format = v;
            break;

        case 't':
            if (value_len == 1)
                cmd->medium = value[0];
            break;

        case 's':
            if (parse_int(value, value_len, &v))
                cmd->width = v;
            break;

        case 'v':
            if (parse_int(value, value_len, &v))
                cmd->height = v;
            break;

        case 'S':
            if (parse_int(value, value_len, &v) && v >= 0)
                cmd->data_size = v;
            break;

        case 'O':
            if (parse_int(value, value_len, &v) && v >= 0)
                cmd->data_offset = v;
            break;

        case 'i':
            if (parse_int(value, value_len, &v) && v >= 0)
                cmd->id = v;
            break;

        case 'I':
            if (parse_int(value, value_len, &v) && v >= 0)
                cmd->number = v;
            break;

        case 'p':
            if (parse_int(value, value_len, &v) && v >= 0)
                cmd->placement_id = v;
            break;

        case 'o':
            if (value_len == 1)
                cmd->compression = value[0];
            break;

        case 'm':
            if (parse_int(value, value_len, &v))
                cmd->more = v != 0;
            break;

        case 'x':
            if (parse_int(value, value_len, &v))
                cmd->src_x = v;
            break;

        case 'y':
            if (parse_int(value, value_len, &v))
                cmd->src_y = v;
            break;

        case 'w':
            if (parse_int(value, value_len, &v))
                cmd->src_w = v;
            break;

        case 'h':
            if (parse_int(value, value_len, &v))
                cmd->src_h = v;
            break;

        case 'X':
            if (parse_int(value, value_len, &v))
                cmd->off_x = v;
            break;

        case 'Y':
            if (parse_int(value, value_len, &v))
                cmd->off_y = v;
            break;

        case 'c':
            if (parse_int(value, value_len, &v))
                cmd->cols = v;
            break;

        case 'r':
            if (parse_int(value, value_len, &v))
                cmd->rows = v;
            break;

        case 'C':
            if (parse_int(value, value_len, &v))
                cmd->cursor_policy = v;
            break;

        case 'z':
            if (parse_int(value, value_len, &v))
                cmd->z = v;
            break;

        case 'd':
            if (value_len == 1)
                cmd->delete = value[0];
            break;

        case 'U':
            if (parse_int(value, value_len, &v))
                cmd->unicode_placeholder = v != 0;
            break;

        default:
            /* Unknown keys are ignored */
            break;
        }
    }
}

static uint8_t *
decompress_zlib(const uint8_t *data, size_t len, size_t expected,
                size_t *out_len)
{
    z_stream zs = {0};
    if (inflateInit(&zs) != Z_OK)
        return NULL;

    size_t cap = expected > 0 ? expected : max((size_t)4096, len * 4);
    uint8_t *out = malloc(cap);

    if (out == NULL) {
        inflateEnd(&zs);
        return NULL;
    }

    zs.next_in = (Bytef *)data;
    zs.avail_in = len;

    size_t used = 0;
    int ret;

    do {
        if (used == cap) {
            size_t new_cap = cap * 2;
            if (new_cap > KITTY_MAX_DATA)
                new_cap = KITTY_MAX_DATA;
            if (new_cap == cap)
                goto err;

            uint8_t *new_out = realloc(out, new_cap);
            if (new_out == NULL)
                goto err;
            out = new_out;
            cap = new_cap;
        }

        zs.next_out = out + used;
        zs.avail_out = cap - used;

        ret = inflate(&zs, Z_NO_FLUSH);
        used = cap - zs.avail_out;

        if (ret == Z_STREAM_END)
            break;
        if (ret != Z_OK)
            goto err;
    } while (zs.avail_in > 0 || zs.avail_out == 0);

    if (ret != Z_STREAM_END)
        goto err;

    inflateEnd(&zs);
    *out_len = used;
    return out;

err:
    inflateEnd(&zs);
    free(out);
    return NULL;
}

static uint8_t *
read_regular_file(const char *path, size_t offset, size_t size,
                  bool unlink_after, size_t *out_len)
{
    /* Refuse obviously sensitive locations */
    if (str_has_prefix(path, "/proc") ||
        str_has_prefix(path, "/sys") ||
        (str_has_prefix(path, "/dev") && !str_has_prefix(path, "/dev/shm")))
    {
        return NULL;
    }

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return NULL;
    }

    if ((off_t)offset >= st.st_size) {
        close(fd);
        return NULL;
    }

    size_t available = st.st_size - offset;
    if (size == 0 || size > available)
        size = available;

    if (size == 0 || size > KITTY_MAX_DATA) {
        close(fd);
        return NULL;
    }

    uint8_t *data = malloc(size);
    if (data == NULL) {
        close(fd);
        return NULL;
    }

    size_t got = 0;
    while (got < size) {
        ssize_t n = pread(fd, data + got, size - got, offset + got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            free(data);
            close(fd);
            return NULL;
        }
        if (n == 0)
            break;
        got += n;
    }

    close(fd);

    if (got != size) {
        free(data);
        return NULL;
    }

    if (unlink_after) {
        const char *tmp = getenv("TMPDIR");
        bool safe =
            strstr(path, "tty-graphics-protocol") != NULL &&
            ((tmp != NULL && str_has_prefix(path, tmp)) ||
             str_has_prefix(path, "/tmp/") ||
             str_has_prefix(path, "/dev/shm/") ||
             str_has_prefix(path, "/var/tmp/"));
        if (safe)
            unlink(path);
    }

    *out_len = size;
    return data;
}

static uint8_t *
read_shm(const char *name, size_t offset, size_t size, size_t *out_len)
{
    if (name[0] != '/')
        return NULL;

    int fd = shm_open(name, O_RDONLY, 0);
    if (fd < 0)
        return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0 || (off_t)offset >= st.st_size) {
        shm_unlink(name);
        close(fd);
        return NULL;
    }

    size_t available = st.st_size - offset;
    if (size == 0 || size > available)
        size = available;

    if (size == 0 || size > KITTY_MAX_DATA) {
        shm_unlink(name);
        close(fd);
        return NULL;
    }

    uint8_t *data = malloc(size);
    if (data == NULL) {
        shm_unlink(name);
        close(fd);
        return NULL;
    }

    size_t got = 0;
    while (got < size) {
        ssize_t n = pread(fd, data + got, size - got, offset + got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            free(data);
            data = NULL;
            break;
        }
        if (n == 0)
            break;
        got += n;
    }

    shm_unlink(name);
    close(fd);

    if (data == NULL || got != size) {
        free(data);
        return NULL;
    }

    *out_len = size;
    return data;
}

static uint8_t *
decode_transport(struct terminal *term, const struct kitty_cmd *cmd,
                 const char *payload, size_t payload_len, size_t *out_len)
{
    uint8_t *data = NULL;
    size_t len = 0;

    switch (cmd->medium) {
    case 'd': {
        char *b64 = xstrndup(payload, payload_len);
        char *decoded = base64_decode(b64, &len);
        free(b64);

        if (decoded == NULL)
            return NULL;

        data = (uint8_t *)decoded;
        break;
    }

    case 'b':
        /* Already decoded, from a chunked direct transfer */
        data = (uint8_t *)xmemdup(payload, payload_len);
        len = payload_len;
        break;

    case 'f':
    case 't': {
        char *path = xstrndup(payload, payload_len);
        data = read_regular_file(
            path, cmd->data_offset, cmd->data_size,
            cmd->medium == 't', &len);
        free(path);
        break;
    }

    case 's': {
        char *name = xstrndup(payload, payload_len);
        data = read_shm(name, cmd->data_offset, cmd->data_size, &len);
        free(name);
        break;
    }

    default:
        return NULL;
    }

    if (data == NULL)
        return NULL;

    if (cmd->compression == 'z') {
        size_t expected = 0;
        if (cmd->format == 24)
            expected = (size_t)cmd->width * cmd->height * 3;
        else if (cmd->format == 32)
            expected = (size_t)cmd->width * cmd->height * 4;
        else if (cmd->format == 100)
            expected = cmd->data_size;

        size_t decompressed_len = 0;
        uint8_t *decompressed = decompress_zlib(
            data, len, expected, &decompressed_len);
        free(data);

        if (decompressed == NULL)
            return NULL;

        data = decompressed;
        len = decompressed_len;
    }

    *out_len = len;
    return data;
}

static struct image *
decode_png(struct terminal *term, const uint8_t *data, size_t len,
           char *err, size_t err_len)
{
#if defined(HAVE_LIBPNG)
    png_image png;
    memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_memory(&png, data, len)) {
        xsnprintf(err, err_len, "EINVAL:Failed to read PNG image");
        return NULL;
    }

    png.format = PNG_FORMAT_RGBA;

    if (png.width == 0 || png.height == 0 ||
        png.width > term->kitty_graphics.max_width ||
        png.height > term->kitty_graphics.max_height)
    {
        png_image_free(&png);
        xsnprintf(err, err_len, "EINVAL:Image too large");
        return NULL;
    }

    size_t pixels = (size_t)png.width * png.height;
    uint8_t *rgba = malloc(PNG_IMAGE_SIZE(png));
    if (rgba == NULL) {
        png_image_free(&png);
        xsnprintf(err, err_len, "ENOMEM");
        return NULL;
    }

    if (!png_image_finish_read(&png, NULL, rgba, 0, NULL)) {
        free(rgba);
        png_image_free(&png);
        xsnprintf(err, err_len, "EINVAL:Failed to decode PNG image");
        return NULL;
    }

    uint32_t *argb = malloc(pixels * sizeof(uint32_t));
    if (argb == NULL) {
        free(rgba);
        png_image_free(&png);
        xsnprintf(err, err_len, "ENOMEM");
        return NULL;
    }

    bool opaque = true;
    for (size_t i = 0; i < pixels; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];

        if (a != 0xff)
            opaque = false;

        argb[i] = (uint32_t)a << 24 | r << 16 | g << 8 | b;
    }

    free(rgba);

    const uint32_t width = png.width;
    const uint32_t height = png.height;
    png_image_free(&png);

    struct image *img = xmalloc(sizeof(*img));
    *img = (struct image){
        .width = width,
        .height = height,
        .data = argb,
        .opaque = opaque,
        .bytes = pixels * sizeof(uint32_t),
    };

    img->pix = pixman_image_create_bits_no_clear(
        PIXMAN_a8r8g8b8, img->width, img->height,
        img->data, img->width * sizeof(uint32_t));

    if (img->pix == NULL) {
        free(argb);
        free(img);
        xsnprintf(err, err_len, "ENOMEM");
        return NULL;
    }

    return img;
#else
    xsnprintf(err, err_len, "ENOTSUP:PNG images not supported");
    return NULL;
#endif
}

static struct image *
decode_image(struct terminal *term, const struct kitty_cmd *cmd,
             uint8_t *data, size_t len, char *err, size_t err_len)
{
    if (cmd->format == 100)
        return decode_png(term, data, len, err, err_len);

    if (cmd->format != 24 && cmd->format != 32) {
        xsnprintf(err, err_len, "EINVAL:Unsupported image format");
        return NULL;
    }

    if (cmd->width <= 0 || cmd->height <= 0 ||
        (unsigned)cmd->width > term->kitty_graphics.max_width ||
        (unsigned)cmd->height > term->kitty_graphics.max_height)
    {
        xsnprintf(err, err_len, "EINVAL:Invalid image dimensions");
        return NULL;
    }

    const size_t bpp = cmd->format == 24 ? 3 : 4;
    const size_t pixels = (size_t)cmd->width * cmd->height;

    if (len < pixels * bpp) {
        xsnprintf(err, err_len, "EINVAL:Not enough image data");
        return NULL;
    }

    uint32_t *argb = malloc(pixels * sizeof(uint32_t));
    if (argb == NULL) {
        xsnprintf(err, err_len, "ENOMEM");
        return NULL;
    }

    bool opaque = true;

    for (size_t i = 0; i < pixels; i++) {
        uint8_t r = data[i * bpp + 0];
        uint8_t g = data[i * bpp + 1];
        uint8_t b = data[i * bpp + 2];
        uint8_t a = bpp == 4 ? data[i * bpp + 3] : 0xff;

        if (a != 0xff)
            opaque = false;

        argb[i] = (uint32_t)a << 24 | r << 16 | g << 8 | b;
    }

    struct image *img = xmalloc(sizeof(*img));
    *img = (struct image){
        .width = cmd->width,
        .height = cmd->height,
        .data = argb,
        .opaque = opaque,
        .bytes = pixels * sizeof(uint32_t),
    };

    img->pix = pixman_image_create_bits_no_clear(
        PIXMAN_a8r8g8b8, img->width, img->height,
        img->data, img->width * sizeof(uint32_t));

    if (img->pix == NULL) {
        free(argb);
        free(img);
        xsnprintf(err, err_len, "ENOMEM");
        return NULL;
    }

    return img;
}

static bool
make_placement(struct terminal *term, struct image *master,
               const struct kitty_cmd *cmd, struct image_placement *out,
               char *err, size_t err_len)
{
    int sx = max(0, cmd->src_x);
    int sy = max(0, cmd->src_y);

    if (sx >= master->width || sy >= master->height) {
        xsnprintf(err, err_len, "EINVAL:Invalid source rectangle");
        return false;
    }

    int sw = cmd->src_w > 0 ? cmd->src_w : master->width - sx;
    int sh = cmd->src_h > 0 ? cmd->src_h : master->height - sy;
    sw = min(sw, master->width - sx);
    sh = min(sh, master->height - sy);

    if (sw <= 0 || sh <= 0) {
        xsnprintf(err, err_len, "EINVAL:Invalid source rectangle");
        return false;
    }

    const int cell_w = term->cell_width;
    const int cell_h = term->cell_height;

    int cols = cmd->cols;
    int rows = cmd->rows;
    int draw_w, draw_h;
    int box_off_x = 0, box_off_y = 0;

    if (cols > 0 && rows > 0) {
        const int box_w = cols * cell_w;
        const int box_h = rows * cell_h;
        const double scale = min(
            (double)box_w / sw, (double)box_h / sh);

        draw_w = max(1, (int)(sw * scale + 0.5));
        draw_h = max(1, (int)(sh * scale + 0.5));

        /* Letterbox/pillarbox */
        box_off_x = (box_w - draw_w) / 2;
        box_off_y = (box_h - draw_h) / 2;
    } else if (cols > 0) {
        draw_w = cols * cell_w;
        draw_h = max(1, (int)((double)sh * draw_w / sw + 0.5));
        rows = max(1, (draw_h + cell_h - 1) / cell_h);
    } else if (rows > 0) {
        draw_h = rows * cell_h;
        draw_w = max(1, (int)((double)sw * draw_h / sh + 0.5));
        cols = max(1, (draw_w + cell_w - 1) / cell_w);
    } else {
        draw_w = sw;
        draw_h = sh;
        cols = max(1, (cmd->off_x + draw_w + cell_w - 1) / cell_w);
        rows = max(1, (cmd->off_y + draw_h + cell_h - 1) / cell_h);
    }

    if ((unsigned)draw_w > term->kitty_graphics.max_width ||
        (unsigned)draw_h > term->kitty_graphics.max_height)
    {
        xsnprintf(err, err_len, "EINVAL:Image too large");
        return false;
    }

    /* Crop, if needed */
    pixman_image_t *src = master->pix;
    uint32_t *crop_data = NULL;
    pixman_image_t *crop_pix = NULL;

    if (sx != 0 || sy != 0 || sw != master->width || sh != master->height) {
        crop_data = malloc((size_t)sw * sh * sizeof(uint32_t));
        if (crop_data == NULL) {
            xsnprintf(err, err_len, "ENOMEM");
            return false;
        }

        crop_pix = pixman_image_create_bits_no_clear(
            PIXMAN_a8r8g8b8, sw, sh, crop_data, sw * sizeof(uint32_t));

        if (crop_pix == NULL) {
            free(crop_data);
            xsnprintf(err, err_len, "ENOMEM");
            return false;
        }

        pixman_image_composite32(
            PIXMAN_OP_SRC, master->pix, NULL, crop_pix,
            sx, sy, 0, 0, 0, 0, sw, sh);

        src = crop_pix;
    }

    uint32_t *dst_data = malloc((size_t)draw_w * draw_h * sizeof(uint32_t));
    if (dst_data == NULL) {
        if (crop_pix != NULL)
            pixman_image_unref(crop_pix);
        free(crop_data);
        xsnprintf(err, err_len, "ENOMEM");
        return false;
    }

    pixman_image_t *dst_pix = pixman_image_create_bits_no_clear(
        PIXMAN_a8r8g8b8, draw_w, draw_h, dst_data,
        draw_w * sizeof(uint32_t));

    if (dst_pix == NULL) {
        free(dst_data);
        if (crop_pix != NULL)
            pixman_image_unref(crop_pix);
        free(crop_data);
        xsnprintf(err, err_len, "ENOMEM");
        return false;
    }

    if (draw_w == sw && draw_h == sh) {
        pixman_image_composite32(
            PIXMAN_OP_SRC, src, NULL, dst_pix,
            0, 0, 0, 0, 0, 0, draw_w, draw_h);
    } else {
        struct pixman_f_transform scale;
        pixman_f_transform_init_scale(
            &scale, (double)sw / draw_w, (double)sh / draw_h);

        struct pixman_transform _scale;
        pixman_transform_from_pixman_f_transform(&_scale, &scale);
        pixman_image_set_transform(src, &_scale);
        pixman_image_set_filter(src, PIXMAN_FILTER_BILINEAR, NULL, 0);

        pixman_image_composite32(
            PIXMAN_OP_SRC, src, NULL, dst_pix,
            0, 0, 0, 0, 0, 0, draw_w, draw_h);

        pixman_image_set_transform(src, NULL);
    }

    if (crop_pix != NULL)
        pixman_image_unref(crop_pix);
    free(crop_data);

    const int abs_row =
        (term->grid->offset + term->grid->cursor.point.row) &
        (term->grid->num_rows - 1);

    rows = min(rows, term->grid->num_rows - abs_row);
    if (rows <= 0) {
        pixman_image_unref(dst_pix);
        free(dst_data);
        xsnprintf(err, err_len, "EINVAL:Image does not fit in grid");
        return false;
    }

    *out = (struct image_placement){
        .pix = NULL,
        .width = -1,
        .height = -1,
        .rows = rows,
        .cols = cols,
        .pos = {
            .col = term->grid->cursor.point.col,
            .row = abs_row,
        },
        .opaque = master->opaque,
        .offset_x = cmd->off_x + box_off_x,
        .offset_y = cmd->off_y + box_off_y,
        .image_id = master->id,
        .image_number = master->number,
        .placement_id = cmd->placement_id,
        .z = cmd->z,
        .cell_width = cell_w,
        .cell_height = cell_h,
        .original = {
            .data = dst_data,
            .pix = dst_pix,
            .width = draw_w,
            .height = draw_h,
        },
        .scaled = {
            .data = NULL,
            .pix = NULL,
            .width = -1,
            .height = -1,
        },
    };

    /* Replace an existing placement with the same image/placement id */
    if (cmd->placement_id != 0 && master->id != 0)
        remove_placements_by_id(term, master->id, cmd->placement_id);

    image_placement_insert(term, *out);

    for (int r = 0; r < rows; r++) {
        int abs = (abs_row + r) & (term->grid->num_rows - 1);
        struct row *row = term->grid->rows[abs];
        if (row == NULL)
            continue;

        row->dirty = true;
        for (int c = out->pos.col;
             c < min(out->pos.col + out->cols, term->cols); c++)
        {
            row->cells[c].attrs.clean = 0;
        }
    }

    if (cmd->cursor_policy == 0) {
        int col = term->grid->cursor.point.col + cols;
        int extra_rows = 0;

        if (col >= term->cols) {
            col = 0;
            extra_rows = 1;
        }

        int target_row = term->grid->cursor.point.row + rows + extra_rows;

        if (target_row >= term->rows) {
            int scroll = target_row - (term->rows - 1);
            for (int i = 0; i < scroll; i++)
                term_linefeed(term);
            target_row = term->rows - 1;
        }

        term_cursor_to(
            term, max(0, target_row), min(col, term->cols - 1));
    }

    return true;
}

static void
handle_transmit(struct terminal *term, const struct kitty_cmd *cmd,
                const char *payload, size_t payload_len, bool display)
{
    char err[128] = "";
    size_t data_len = 0;

    uint8_t *data = decode_transport(
        term, cmd, payload, payload_len, &data_len);

    if (data == NULL) {
        error_response(term, cmd, "EINVAL:Failed to read image data");
        return;
    }

    struct image *img = decode_image(term, cmd, data, data_len, err, sizeof(err));
    free(data);

    if (img == NULL) {
        error_response(term, cmd, "%s", err);
        return;
    }

    const bool should_reply = cmd->id != 0 || cmd->number != 0;

    if (cmd->action == 'q') {
        free_image(img);
        free(img);
        if (should_reply)
            respond(term, cmd, cmd->id, cmd->number, "OK");
        return;
    }

    uint32_t id = cmd->id != 0 ? cmd->id : new_image_id(term);
    img->id = id;
    img->number = cmd->number;

    /* Re-transmitting an existing id replaces it and all its placements */
    remove_placements_by_id(term, id, 0);
    remove_image_by_id(term, id);

    store_image(term, img);
    free(img);

    if (display) {
        struct image_placement placement;
        if (!make_placement(
                term, find_image_by_id(term, id), cmd,
                &placement, err, sizeof(err)))
        {
            error_response(term, cmd, "%s", err);
            return;
        }
    }

    if (should_reply)
        respond(term, cmd, id, cmd->number, "OK");
}

static void
handle_put(struct terminal *term, const struct kitty_cmd *cmd)
{
    struct image *img = NULL;

    if (cmd->id != 0)
        img = find_image_by_id(term, cmd->id);
    else if (cmd->number != 0)
        img = find_image_by_number(term, cmd->number);

    if (img == NULL) {
        error_response(term, cmd, "ENOENT:Image not found");
        return;
    }

    char err[128] = "";
    struct image_placement placement;

    if (!make_placement(term, img, cmd, &placement, err, sizeof(err))) {
        error_response(term, cmd, "%s", err);
        return;
    }

    if (cmd->id != 0 || cmd->number != 0)
        respond(term, cmd, img->id, img->number, "OK");
}

struct delete_ctx {
    char mode;
    uint32_t id;
    uint32_t number;
    uint32_t placement_id;
    int x, y, z;
};

static int
placement_screen_start(
    const struct terminal *term, const struct grid *grid,
    const struct image_placement *img)
{
    const int sb_view = grid_row_abs_to_sb(grid, term->rows, grid->view);
    const int sb_img = grid_row_abs_to_sb(grid, term->rows, img->pos.row);
    return sb_img - sb_view;
}

static bool
placement_matches_delete(
    const struct terminal *term, const struct grid *grid,
    const struct image_placement *img, const struct delete_ctx *ctx)
{
    switch (ctx->mode) {
    case 'a':
        return true;

    case 'i':
        if (img->image_id != ctx->id)
            return false;
        return ctx->placement_id == 0 ||
               img->placement_id == ctx->placement_id;

    case 'n':
        if (img->image_number != ctx->number)
            return false;
        return ctx->placement_id == 0 ||
               img->placement_id == ctx->placement_id;

    case 'r':
        return img->image_id >= (uint32_t)ctx->x &&
               img->image_id <= (uint32_t)ctx->y;

    case 'c': {
        int row = grid->cursor.point.row;
        int col = grid->cursor.point.col;
        return image_placement_intersects(
            term, grid, img, row, col, row, col);
    }

    case 'p': {
        int row = ctx->y - 1;
        int col = ctx->x - 1;
        return image_placement_intersects(
            term, grid, img, row, col, row, col);
    }

    case 'q': {
        int row = ctx->y - 1;
        int col = ctx->x - 1;
        return img->z == ctx->z &&
               image_placement_intersects(
                   term, grid, img, row, col, row, col);
    }

    case 'x': {
        int col = ctx->x - 1;
        return img->pos.col <= col &&
               col <= img->pos.col + img->cols - 1;
    }

    case 'y': {
        int row = ctx->y - 1;
        int start = placement_screen_start(term, grid, img);
        return start <= row && row <= start + img->rows - 1;
    }

    case 'z':
        return img->z == ctx->z;

    default:
        return false;
    }
}

static void
handle_delete(struct terminal *term, const struct kitty_cmd *cmd)
{
    /* Abort any in-progress upload */
    term->kitty_graphics.upload.active = false;

    char mode = cmd->delete;
    bool free_data = isupper((unsigned char)mode);
    mode = tolower((unsigned char)mode);

    struct delete_ctx ctx = {
        .mode = mode,
        .id = cmd->id,
        .number = cmd->number,
        .placement_id = cmd->placement_id,
        .x = cmd->src_x,
        .y = cmd->src_y,
        .z = cmd->z,
    };

    if (mode == 'a') {
        /* All placements visible on the current screen */
        struct grid *grid = term->grid;
        tll_foreach(grid->image_placements, it) {
            image_placement_destroy(&it->item);
            tll_remove(grid->image_placements, it);
        }
    } else {
        bool global =
            mode == 'i' || mode == 'n' || mode == 'r';

        for (size_t i = 0; i < (global ? 2u : 1u); i++) {
            struct grid *grid = global
                ? (i == 0 ? &term->normal : &term->alt)
                : term->grid;

            tll_foreach(grid->image_placements, it) {
                if (!placement_matches_delete(
                        term, grid, &it->item, &ctx))
                    continue;

                image_placement_destroy(&it->item);
                tll_remove(grid->image_placements, it);
            }
        }
    }

    if (free_data)
        gc_images(term);
}

static bool
upload_append(struct kitty_graphics *kg, const char *data, size_t len)
{
    struct kitty_upload *up = &kg->upload;

    if (len > KITTY_MAX_DATA || up->size > KITTY_MAX_DATA - len)
        return false;

    size_t needed = up->size + len + 1;
    if (needed > up->alloc) {
        size_t new_alloc = max(needed, up->alloc * 2);
        uint8_t *new_data = realloc(up->data, new_alloc);
        if (new_data == NULL)
            return false;
        up->data = new_data;
        up->alloc = new_alloc;
    }

    memcpy(up->data + up->size, data, len);
    up->size += len;
    up->data[up->size] = '\0';
    return true;
}

static void
upload_reset(struct kitty_graphics *kg)
{
    kg->upload.active = false;
    kg->upload.size = 0;
    if (kg->upload.data != NULL)
        kg->upload.data[0] = '\0';
}

static bool
upload_append_chunk(struct kitty_graphics *kg,
                    const char *payload, size_t len)
{
    if (!kg->upload.binary)
        return upload_append(kg, payload, len);

    char *b64 = xstrndup(payload, len);
    size_t decoded_len = 0;
    char *decoded = base64_decode(b64, &decoded_len);
    free(b64);

    if (decoded == NULL)
        return false;

    bool ok = upload_append(kg, decoded, decoded_len);
    free(decoded);
    return ok;
}

void
kitty_dispatch(struct terminal *term)
{
    if (!term->conf->tweak.kitty_graphics)
        return;

    if (term->apc.idx == 0 || term->apc.data == NULL)
        return;

    if (term->apc.data[0] != 'G')
        return;

    char *str = (char *)term->apc.data + 1;
    size_t str_len = term->apc.idx - 1;

    const char *semicolon = memchr(str, ';', str_len);
    const char *control = str;
    size_t control_len = semicolon != NULL
        ? (size_t)(semicolon - str) : str_len;
    const char *payload = semicolon != NULL
        ? semicolon + 1 : str + str_len;
    size_t payload_len = semicolon != NULL
        ? str_len - control_len - 1 : 0;

    struct kitty_graphics *kg = &term->kitty_graphics;
    struct kitty_cmd cmd;
    parse_control(control, control_len, &cmd);

    if (kg->upload.active) {
        if (!upload_append_chunk(kg, payload, payload_len)) {
            upload_reset(kg);
            error_response(term, &cmd, "ENOMEM:Upload too large");
            return;
        }

        if (cmd.more)
            return;

        const bool binary = kg->upload.binary;
        cmd = kg->upload.cmd;
        if (binary)
            cmd.medium = 'b';
        payload = (const char *)kg->upload.data;
        payload_len = kg->upload.size;
        kg->upload.active = false;
    } else if (cmd.more) {
        kg->upload.cmd = cmd;
        kg->upload.size = 0;
        kg->upload.binary = cmd.medium == 'd';
        kg->upload.active = true;

        if (!upload_append_chunk(kg, payload, payload_len)) {
            upload_reset(kg);
            error_response(term, &cmd, "ENOMEM:Upload too large");
        }
        return;
    }

    switch (cmd.action) {
    case 't':
    case 'T':
    case 'q':
        handle_transmit(
            term, &cmd, payload, payload_len, cmd.action == 'T');
        break;

    case 'p':
        handle_put(term, &cmd);
        break;

    case 'd':
        handle_delete(term, &cmd);
        break;

    default:
        error_response(term, &cmd, "EINVAL:Unsupported action");
        break;
    }
}

void
kitty_graphics_clear_all(struct terminal *term)
{
    image_placement_destroy_all(&term->normal);
    image_placement_destroy_all(&term->alt);

    tll_foreach(term->kitty_graphics.images, it) {
        free_image(&it->item);
        tll_remove(term->kitty_graphics.images, it);
    }

    term->kitty_graphics.total_bytes = 0;
}

void
kitty_graphics_clear_alt(struct terminal *term)
{
    image_placement_destroy_all(&term->alt);
}

void
kitty_graphics_fini(struct terminal *term)
{
    kitty_graphics_clear_all(term);

    free(term->kitty_graphics.upload.data);
    term->kitty_graphics.upload.data = NULL;
    term->kitty_graphics.upload.size = 0;
    term->kitty_graphics.upload.alloc = 0;
    term->kitty_graphics.upload.active = false;
}
