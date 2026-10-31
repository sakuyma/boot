#include "image.h"

#include <limits.h>
#include <string.h>

#define LOG_MODULE "image"
#define LOG_ENABLE_DBG 0
#include "log.h"
#include "debug.h"
#include "grid.h"
#include "render.h"
#include "util.h"
#include "xmalloc.h"

static void
image_placement_invalidate_cache(struct image_placement *img)
{
    if (img->scaled.pix != NULL)
        pixman_image_unref(img->scaled.pix);

    free(img->scaled.data);

    img->pix = NULL;
    img->width = -1;
    img->height = -1;
    img->scaled.data = NULL;
    img->scaled.pix = NULL;
    img->scaled.width = -1;
    img->scaled.height = -1;
}

void
image_placement_destroy(struct image_placement *img)
{
    image_placement_invalidate_cache(img);

    if (img->original.pix != NULL)
        pixman_image_unref(img->original.pix);

    free(img->original.data);
    img->original.pix = NULL;
    img->original.data = NULL;
}

void
image_placement_destroy_all(struct grid *grid)
{
    tll_foreach(grid->image_placements, it) {
        image_placement_destroy(&it->item);
        tll_remove(grid->image_placements, it);
    }
}

void
image_placement_clear(struct grid *grid)
{
    image_placement_destroy_all(grid);
}

static void
image_placement_erase(struct terminal *term, struct image_placement *img)
{
    for (int i = 0; i < img->rows; i++) {
        int r = (img->pos.row + i) & (term->grid->num_rows - 1);

        struct row *row = term->grid->rows[r];
        if (row == NULL)
            continue;

        row->dirty = true;

        for (int c = img->pos.col;
             c < min(img->pos.col + img->cols, term->cols); c++)
        {
            row->cells[c].attrs.clean = 0;
        }
    }

    image_placement_destroy(img);
}

void
image_placement_insert(struct terminal *term, struct image_placement img)
{
    int end_row = grid_row_abs_to_sb(
        term->grid, term->rows, img.pos.row + img.rows - 1);

    tll_foreach(term->grid->image_placements, it) {
        int rebased = grid_row_abs_to_sb(
            term->grid, term->rows, it->item.pos.row + it->item.rows - 1);

        if (rebased < end_row) {
            tll_insert_before(term->grid->image_placements, it, img);
            return;
        }
    }

    tll_push_back(term->grid->image_placements, img);
}

void
image_placement_scroll_up(struct terminal *term, int rows)
{
    if (likely(tll_length(term->grid->image_placements) == 0))
        return;

    tll_rforeach(term->grid->image_placements, it) {
        struct image_placement *img = &it->item;

        int start = grid_row_abs_to_sb(
            term->grid, term->rows, img->pos.row);

        if (start < rows) {
            image_placement_erase(term, img);
            tll_remove(term->grid->image_placements, it);
        }
    }
}

void
image_placement_scroll_down(struct terminal *term, int rows)
{
    if (likely(tll_length(term->grid->image_placements) == 0))
        return;

    xassert(term->grid->num_rows >= rows);

    tll_foreach(term->grid->image_placements, it) {
        struct image_placement *img = &it->item;

        int end = grid_row_abs_to_sb(
            term->grid, term->rows, img->pos.row + img->rows - 1);
        if (end >= term->grid->num_rows - rows) {
            image_placement_erase(term, img);
            tll_remove(term->grid->image_placements, it);
        } else
            break;
    }
}

void
image_placement_cell_size_changed(struct terminal *term)
{
    tll_foreach(term->normal.image_placements, it)
        image_placement_invalidate_cache(&it->item);

    tll_foreach(term->alt.image_placements, it)
        image_placement_invalidate_cache(&it->item);
}

