/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/runtime/aos_skill_spear.h"

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

static void flight_tests(void) {
    memset(cells, 0, sizeof cells);
    AosSpear s = aos_spear_create((100 << 16) + 0x8000, 200 << 16, false);
    assert(s.x == 108 << 16 && s.y == 174 << 16 && s.angle == -0x800 && s.step == 0x100);
    assert(aos_spear_update(&s, &layer, 0, 100, 16, 16));
    assert(s.state == 1 && s.angle == -0x700 && s.x == 112 << 16 && s.vy == -0x10000);
    /* The tip is 16 pixels along the angle: ahead and a little up. */
    assert(s.tip_x > 15 * 65536 && s.tip_x <= 16 * 65536 && s.tip_y < 0);
    /* It turns until it points down, then stays. */
    for (int i = 0; i < 100; ++i) aos_spear_update(&s, &layer, (s.x >> 16) - 120,
                                                   (s.y >> 16) - 80, 16, 16);
    assert(s.angle == 0x4000 && s.state == 1 && s.vy == 0x80000);
    assert(s.tip_x == 0 && s.tip_y == 16 * 65536);
    /* Leaving the margins: state 3 without moving, deleted next. */
    int32_t x = s.x;
    assert(aos_spear_update(&s, &layer, -1000, 0, 16, 16) && s.state == 3 && s.x == x);
    assert(!aos_spear_update(&s, &layer, -1000, 0, 16, 16));
}

static void left_tests(void) {
    AosSpear s = aos_spear_create(100 << 16, 200 << 16, true);
    assert(s.x == 92 << 16 && s.angle == 0x800 && s.step == -0x100 && s.vx == -0x40000);
    aos_spear_update(&s, &layer, 0, 100, 16, 16);
    /* Turned by 0x8000: the tip is behind the facing, so to the left. */
    assert(s.tip_x < -15 * 65536);
}

static void stick_tests(void) {
    memset(cells, 0, sizeof cells);
    /* A solid wall column at x 128..135. */
    for (int y = 0; y < H; ++y) cells[y * W + 16] = 0x03;
    AosSpear s = aos_spear_create(100 << 16, 120 << 16, false);
    int frames = 0;
    while (s.state == 1 || s.state == 0) {
        aos_spear_update(&s, &layer, 0, 0, 16, 16);
        frames++;
    }
    assert(s.state == 2 && s.timer == 0x20 && frames < 10);
    int32_t x = s.x;
    for (int i = 0; i < 0x20; ++i) aos_spear_update(&s, &layer, 0, 0, 16, 16);
    assert(s.state == 2 && s.blinking && s.x == x && s.timer == 0);
    aos_spear_update(&s, &layer, 0, 0, 16, 16);
    assert(s.state == 3 && !aos_spear_update(&s, &layer, 0, 0, 16, 16));
    /* A special (bit 3) cell does not hold it. */
    memset(cells, 0, sizeof cells);
    for (int y = 0; y < H; ++y) cells[y * W + 16] = 0x08;
    AosSpear t = aos_spear_create(100 << 16, 120 << 16, false);
    for (int i = 0; i < 10; ++i) aos_spear_update(&t, &layer, 0, 0, 16, 16);
    assert(t.state == 1);
}

int main(void) {
    flight_tests();
    left_tests();
    stick_tests();
    puts("aos skill spear tests passed");
    return 0;
}
