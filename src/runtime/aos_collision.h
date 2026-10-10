/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow BG1 collision primitives.
 *
 * Ports of the cvaos routines in src/code_08001194.c: the cell lookup
 * sub_08001A00 (on a table exported by scripts/aos_runtime_room.py, with the
 * slope X-flip toggle already applied), the slope height sub_08001B40, the
 * mode rewrite sub_08001BA0, the vertical walks sub_08001C1C, sub_08001CCC,
 * sub_08001D94 and sub_08001E58, the slope/bit-3 test sub_08001F3C and the
 * horizontal pushes sub_080022A8 and sub_080022E8. Coordinates are room
 * pixels. The breakable-block override (unk_A074_6 / unk_F0C0) is not
 * modelled. No SDL dependency. */
#ifndef AOS_COLLISION_H
#define AOS_COLLISION_H

#include <stdbool.h>
#include <stdint.h>

#define AOS_CELL_SIZE 8

typedef struct {
    int width_screens, height_screens;
    int width_cells, height_cells;
    const uint8_t *cells;      /* width_cells * height_cells bytes */
} AosCollision;

uint8_t aos_collision_cell(const AosCollision *layer, int32_t x, int32_t y);
int aos_slope_height(uint8_t value, int32_t x);
uint8_t aos_collision_mode(uint8_t value, int mode);
/* Distance out of a ceiling below (y) going down, at most 9 pixels. */
int aos_ceiling_depth(const AosCollision *layer, int32_t x, int32_t y, int mode,
                      bool use_mode);
/* Negative distance out of a floor going up, at most 8 pixels. */
int aos_floor_depth(const AosCollision *layer, int32_t x, int32_t y, int mode,
                    bool use_mode);
bool aos_collision_special(const AosCollision *layer, int32_t x, int32_t y);
/* Pushes out of a bit-1 wall cell: right edge (negative) and left edge. */
int aos_wall_push_left(const AosCollision *layer, int32_t x, int32_t y);
int aos_wall_push_right(const AosCollision *layer, int32_t x, int32_t y);

#endif /* AOS_COLLISION_H */
