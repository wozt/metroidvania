/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow BG1 collision primitives. */
#include "aos_collision.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    static uint8_t cells[64 * 64];
    AosCollision layer = {2, 2, 64, 64, cells, NULL, false};
    /* A solid floor row at cells y=10 (pixels 80..87), solid below. */
    for (int x = 0; x < 64; ++x) {
        cells[10 * 64 + x] = 0x03;
        cells[11 * 64 + x] = 0x03;
    }
    assert(aos_collision_cell(&layer, 20, 84) == 0x03);
    assert(aos_collision_cell(&layer, 20, 70) == 0x00);
    /* Out-of-room coordinates clamp to the edge cells. */
    assert(aos_collision_cell(&layer, -40, 84) == 0x03);
    /* One-screen dimensions clamp to 30 x 26 cells, not 32 x 32. */
    AosCollision small = {1, 1, 64, 64, cells, NULL, false};
    cells[25 * 64 + 29] = 0x01;
    assert(aos_collision_cell(&small, 31 * 8, 31 * 8) == 0x01);
    cells[25 * 64 + 29] = 0x00;

    /* Feet 3 pixels inside the floor walk up to its surface. */
    assert(aos_floor_depth(&layer, 20, 83, 0, false) == -4);
    assert(aos_floor_depth(&layer, 20, 79, 0, false) == 0);
    /* Two solid cells: the walk stops after at most 8 pixels per call. */
    assert(aos_floor_depth(&layer, 20, 90, 0, false) == -11);
    /* A head 2 pixels into a ceiling walks down out of it. */
    assert(aos_ceiling_depth(&layer, 20, 86, 0, false) == 2 + 8);

    /* Floor slope 0x41: 45 degrees, rising to the left (height grows with x). */
    assert(aos_slope_height(0x41, 0) == 0 && aos_slope_height(0x41, 7) == 7);
    assert(aos_slope_height(0x45, 0) == 7);                 /* bit 2 mirrors */
    assert(aos_slope_height(0x81, 7) == 3);                 /* half step */
    assert(aos_slope_height(0xA1, 7) == 4 + 3);             /* second half */
    assert(aos_slope_height(0xC1, 7) == 1);                 /* quarter step */
    cells[5 * 64 + 2] = 0x41;
    /* At x=16+4 the first solid row is 4 pixels below the cell top (y=44). */
    assert(aos_floor_depth(&layer, 20, 47, 0, false) == -4);
    assert(aos_floor_depth(&layer, 20, 44, 0, false) == -1);
    assert(aos_floor_depth(&layer, 20, 43, 0, false) == 0);
    /* Platforms (bit 0 only) never stop horizontal movement. */
    cells[5 * 64 + 2] = 0x01;
    assert(aos_wall_push_left(&layer, 20, 44) == 0);
    cells[5 * 64 + 2] = 0x03;
    assert(aos_wall_push_left(&layer, 20, 44) == -5);
    assert(aos_wall_push_right(&layer, 20, 44) == 4);
    cells[5 * 64 + 2] = 0xFF;
    assert(aos_wall_push_left(&layer, 20, 44) == -5);
    assert(aos_wall_push_right(&layer, 20, 44) == 12);

    /* sub_08001BA0 walk modes and the bit-3 test of sub_08001F3C. */
    assert(aos_collision_mode(0x08, 1) == 0x01);
    assert(aos_collision_mode(0x09, 1) == 0x09);
    assert(aos_collision_mode(0x09, 2) == 0x03);
    assert(aos_collision_mode(0x03, 2) == 0x00);
    assert(aos_collision_mode(0x03, 0) == 0x03);
    cells[6 * 64 + 2] = 0x08;
    assert(aos_collision_special(&layer, 20, 50));
    cells[6 * 64 + 2] = 0x03;
    assert(!aos_collision_special(&layer, 20, 50));
    puts("Aria collision tests passed");
    return 0;
}
