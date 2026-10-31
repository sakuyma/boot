#pragma once

#include "terminal.h"

void kitty_dispatch(struct terminal *term);

/* Free all kitty graphics state (images and placements) */
void kitty_graphics_fini(struct terminal *term);
void kitty_graphics_clear_all(struct terminal *term);
void kitty_graphics_clear_alt(struct terminal *term);
