/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_shot.h"

int main(void) {
    AosShot s = aos_shot_create((100 << 16) + 0xC000, (200 << 16) + 0x8000, false);
    assert(s.x == 108 << 16 && s.y == 174 << 16 && s.vx == 0x40000);
    /* The first update already moves. */
    assert(aos_shot_update(&s, 0, 100, 16, 16) && s.x == 112 << 16 && s.state == 1);
    /* 112 + 4n passes 0xF0 + 16 = 256 at 260, 37 moves later: deleted then. */
    int moves = 1;
    while (aos_shot_update(&s, 0, 100, 16, 16)) moves++;
    assert(moves == 37 && s.x >> 16 == 260);
    AosShot left = aos_shot_create(100 << 16, 200 << 16, true);
    assert(left.x == 92 << 16 && left.vx == -0x40000 && left.facing_left);
    puts("aos skill shot tests passed");
    return 0;
}
