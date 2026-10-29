/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free, deterministic phase tests for Skill_EvilButcher_Ripper_Use. */
#include "aos_skill_ripper.h"
#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>

static void flying_and_rebound(void) {
    aos_audio_clear();
    AosRipperSoul r = aos_ripper_soul_create(100 << 16, 160 << 16, false, 0, true);
    assert(r.state == AOS_RIPPER_FLYING && r.x == 100 << 16);
    assert(r.y == 134 << 16 && r.vx == 0x60000);
    assert(aos_ripper_soul_update(&r, 0, false, NULL));
    assert(r.x == 106 << 16 && aos_audio_requested(AOS_AUDIO_START_SONG, 0xE6) == 1);
    assert(aos_ripper_soul_update(&r, 3, false, NULL));
    assert(r.x == 115 << 16 && r.state == AOS_RIPPER_REBOUNDING);
    assert(r.vx == -0x18000 && r.vy == -0x20000 && r.ay == 0x2000);
    assert(r.timer == AOS_RIPPER_END_FRAMES);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x138) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xE6) == 1);
    assert(aos_ripper_soul_update(&r, 0, false, NULL));
    assert(r.y == 132 << 16 && r.vy == -0x1E000 && r.angle == -1);
    assert(r.timer == AOS_RIPPER_END_FRAMES - 1 && r.blink == 1);
    for (int i = 0; i < 49; ++i) assert(aos_ripper_soul_update(&r, 0, false, NULL));
    assert(r.timer == 0 && r.blink == 2);
    assert(!aos_ripper_soul_update(&r, 0, false, NULL));
    assert(r.state == AOS_RIPPER_REMOVED);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x138) == 1);
}

static void facing_left_and_fade(void) {
    aos_audio_clear();
    AosRipperSoul r = aos_ripper_soul_create(100 << 16, 160 << 16, true, 1, false);
    assert(r.vx == -0x60000);
    assert(aos_ripper_soul_update(&r, -2, false, NULL));
    assert(r.x == 92 << 16 && r.vx == 0x18000);
    assert(r.state == AOS_RIPPER_FADING && r.timer == 50);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x138) == 0);
    for (int i = 0; i < 50; ++i) assert(aos_ripper_soul_update(&r, 0, false, NULL));
    assert(!aos_ripper_soul_update(&r, 0, false, NULL));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xE6) == 1);
}

static void outside_and_invalid_variant(void) {
    aos_audio_clear();
    AosRipperSoul r = aos_ripper_soul_create(100 << 16, 160 << 16, false, 0, true);
    assert(!aos_ripper_soul_update(&r, 0, true, NULL));
    assert(!aos_ripper_soul_update(&r, 0, false, NULL));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xE6) == 1);
    r = aos_ripper_soul_create(0, 0, false, 2, true);
    assert(r.state == AOS_RIPPER_REMOVED &&
           !aos_ripper_soul_update(&r, 0, false, NULL));
    assert(!aos_ripper_soul_update(NULL, 0, false, NULL));
}

static void attached_target_lifecycle(void) {
    AosRipperSoul r = aos_ripper_soul_create(100 << 16, 80 << 16, false, 1, true);
    assert(aos_ripper_soul_update(&r, 0, false, NULL));
    aos_ripper_soul_attach(&r, 104 << 16, 60 << 16);
    assert(r.attached && r.timer == 0x78 && r.target_dx == 2 << 16 &&
           r.target_dy == -(6 << 16));
    AosRipperTarget target = {.x = 110 << 16, .y = 64 << 16, .active = true};
    for (int frame = 0; frame < 21; ++frame) {
        assert(aos_ripper_soul_update(&r, 0, false, &target));
        assert(r.x == 112 << 16 && r.y == 58 << 16);
    }
    assert(!aos_ripper_soul_update(&r, 0, false, &target));

    r = aos_ripper_soul_create(100 << 16, 80 << 16, false, 0, true);
    aos_ripper_soul_attach(&r, 100 << 16, 80 << 16);
    assert(!aos_ripper_soul_update(&r, 0, false, &target));
    r = aos_ripper_soul_create(100 << 16, 80 << 16, false, 1, true);
    aos_ripper_soul_attach(&r, 100 << 16, 80 << 16);
    target.defeated = true;
    assert(!aos_ripper_soul_update(&r, 0, false, &target));
    r = aos_ripper_soul_create(100 << 16, 80 << 16, false, 1, true);
    aos_ripper_soul_attach(&r, 100 << 16, 80 << 16);
    target.defeated = false;
    assert(!aos_ripper_soul_update(&r, 0, true, &target));
}

int main(void) {
    flying_and_rebound();
    facing_left_and_fade();
    outside_and_invalid_variant();
    attached_target_lifecycle();
    puts("aos_skill_ripper: OK (ROM-free source-derived motion and hit helper)");
    return 0;
}
