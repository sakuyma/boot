#pragma once

#include "terminal.h"

/* Placement lifetime management. The actual kitty graphics protocol
 * parser lives in kitty.c */

void image_placement_destroy(struct image_placement *img);
void image_placement_destroy_all(struct grid *grid);
void image_placement_clear(struct grid *grid);

void image_placement_scroll_up(struct terminal *term, int rows);
void image_placement_scroll_down(struct terminal *term, int rows);

void image_placement_cell_size_changed(struct terminal *term);
void image_placement_sync_cache(
    const struct terminal *term, struct image_placement *img);

void image_placement_reflow_grid(struct terminal *term, struct grid *grid);
void image_placement_reflow(struct terminal *term);

/* Insert a placement into the current grid */
void image_placement_insert(
    struct terminal *term, struct image_placement img);

/* Remove all placements intersecting the given (screen relative)
 * rectangle. Used by ED (erase in display). */
void image_placement_erase_screen_region(
    struct terminal *term, struct grid *grid,
    int start_row, int start_col, int end_row, int end_col);

bool image_placement_intersects(
    const struct terminal *term, const struct grid *grid,
    const struct image_placement *img,
    int start_row, int start_col, int end_row, int end_col);
