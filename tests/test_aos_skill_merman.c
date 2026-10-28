/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_merman.h"

static void create_tests(void) {
    AosMermanDrop d = aos_merman_drop_create(4, 100 << 16, 50 << 16, false);
    assert(d.vx == 0x50000 && d.ax == -0xA00 && d.gravity == 0x2000 && d.splash == 6);
    AosMermanDrop left = aos_merman_drop_create(0, 100 << 16, 50 << 16, true);
    assert(left.vx == -0x10000 && left.ax == 0x200 && left.facing_left);
    AosMermanDrop slow = aos_merman_drop_create(0, 0, 0, false);
    assert(slow.ax == -0x200);
}

static void flight_tests(void) {
    AosMermanDrop d = aos_merman_drop_create(0, 100 << 16, 50 << 16, false);
    bool splash;
    /* The first update arms it and already moves. */
    assert(aos_merman_drop_update(&d, 0, 0, 16, 16, &splash));
    assert(d.state == 1 && d.attacking && d.timer == 15 && !splash);
    assert(d.vx == 0x10000 - 0x200 && d.x == (100 << 16) + 0x10000 - 0x200 && d.y == 50 << 16);
    /* The splash frame is the one where the timer equals index + 2. */
    int splashes = 0, frames = 1;
    while (d.state == 1) {
        aos_merman_drop_update(&d, 0, 0, 16, 16, &splash);
        splashes += splash;
        frames++;
    }
    assert(splashes == 1 && frames == 17 && !d.attacking && d.state == 2);
    /* The turning frame moves X twice and starts the fall. */
    assert(d.vy == 0x2000 && d.vx == 0x10000 - 18 * 0x200);
    /* It falls until it leaves the margins, then goes the next update. */
    while (d.state == 2) aos_merman_drop_update(&d, 0, 0, 16, 16, &splash);
    assert(d.state == 3 && d.vy <= 0x80000);
    assert(!aos_merman_drop_update(&d, 0, 0, 16, 16, &splash));
}

int main(void) {
    create_tests();
    flight_tests();
    puts("aos skill merman tests passed");
    return 0;
}
