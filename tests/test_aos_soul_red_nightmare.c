/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/red_souls/aos_soul_red_nightmare.h"

static uint32_t seq_rng(void) {
    static uint32_t s = 0x12345678;
    s = s * 1103515245u + 12345u;
    return s >> 8;
}

int main(void) {
    /* Facing right: placed at +8 px, vx +0x4000 with ax +0x2000,
     * the + 0x58 bit 6 inverted to set. */
    AosNightmare n = aos_nightmare_create(0x6400000, 0x5000000, false);
    assert(n.x == 0x6400000 + 8 * 0x10000);
    assert(n.y == 0x5000000);
    assert(n.vx == 0x4000);
    assert(n.ax == 0x2000);
    assert(n.vy == 0 && n.ay == 0);
    assert(n.facing_left);
    assert(n.state == 1);
    assert(n.charge == 0);

    /* Mirrored: -8 px, negative drift, bit 6 clear. */
    AosNightmare nm = aos_nightmare_create(0x6400000, 0, true);
    assert(nm.x == 0x6400000 - 8 * 0x10000);
    assert(nm.vx == -0x4000);
    assert(nm.ax == -0x2000);
    assert(!nm.facing_left);

    /* State 1 jitters y upward every frame and reports the trail;
     * the charge ramps 0x1000/frame and crosses 0xFFFF on the 17th
     * update, entering state 2 (which still jitters that frame). */
    int frames = 0;
    int32_t y0 = n.y;
    while (n.state == 1 && frames < 64) {
        int32_t before = n.y;
        assert(aos_nightmare_update(&n, seq_rng));
        assert(n.trail);
        /* y always decreases: jitter in [-0x140000, -0xC0001] plus
         * the (still small) vy integration. */
        assert(n.y < before);
        frames++;
    }
    assert(n.state == 2);
    assert(frames == 17);
    assert(n.y < y0 - 12 * 0x10000 * 10); /* rose well over 120 px */

    /* State 2 stops the jitter/trail but keeps the drift
     * integration; vx keeps growing by ax until the 0x80000 cap. */
    frames = 0;
    while (n.state == 2 && frames < 200 && n.vx < 0x80000) {
        int32_t vx0 = n.vx;
        assert(aos_nightmare_update(&n, seq_rng));
        assert(!n.trail);
        assert(n.vx == vx0 + n.ax || n.vx == 0x80000);
        frames++;
    }
    assert(n.vx == 0x80000); /* capped drift */

    /* The margin probe deletes immediately from any state. */
    AosNightmare out = aos_nightmare_create(0, 0, false);
    out.outside = true;
    assert(!aos_nightmare_update(&out, seq_rng));
    AosNightmare marked = aos_nightmare_create(0, 0, false);
    marked.marked = true;
    assert(!aos_nightmare_update(&marked, seq_rng));

    puts("test_aos_soul_red_nightmare: ok");
    return 0;
}
