/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/blue_souls/aos_soul_blue_creaking_skull.h"

static uint32_t lcg = 0x12345678;
static uint32_t test_random(void) {
    lcg = lcg * 1103515245 + 12345;
    return (lcg >> 16) & 0x7FFF;
}

static AosStats stats;
static AosBlueSoulRecord blue[AOS_BLUE_SOUL_RECORDS];
static AosGuardian guardian;
static uint32_t soma_flags;
static AosCreakingSkullGuardian skull;

static bool step(bool empowered) {
    return aos_creaking_skull_guardian_update(&skull, &guardian, &stats, blue,
                                              AOS_KEY_R, 0, &soma_flags,
                                              100 << 16, 200 << 16, true,
                                              empowered, test_random);
}

int main(void) {
    blue[14] = (AosBlueSoulRecord){0x08030A25, 5, 2, 0, 0};
    stats = (AosStats){.blue_soul = 14, .mp = 80, .max_mp = 80};
    guardian = (AosGuardian){0};
    soma_flags = 0x100000;
    skull = (AosCreakingSkullGuardian){0};

    /* Cast: state 0 loads record A[0], spawns the three children and
     * faces away from Soma; the anchor sits 0x14 above him. */
    assert(step(false));
    assert(skull.state == 1 && skull.record && skull.record->native_low == 0x1AA8 &&
           !skull.facing_left && skull.x == 100 << 16 &&
           skull.y == (200 << 16) - (0x14 << 16) && skull.children[0].index == 0 &&
           skull.children[2].index == 2);
    assert(!skull.children[0].dead && skull.children[0].state == 1 &&
           skull.children[1].state == 1 && skull.children[2].state == 1);
    assert(skull.children[0].x == skull.x && skull.children[0].y == skull.y);

    /* State 1 ramps + 0x28 by 0x1000 a frame; past 0xFFFF -> state 2
     * -> state 3 with + 0xC armed. */
    for (int i = 0; i < 15; ++i) assert(step(false));
    assert(skull.state == 1 && skull.scale == 0xF000);
    assert(step(false));
    assert(skull.state == 2 && skull.scale == 0x10000);
    assert(step(false));
    assert(skull.state == 3 && skull.flag_c == 1);

    /* State 3 picks A[0] (0x10 -> 15 hold frames), pulses + 0x30 for
     * the children and lands in state 4. */
    assert(step(false));
    assert(skull.state == 4 && skull.timer == 0xF && skull.flag_30 == 1 &&
           skull.field31 == 0 && !skull.song_9b);
    /* The flag_30 pulse pushed children 0/1 through aim into their
     * turn (state 2 computes the velocity and lands in state 3 on
     * the same update, like the native fallthrough). */
    assert(skull.children[0].state == 3 && skull.children[1].state == 3);

    /* 15 decrements then the transition: state 3 reads A[1]. */
    for (int i = 0; i < 15; ++i) assert(step(false));
    assert(skull.state == 4 && skull.timer == 0 && skull.index == 0);
    assert(step(false));
    assert(skull.state == 3 && skull.index == 1);
    assert(step(false));
    assert(skull.state == 4 && skull.index == 1 && skull.timer == 0xF);

    /* A[1] drains (0x10 -> 15), A[2] is NULL -> state 2 restarts
     * table A at index 0, then A[0] is picked again. */
    for (int i = 0; i < 16; ++i) assert(step(false));
    assert(skull.state == 3 && skull.index == 2);
    assert(step(false));
    assert(skull.state == 2 && skull.index == 0);
    assert(step(false));
    assert(skull.state == 3 && skull.flag_c == 1);
    assert(step(false));
    assert(skull.state == 4 && skull.index == 0 && skull.record->native_low == 0x1AA8);

    /* Empowered: the + 0xC recheck switches + 0x34 to table B and
     * state 3 directly — B[0] is record 0 again. */
    assert(step(true));
    assert(skull.state == 4 && skull.index == 0 && skull.record->native_low == 0x1AA8);
    /* Walk table B to its index-3 record (AD0): B[1] 15 frames,
     * B[2] 23 frames, then B[3] picked. */
    for (int i = 0; i < 16; ++i) assert(step(true));
    assert(skull.state == 3 && skull.index == 1);
    assert(step(true));
    assert(skull.state == 4 && skull.index == 1 && skull.timer == 0xF);
    for (int i = 0; i < 16; ++i) assert(step(true));
    assert(skull.state == 3 && skull.index == 2);
    assert(step(true));
    assert(skull.state == 4 && skull.index == 2 && skull.timer == 0x17);
    for (int i = 0; i < 24; ++i) assert(step(true));
    assert(skull.state == 3 && skull.index == 3);
    assert(step(true));
    assert(skull.state == 4 && skull.index == 3 && skull.timer == 0xF &&
           skull.field31 == 0);
    /* AD0's hold drains: song 0x9B fires and index moves to 4. */
    for (int i = 0; i < 15; ++i) assert(!skull.song_9b && step(true));
    assert(step(true));
    assert(skull.song_9b && skull.state == 3 && skull.index == 4);

    /* Upkeep: the toggled soul ends when R is pressed again after
     * its first update — the drain deletes children with it. */
    guardian.soul = 14;
    guardian.state = 1;
    assert(!aos_creaking_skull_guardian_update(&skull, &guardian, &stats, blue,
                                               AOS_KEY_R, AOS_KEY_R, &soma_flags,
                                               100 << 16, 200 << 16, true,
                                               true, test_random));
    for (int k = 0; k < 3; ++k) assert(skull.children[k].dead);

    puts("test_aos_soul_blue_creaking_skull: ok");
    return 0;
}
