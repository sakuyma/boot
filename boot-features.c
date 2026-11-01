#include "boot-features.h"
#include "version.h"

const char version_and_features[] =
    "version: " BOOT_VERSION

#if defined(BOOT_PGO_ENABLED) && BOOT_PGO_ENABLED
    " +pgo"
#else
    " -pgo"
#endif

#if defined(BOOT_IME_ENABLED) && BOOT_IME_ENABLED
    " +ime"
#else
    " -ime"
#endif

#if defined(BOOT_GRAPHEME_CLUSTERING) && BOOT_GRAPHEME_CLUSTERING
    " +graphemes"
#else
    " -graphemes"
#endif

#if defined(HAVE_XDG_TOPLEVEL_TAG)
    " +toplevel-tag"
#else
    " -toplevel-tag"
#endif

#if defined(HAVE_EXT_BACKGROUND_EFFECT)
    " +blur"
#else
    " -blur"
#endif

#if !defined(NDEBUG)
    " +assertions"
#else
    " -assertions"
#endif
;
