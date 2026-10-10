/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow BG1 collision primitives.
 *
 * Ports of the cvaos routines in src/code_08001194.c: the cell lookup
 * sub_08001A00 (on a table exported by scripts/aos_runtime_room.py, with the
 * slope X-flip toggle already applied), the slope height sub_08001B40, the
 * mode rewrite sub_08001BA0, the vertical walks sub_08001C1C, sub_08001CCC,
 * sub_08001D94 and sub_08001E58, the slope/bit-3 test sub_08001F3C and the
 * horizontal pushes sub_080022A8 and sub_080022E8, and the solid-block
 * override of 16x16 blocks (gEwramData + 0xF0C0 with unk_A074_6, written by
 * sub_08002200 / sub_08002248). Coordinates are room pixels. No SDL. */
#ifndef AOS_COLLISION_H
#define AOS_COLLISION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AOS_CELL_SIZE 8

typedef struct {
    int width_screens, height_screens;
    int width_cells, height_cells;
    const uint8_t *cells;      /* width_cells * height_cells bytes */
    uint8_t *blocks;           /* optional: one byte per 16x16 block */
    bool blocks_active;        /* unk_A074_6 */
} AosCollision;

/* Bytes needed for the block override of a layer. */
size_t aos_collision_block_bytes(const AosCollision *layer);
/* sub_08002200 (solid) and sub_08002248 (clear) at room pixel (x, y). */
void aos_collision_set_block(AosCollision *layer, int32_t x, int32_t y, bool solid);

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
