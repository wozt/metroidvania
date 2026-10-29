/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillSkullMillioneUse. */
#include "aos_skill_millione.h"
#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>

static void right_facing_sequence(void) {
    aos_audio_clear();
    AosMillioneSoul soul = aos_millione_soul_create();
    AosMillioneSpawn spawn;
    int spawned = 0;
    const int expected_x[] = {121, 123, 125};
    for (int frame = 0; frame < 6; ++frame) {
        assert(aos_millione_soul_update(&soul, 100 << 16, 80 << 16, false, &spawn));
        assert(soul.x == (120 + frame) << 16 && soul.y == 60 << 16);
        if (spawn.created) {
            assert(spawned < AOS_MILLIONE_CHILDREN);
            assert(spawn.x == expected_x[spawned] << 16 && spawn.y == 60 << 16);
            assert(!spawn.facing_left);
            spawned++;
        }
    }
    assert(spawned == AOS_MILLIONE_CHILDREN);
    assert(soul.state == AOS_MILLIONE_ENDING && soul.emitted == AOS_MILLIONE_CHILDREN);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xC9) == 1);
    assert(!aos_millione_soul_update(&soul, 100 << 16, 80 << 16, false, &spawn));
    assert(soul.state == AOS_MILLIONE_REMOVED && !spawn.created);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xC9) == 1);
}

static void follows_soma_and_mirrors(void) {
    AosMillioneSoul soul = aos_millione_soul_create();
    AosMillioneSpawn spawn;
    assert(aos_millione_soul_update(&soul, 90 << 16, 100 << 16, true, &spawn));
    assert(soul.x == 70 << 16 && soul.y == 80 << 16 && soul.facing_left);
    assert(!spawn.created);
    assert(aos_millione_soul_update(&soul, 92 << 16, 98 << 16, true, &spawn));
    assert(soul.x == 71 << 16 && soul.y == 78 << 16);
    assert(spawn.created && spawn.facing_left && spawn.x == soul.x && spawn.y == soul.y);
    assert(!aos_millione_soul_update(NULL, 0, 0, false, &spawn));
}

int main(void) {
    right_facing_sequence();
    follows_soma_and_mirrors();
    puts("aos_skill_millione: OK (ROM-free source-derived parent emission)");
    return 0;
}
