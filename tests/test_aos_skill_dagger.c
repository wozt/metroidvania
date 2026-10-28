/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/runtime/aos_skill_dagger.h"

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

static void flight_tests(void) {
    memset(cells, 0, sizeof cells);
    AosDagger right = aos_dagger_create(0, 100 << 16, 80 << 16);
    AosDagger left = aos_dagger_create(1, 100 << 16, 80 << 16);
    assert(!right.facing_left && right.vx == 0x50000 && left.facing_left && left.vx == -0x50000);
    assert(aos_dagger_update(&right, &layer, 0, 0, 16, 16));
    assert(right.state == 1 && right.x == 105 << 16);
    /* Struck: state 4, deleted on the next update. */
    right.struck = true;
    assert(aos_dagger_update(&right, &layer, 0, 0, 16, 16) && right.state == 4);
    assert(!aos_dagger_update(&right, &layer, 0, 0, 16, 16));
    /* Outside the margins. */
    while (left.state == 1 || left.state == 0) aos_dagger_update(&left, &layer, 0, 0, 16, 16);
    assert(left.state == 4 && (left.x >> 16) < -16);
}

static void wall_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int y = 0; y < H; ++y) cells[y * W + 16] = 0x03;   /* x 128..135 */
    AosDagger d = aos_dagger_create(0, 100 << 16, 80 << 16);
    int frames = 0;
    while (d.state <= 1) {
        aos_dagger_update(&d, &layer, 0, 0, 16, 16);
        frames++;
    }
    /* 105, 110, 115, 120, 125: 125 + 6 = 131 is in the wall. */
    assert(d.state == 2 && frames == 5 && d.x >> 16 == 125);
    aos_dagger_update(&d, &layer, 0, 0, 16, 16);
    assert(d.state == 3 && d.blinking && d.timer == 15 && d.x >> 16 == 125);
    for (int i = 0; i < 15; ++i) aos_dagger_update(&d, &layer, 0, 0, 16, 16);
    assert(d.state == 3 && d.timer == 0);
    aos_dagger_update(&d, &layer, 0, 0, 16, 16);
    assert(d.state == 4 && !aos_dagger_update(&d, &layer, 0, 0, 16, 16));
}

static void trail_tests(void) {
    memset(cells, 0, sizeof cells);
    AosDagger d = aos_dagger_create(0, 100 << 16, 80 << 16);
    AosDaggerTrail trail = aos_dagger_trail_create(&d);
    assert(trail.history[0][0] == 0 && trail.history[9][0] == 100);
    for (int f = 1; f <= 4; ++f) {
        aos_dagger_update(&d, &layer, 0, 0, 16, 16);
        aos_dagger_trail_update(&trail, &d);
    }
    /* Entries 1-3, the images drawn, are one to three frames back; the
     * unwritten entry 0 of the fresh entity has shifted down as (0, 0). */
    assert(trail.history[0][0] == 120 && trail.history[1][0] == 115 &&
           trail.history[3][0] == 105 && trail.history[4][0] == 0 &&
           trail.history[5][0] == 100);
}

int main(void) {
    flight_tests();
    wall_tests();
    trail_tests();
    puts("aos skill dagger tests passed");
    return 0;
}
