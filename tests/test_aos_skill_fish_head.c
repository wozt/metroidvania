/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillFishHeadUse. */
#include "aos_skill_fish_head.h"
#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>

static void accelerates_right(void) {
    aos_audio_clear();
    AosFishHeadSoul fish = aos_fish_head_soul_create(100 << 16, 160 << 16, false);
    assert(fish.x == 108 << 16 && fish.y == 134 << 16);
    assert(fish.vx == 0x20000 && fish.ax == 0x1000);
    assert(aos_fish_head_soul_update(&fish, 0, 0, 4000, 4000));
    assert(fish.x == (108 << 16) + 0x21000 && fish.vx == 0x21000);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xD8) == 1);
    for (int i = 1; i < 96; ++i)
        assert(aos_fish_head_soul_update(&fish, 0, 0, 4000, 4000));
    assert(fish.vx == 0x80000);
    assert(aos_fish_head_soul_update(&fish, 0, 0, 4000, 4000));
    assert(fish.vx == 0x80000);
    fish.struck = true;
    assert(!aos_fish_head_soul_update(&fish, 0, 0, 4000, 4000));
    assert(fish.state == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xD8) == 1);
}

static void mirrors_and_exits(void) {
    AosFishHeadSoul fish = aos_fish_head_soul_create(100 << 16, 160 << 16, true);
    assert(fish.x == 92 << 16 && fish.vx == -0x20000 && fish.ax == -0x1000);
    assert(aos_fish_head_soul_update(&fish, 0, 0, 16, 16));
    assert(fish.x == (92 << 16) - 0x21000);
    assert(!aos_fish_head_soul_update(&fish, 1000, 1000, 16, 16));
    assert(!aos_fish_head_soul_update(NULL, 0, 0, 0, 0));
}

int main(void) {
    accelerates_right();
    mirrors_and_exits();
    puts("aos_skill_fish_head: OK (ROM-free source-derived accelerating shot)");
    return 0;
}
