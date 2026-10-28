/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_throw.h"

/* A record shaped like the Skeleton's (parameter 3): half a pixel forward,
 * four up, gravity 0x2000, spin 0x0B00 (values chosen for the test). */
static const AosThrowRecord bone = {0x8000, -0x40000, 0x2000, 7, 0, 0, 0, 0xE6, 0x0080,
                                    0x0B00, 0};

static void flight_tests(void) {
    assert(aos_throw_supported(&bone));
    AosThrow e = aos_throw_create(100 << 16, 200 << 16, false);
    assert(e.x == 108 << 16 && e.y == 174 << 16 && e.state == 0);
    /* The first update only sets the speeds, then the tail turns it. */
    assert(aos_throw_update(&e, &bone, 0, 0, 16, 16));
    assert(e.state == 1 && e.vx == 0x8000 && e.vy == -0x40000 && e.x == 108 << 16);
    assert(e.angle == 0x0B00 && e.spin == 0x0B00);
    /* Flying: the gravity joins the speed before the move. */
    assert(aos_throw_update(&e, &bone, 0, 0, 16, 16));
    assert(e.vy == -0x3E000 && e.y == (174 << 16) - 0x3E000 && e.x == (108 << 16) + 0x8000);
    assert(e.angle == 0x1600);
    /* The fall is capped at 8 pixels a frame (camera following it). */
    for (int i = 0; i < 400; ++i) aos_throw_update(&e, &bone, (e.x >> 16) - 120, (e.y >> 16) - 80, 16, 16);
    assert(e.vy == 0x80000 && e.state == 1);
    /* The tail tests the moved position: y 176 is still on the margin
     * (160 + 16), y 184 is past it and the next update deletes it. */
    AosThrow edge = aos_throw_create(100 << 16, 200 << 16, false);
    aos_throw_update(&edge, &bone, 0, 0, 16, 16);
    edge.vy = 0x80000 - 0x2000;
    edge.y = (176 << 16) - 0x80000;
    assert(aos_throw_update(&edge, &bone, 0, 0, 16, 16) && edge.state == 1 && edge.y >> 16 == 176);
    assert(aos_throw_update(&edge, &bone, 0, 0, 16, 16) && edge.state == 3 && edge.y >> 16 == 184);
    assert(!aos_throw_update(&edge, &bone, 0, 0, 16, 16));
}

static void facing_left_tests(void) {
    AosThrow e = aos_throw_create(100 << 16, 200 << 16, true);
    assert(e.x == 92 << 16);
    aos_throw_update(&e, &bone, 0, 0, 16, 16);
    assert(e.vx == -0x8000 && e.spin == -0x0B00 && e.angle == (uint16_t)-0x0B00);
    aos_throw_update(&e, &bone, 0, 0, 16, 16);
    assert(e.x == (92 << 16) - 0x8000);
}

static void refused_tests(void) {
    AosThrowRecord bounce = bone;
    bounce.flags = 0x0044;
    assert(!aos_throw_supported(&bounce));
    bounce.flags = AOS_THROW_ANIMATED;
    assert(!aos_throw_supported(&bounce));
}

int main(void) {
    flight_tests();
    facing_left_tests();
    refused_tests();
    puts("aos skill throw tests passed");
    return 0;
}
