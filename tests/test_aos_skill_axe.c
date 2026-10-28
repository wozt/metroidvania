/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_axe.h"
#include "../src/runtime/aos_audio.h"

static void axe_tests(void) {
    aos_audio_clear();
    /* Fractions are dropped. */
    AosAxe axe = aos_axe_create((100 << 16) + 0x8000, (200 << 16) + 0x4000, false);
    assert(aos_audio_count() == 1 && aos_audio_requested(AOS_AUDIO_START_SONG, 0xE3));
    assert(axe.x == 100 << 16 && axe.y == 174 << 16 && axe.vx == 0x34000 && axe.ax == -0x1000);
    assert(aos_axe_update(&axe, 0, 100, 16, 16));
    assert(axe.state == 1 && axe.x == (100 << 16) + 0x34000 && axe.vx == 0x33000);
    assert(axe.angle == 0 && axe.turns == 1);
    assert(aos_axe_update(&axe, 0, 100, 16, 16) && axe.angle == 0x800);
    /* It comes back, and the pull stops at 8 pixels a frame. */
    int frames = 2;
    while (axe.vx > -0x80000 && frames < 400) {
        aos_axe_update(&axe, (axe.x >> 16) - 120, 100, 16, 16);
        frames++;
    }
    assert(axe.vx == -0x80000 && axe.state == 1);
    aos_axe_update(&axe, (axe.x >> 16) - 120, 100, 16, 16);
    assert(axe.vx == -0x80000);
    /* Off screen: state 2, deleted the next update. */
    assert(aos_axe_update(&axe, -1000, 100, 16, 16) && axe.state == 2);
    assert(!aos_axe_update(&axe, -1000, 100, 16, 16));
    AosAxe left = aos_axe_create(100 << 16, 200 << 16, true);
    assert(left.vx == -0x34000 && left.ax == 0x1000 && left.spin == -1);
    aos_axe_update(&left, 0, 100, 16, 16);
    aos_axe_update(&left, 0, 100, 16, 16);
    assert(left.angle == (uint16_t)-0x800);
}

static void trail_tests(void) {
    AosAxe axe = aos_axe_create(100 << 16, 200 << 16, false);
    AosAxeTrail trail = aos_axe_trail_create(0, false);
    assert(trail.lag == 3 && trail.angle == -0x1800);
    assert(aos_axe_trail_create(1, true).lag == 6 && aos_axe_trail_create(1, true).angle == 0x3000);
    int16_t xs[8];
    for (int f = 0; f < 8; ++f) {
        aos_axe_update(&axe, 0, 100, 16, 16);
        xs[f] = (int16_t)(axe.x >> 16);
        assert(aos_axe_trail_update(&trail, &axe));
        assert(trail.y == axe.y);
        /* lag frames back; the start fills the history with the first X. */
        assert(trail.x >> 16 == (f >= 3 ? xs[f - 3] : xs[0]));
    }
    assert(trail.angle == -0x1800 + 8 * 0x800);
    axe.state = 2;
    assert(!aos_axe_trail_update(&trail, &axe));
}

int main(void) {
    axe_tests();
    trail_tests();
    puts("aos skill axe tests passed");
    return 0;
}
