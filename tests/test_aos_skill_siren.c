/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_siren.h"

int main(void) {
    AosSirenSoul right = aos_siren_soul_create((100 << 16) + 0xA000,
                                               (200 << 16) + 0x8000, false, 0, 0x4000);
    assert(right.x == 108 << 16 && right.y == 174 << 16);
    assert(right.vx == 0x40000 && right.frame == 2 && right.song == 0xDD);
    assert(aos_siren_soul_update(&right, 0, 100, 16, 16));
    assert(right.x == 112 << 16 && right.y == 175 << 16);
    assert(right.phase == 0x4400 && right.state == 1);

    AosSirenSoul left = aos_siren_soul_create(100 << 16, 200 << 16, true, 1, 0);
    assert(left.x == 92 << 16 && left.vx == -0x40000);
    assert(left.frame == 3 && left.song == 0xDE);
    left.struck = true;
    assert(aos_siren_soul_update(&left, 0, 100, 16, 16) && left.state == 2);
    assert(!aos_siren_soul_update(&left, 0, 100, 16, 16));

    AosSirenSoul outside = aos_siren_soul_create(260 << 16, 180 << 16, false, 0, 0);
    assert(aos_siren_soul_update(&outside, 0, 100, 16, 16) && outside.state == 2);
    assert(!aos_siren_soul_update(&outside, 0, 100, 16, 16));
    puts("aos skill siren tests passed");
    return 0;
}
