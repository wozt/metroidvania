/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_crow.h"

int main(void) {
    AosCrow crow = aos_crow_create(false);
    assert(crow.angle == 0x1000 && crow.step == -0x147 && crow.facing_left && crow.timer == 50);
    AosCrow left = aos_crow_create(true);
    assert(left.angle == 0x7000 && left.step == 0x147 && !left.facing_left);
    /* The radius opens to nearly 60 pixels at mid course and closes again. */
    int widest = 0, frames = 0;
    while (aos_crow_update(&crow, 100 << 16, 100 << 16)) {
        if (crow.radius > widest) widest = crow.radius;
        frames++;
    }
    /* Its widest, 25 * 0x28F = 0x3FFB, falls just short of a quarter turn:
     * 59 pixels once truncated; 51 updates from timer 50 to 0. */
    assert(widest == 59 && frames == 51 && crow.radius == 0);
    /* 18 pixels above Soma at radius 0. */
    AosCrow start = aos_crow_create(false);
    start.timer = 0;
    aos_crow_update(&start, 100 << 16, 100 << 16);
    assert(start.radius == 0 && start.x == 100 << 16 && start.y == (100 - 18) << 16);
    assert(start.state == 2 && !aos_crow_update(&start, 0, 0));
    puts("aos skill crow tests passed");
    return 0;
}
