/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_audio.h"
#include "aos_soul_blue_ghost.h"

#include <assert.h>
#include <stdio.h>

static uint32_t zero(void) { return 0; }

int main(void) {
    /* Records 5 and 6 as on the ROM: the shared routine, toggled, 3 and 5
     * MP per second, argument 0 (the Student Witch) and 1 (the Giant
     * Ghost). */
    AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS] = {{0}};
    records[5] = (AosBlueSoulRecord){0x0802F96D, 3, 2, 0, 0};
    records[6] = (AosBlueSoulRecord){0x0802F96D, 5, 2, 0, 1};
    AosStats stats = {.blue_soul = 6, .mp = 80, .max_mp = 80};
    AosGuardian guardian = {0};
    uint32_t flags = 0;
    AosGhostGuardian ghost = {0};
    int32_t sx = 100 << 16, sy = 150 << 16;
    aos_audio_clear();
    assert(aos_guardian_try(&guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, false, &flags));
    /* It grows by 0x2000 a frame (sound 0x8D) to 1.0, then attacks. */
    assert(aos_ghost_guardian_update(&ghost, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R,
                                     &flags, sx, sy, false, 0, 0));
    assert(ghost.giant && ghost.scale == 0x2000 && guardian.state == 1 && !ghost.attacking &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x8D) == 1 && ghost.x == sx);
    int frames = 1;
    while (guardian.state == 1 && frames < 20) {
        assert(aos_ghost_guardian_update(&ghost, &guardian, &stats, records, 0, 0, &flags, sx,
                                         sy, true, 0, 0));
        ++frames;
    }
    assert(frames == 9 && ghost.scale == 0x10000 && ghost.attacking && ghost.facing_left);
    int8_t box[4];
    aos_ghost_guardian_box(&ghost, box);
    assert(box[0] == -24 && box[1] == -40 && box[2] == 48 && box[3] == 40);
    /* A touch at (140, 134): a spirit thrown at 4.0 away from (100, 134)
     * (the zero random puts the aim 2 pixels up): mostly rightward. */
    aos_ghost_guardian_touch(&ghost, 140, 134, zero);
    assert(ghost.shots[0].alive && ghost.shots[0].x == 140 << 16 &&
           ghost.shots[0].vx > 0x3F000 && ghost.shots[0].vy < 0);
    aos_audio_clear();
    aos_ghost_guardian_update(&ghost, &guardian, &stats, records, 0, 0, &flags, sx, sy, true, 0, 0);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x98) == 1 && ghost.shots[0].state == 1 &&
           ghost.shots[0].x > 140 << 16);
    /* It leaves the screen (margin 16) and frees its slot. */
    for (frames = 0; ghost.shots[0].alive && frames < 100; ++frames)
        aos_ghost_guardian_update(&ghost, &guardian, &stats, records, 0, 0, &flags, sx, sy, true,
                                  0, 0);
    assert(!ghost.shots[0].alive && frames > 20);
    /* Pressed again, it shrinks by 0x2000 a frame and the soul ends. */
    aos_ghost_guardian_update(&ghost, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, &flags, sx,
                              sy, true, 0, 0);
    assert(guardian.state == 3 && !ghost.attacking && ghost.scale == 0xE000);
    for (frames = 0; frames < 20; ++frames)
        if (!aos_ghost_guardian_update(&ghost, &guardian, &stats, records, 0, 0, &flags, sx, sy,
                                       true, 0, 0))
            break;
    assert(frames == 7 && !guardian.active);

    /* The Student Witch: sound 0x97, the (16, -32, 16 x 32) box. */
    stats.blue_soul = 5;
    ghost = (AosGhostGuardian){0};
    aos_audio_clear();
    assert(aos_guardian_try(&guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, false, &flags));
    aos_ghost_guardian_update(&ghost, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, &flags, sx,
                              sy, false, 0, 0);
    aos_ghost_guardian_box(&ghost, box);
    assert(!ghost.giant && aos_audio_requested(AOS_AUDIO_START_SONG, 0x97) == 1 && box[0] == 16 &&
           box[3] == 32);
    puts("aos_soul_blue_ghost: ok");
    return 0;
}
