/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_devil.h"

int main(void) {
    AosDevilFire up = aos_devil_fire_create(0, 100 << 16, 80 << 16, false);
    AosDevilFire mid = aos_devil_fire_create(1, 100 << 16, 80 << 16, false);
    AosDevilFire left = aos_devil_fire_create(2, 100 << 16, 80 << 16, true);
    assert(up.angle == -0x1800 && mid.angle == 0 && left.angle == (int16_t)(-0x8000 - 0x1800));
    /* Frames: angle 0 (right) is frame 4, up-right -0x1800 rounds to 2. */
    assert(aos_devil_fire_frame(&mid) == 4 && aos_devil_fire_frame(&up) == 2);

    /* No target: straight flight, accelerating 0x4000 a frame up to 0x50000. */
    const bool none[4] = {false, false, false, false};
    aos_devil_fire_launch(&mid, 2, none);
    assert(mid.state == 3 && mid.target == -1 && mid.timer == 0x40);
    aos_devil_fire_update(&mid, NULL);
    assert(mid.speed == 0x4000 && mid.vx == (0x10000 >> 8) * (0x4000 >> 8) && mid.vy == 0);
    for (int i = 0; i < 30; ++i) aos_devil_fire_update(&mid, NULL);
    assert(mid.speed == 0x50000);

    /* The first filled slot from random & 3, wrapping. */
    const bool slot1[4] = {false, true, false, false};
    AosDevilFire homing = aos_devil_fire_create(1, 100 << 16, 80 << 16, false);
    aos_devil_fire_launch(&homing, 6, slot1);
    assert(homing.target == 1 && homing.state == 2);
    aos_devil_fire_launch(&homing, 3, slot1);
    assert(homing.target == 1);

    /* Homing: a target straight below turns it 0x100 a frame toward 0x4000. */
    AosSkillTarget below = {true, 100 << 16, 200 << 16, 10};
    aos_devil_fire_update(&homing, &below);
    assert(homing.angle == 0x100 && homing.timer == 0x3F && homing.state == 2);
    for (int i = 0; i < 10; ++i) aos_devil_fire_update(&homing, &below);
    assert(homing.angle == 0xB00);
    /* Its hit, the target's death or the timer end the homing. */
    homing.struck = true;
    aos_devil_fire_update(&homing, &below);
    assert(homing.state == 3);
    AosDevilFire chaser = aos_devil_fire_create(1, 100 << 16, 80 << 16, false);
    aos_devil_fire_launch(&chaser, 1, slot1);
    AosSkillTarget dead = below;
    dead.hp = 0;
    aos_devil_fire_update(&chaser, &dead);
    assert(chaser.state == 3);
    AosDevilFire patient = aos_devil_fire_create(1, 100 << 16, 80 << 16, false);
    aos_devil_fire_launch(&patient, 1, slot1);
    for (int i = 0; i < 0x3F; ++i) aos_devil_fire_update(&patient, &below);
    assert(patient.state == 2);
    aos_devil_fire_update(&patient, &below);
    assert(patient.state == 3);

    /* Margins of entry 2. */
    assert(!aos_devil_fire_outside(&up, 0, 0, 16, 16));
    assert(aos_devil_fire_outside(&up, 200, 0, 16, 16));
    puts("aos skill devil tests passed");
    return 0;
}
