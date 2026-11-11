/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/runtime/aos/core/aos_input.h"
#include "../src/runtime/aos/blue_souls/aos_soul_blue_sky_fish.h"

static AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS];

static uint32_t rng_zero(void) { return 0; }

int main(void) {
    AosGuardian guardian = {.active = true, .soul = 21, .state = 0};
    AosStats stats = {.mp = 100, .max_mp = 100};
    uint32_t flags = 0;
    AosSkyFishGuardian fish = {0};

    /* State 0 loads and plays 0x99, entering state 1 with timer 0x10. */
    assert(aos_sky_fish_guardian_update(&fish, &guardian, &stats, records,
                                      AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));
    assert(fish.state == 1 && fish.timer == 0x10 && fish.song_99 &&
           !fish.entered && !fish.fx); /* case 0 jumps to the tail */
    /* State 1 spawns its two sparkles every frame. */
    assert(aos_sky_fish_guardian_update(&fish, &guardian, &stats, records,
                                      AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));
    assert(fish.fx);

    /* State 1 counts 0x10 down then enters state 2 with song 0x1A8 and
     * the + 0x30 mark; the frame counter restarts the cruise. */
    int frames = 0;
    while (fish.state == 1 && frames < 64) {
        assert(aos_sky_fish_guardian_update(&fish, &guardian, &stats, records,
                                          AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));
        frames++;
    }
    assert(fish.state == 2 && fish.entered && fish.song_1a8);
    assert(frames == 16); /* one state-1 update already ran above */

    /* State 2 plays 0x95 on frame 0 and every 0x3C frames, counts to
     * 0x12B then lands in state 4 on the next update. */
    assert(fish.song_95 == 0 || fish.frame == 0);
    frames = 0;
    int songs = 0;
    while (fish.state == 2 && frames < 400) {
        fish.song_95 = false;
        assert(aos_sky_fish_guardian_update(&fish, &guardian, &stats, records,
                                          AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));
        if (fish.song_95) songs++;
        frames++;
    }
    assert(fish.state == 4);
    assert(songs >= 5); /* frames 0, 60, 120, 180, 240, 300 minus end */

    /* State 4 becomes state 5 with the 0xF0 linger (entered was set);
     * the trail sparkles every eighth frame and the delete lands when
     * the timer drains. */
    assert(aos_sky_fish_guardian_update(&fish, &guardian, &stats, records,
                                      AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));
    assert(fish.state == 5 && fish.timer == 0xF0);
    frames = 0;
    while (fish.state == 5 && frames < 512) {
        fish.fx = false;
        uint8_t before = fish.timer;
        bool alive = aos_sky_fish_guardian_update(&fish, &guardian, &stats, records,
                                                AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero);
        /* The burst fires while the OLD + 0x0D is a nonzero multiple
         * of 8 (the native checks before decrementing). */
        if (before && (before & 7) == 0) assert(fish.fx);
        frames++;
        if (!alive) break;
    }
    assert(!guardian.active);

    /* An early upkeep end (+ 0x30 clear) skips the linger: state 5
     * with timer 0 deletes on its first update. */
    AosGuardian g2 = {.active = true, .soul = 21, .state = 0};
    AosSkyFishGuardian f2 = {0};
    assert(aos_sky_fish_guardian_update(&f2, &g2, &stats, records,
                                      AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));
    /* Force the upkeep window: state 2 without the entered mark by
     * hand — the native only sets + 0x30 on the state-1 exit, so drive
     * it there legitimately instead: refuse via flags & 0x1A2 in
     * state 2 before entering? Simplest faithful check: a fresh fish
     * in state 4 with entered clear deletes next update. */
    f2.state = 4;
    f2.entered = false;
    assert(aos_sky_fish_guardian_update(&f2, &g2, &stats, records,
                                      AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));
    assert(f2.state == 5 && f2.timer == 0);
    assert(!aos_sky_fish_guardian_update(&f2, &g2, &stats, records,
                                       AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));

    /* The source + 0x10 & 0x82 refusal deletes through the guardian
     * cleanup on the first update. */
    AosGuardian g3 = {.active = true, .soul = 21, .state = 0};
    AosSkyFishGuardian f3 = {0};
    flags = 0x82;
    assert(!aos_sky_fish_guardian_update(&f3, &g3, &stats, records,
                                       AOS_KEY_R, 0, &flags, 0, 0, false, rng_zero));
    assert(f3.refused);
    flags = 0;

    puts("test_aos_soul_blue_sky_fish: ok");
    return 0;
}