void
image_placement_sync_cache(
    const struct terminal *term, struct image_placement *img)
{
    if (img->pix != NULL)
        return;

    /* Cache should be invalid */
    xassert(img->scaled.data == NULL);
    xassert(img->scaled.pix == NULL);
    xassert(img->scaled.width < 0);
    xassert(img->scaled.height < 0);

    if (img->cell_width == term->cell_width &&
        img->cell_height == term->cell_height)
    {
        img->pix = img->original.pix;
        img->width = img->original.width;
        img->height = img->original.height;
    } else {
        const double width_ratio = (double)term->cell_width / img->cell_width;
        const double height_ratio = (double)term->cell_height / img->cell_height;

        struct pixman_f_transform scale;
        pixman_f_transform_init_scale(
            &scale, 1. / width_ratio, 1. / height_ratio);

        struct pixman_transform _scale;
        pixman_transform_from_pixman_f_transform(&_scale, &scale);
        pixman_image_set_transform(img->original.pix, &_scale);
        pixman_image_set_filter(
            img->original.pix, PIXMAN_FILTER_BILINEAR, NULL, 0);

        int scaled_width = (double)img->original.width * width_ratio;
        int scaled_height = (double)img->original.height * height_ratio;
        int scaled_stride = scaled_width * sizeof(uint32_t);

        uint8_t *scaled_data = xmalloc(scaled_height * scaled_stride);
        pixman_image_t *scaled_pix = pixman_image_create_bits_no_clear(
            PIXMAN_a8r8g8b8, scaled_width, scaled_height,
            (uint32_t *)scaled_data, scaled_stride);

        pixman_image_composite32(
            PIXMAN_OP_SRC, img->original.pix, NULL, scaled_pix, 0, 0, 0, 0,
            0, 0, scaled_width, scaled_height);

        pixman_image_set_transform(img->original.pix, NULL);

        img->scaled.data = scaled_data;
        img->scaled.pix = img->pix = scaled_pix;
        img->scaled.width = img->width = scaled_width;
        img->scaled.height = img->height = scaled_height;
    }
}

void
image_placement_reflow_grid(struct terminal *term, struct grid *grid)
{
    struct grid *active_grid = term->grid;
    term->grid = grid;

    tll(struct image_placement) copy = tll_init();
    tll_foreach(grid->image_placements, it)
        tll_push_back(copy, it->item);
    tll_free(grid->image_placements);

    tll_rforeach(copy, it) {
        struct image_placement *img = &it->item;
        int start = img->pos.row;
        int end = (start + img->rows - 1) & (grid->num_rows - 1);

        if (end < start || img->rows > grid->num_rows) {
            /* Image crosses the scrollback wrap-around, or is too
             * large for the new grid */
            image_placement_destroy(img);
            continue;
        }

        bool destroyed = false;
        int last_row = -1;

        for (int j = 0; j < img->rows; j++) {
            int row_no = grid_row_abs_to_sb(
                term->grid, term->rows, img->pos.row + j);
            if (last_row != -1 && last_row >= row_no) {
                image_placement_destroy(img);
                destroyed = true;
                break;
            }
            last_row = row_no;
        }

        if (destroyed)
            continue;

        image_placement_insert(term, *img);
    }

    tll_free(copy);
    term->grid = active_grid;
}

void
image_placement_reflow(struct terminal *term)
{
    for (size_t i = 0; i < 2; i++) {
        struct grid *grid = i == 0 ? &term->normal : &term->alt;
        image_placement_reflow_grid(term, grid);
    }
}

bool
image_placement_intersects(
    const struct terminal *term, const struct grid *grid,
    const struct image_placement *img,
    int start_row, int start_col, int end_row, int end_col)
{
    const int sb_view = grid_row_abs_to_sb(
        grid, term->rows, grid->view);
    const int sb_start = grid_row_abs_to_sb(
        grid, term->rows, img->pos.row);

    const int rel_start = sb_start - sb_view;
    const int rel_end = rel_start + img->rows - 1;

    if (rel_end < start_row || rel_start > end_row)
        return false;

    if (img->pos.col + img->cols - 1 < start_col ||
        img->pos.col > end_col)
        return false;

    return true;
}

void
image_placement_erase_screen_region(
    struct terminal *term, struct grid *grid,
    int start_row, int start_col, int end_row, int end_col)
{
    tll_foreach(grid->image_placements, it) {
        if (image_placement_intersects(
                term, grid, &it->item,
                start_row, start_col, end_row, end_col))
        {
            struct image_placement *img = &it->item;
            struct grid *saved = term->grid;
            term->grid = grid;
            image_placement_erase(term, img);
            term->grid = saved;
            tll_remove(grid->image_placements, it);
        }
    }

    for (int r = start_row; r <= end_row; r++) {
        if (r < 0 || r >= term->rows)
            continue;

        struct row *row = grid_row_in_view(grid, r);
        row->dirty = true;

        int c_start = r == start_row ? start_col : 0;
        int c_end = r == end_row ? end_col : term->cols - 1;
        for (int c = c_start; c <= c_end && c < term->cols; c++)
            row->cells[c].attrs.clean = 0;
    }
}
