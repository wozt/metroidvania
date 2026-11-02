/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_audio.h"
#include "aos_enemy.h"
#include "aos_soul.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* Record 4 as on the ROM: SkillBuerUse, toggled, 5 MP per second. */
    AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS] = {{0}};
    records[4] = (AosBlueSoulRecord){0x0802FF41, 5, 2, 0, 0};
    AosStats stats = {.blue_soul = 4, .mp = 80, .max_mp = 80};
    AosGuardian guardian = {0};
    uint32_t flags = 0x100000;
    AosBuerGuardian buer = {0};
    aos_audio_clear();
    assert(aos_guardian_try(&guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, false, &flags));

    /* First update: both sounds, height -16 eased to -16.25 after use, radius
     * 1, the flames opposite each other at Soma's x and his feet - 16. */
    assert(aos_buer_guardian_update(&buer, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R,
                                    &flags, 100, 200, false));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x1A5) == 1 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x92) == 1);
    assert(guardian.state == 1 && buer.radius == 1 && buer.height == (int32_t)0xFFEFC000);
    assert(buer.x == 100 << 16 && buer.y == 184 << 16);
    assert(buer.flames[0].x == (101 << 16) && buer.flames[0].y == 184 << 16);
    assert(buer.flames[1].x == (99 << 16) && buer.flames[1].y == 184 << 16);
    assert(buer.flames[0].angle == 0x200 && buer.flames[1].angle == 0x8200);

    /* The radius stops at 28; standing, the height reaches -20. */
    for (int frame = 0; frame < 40; ++frame)
        assert(aos_buer_guardian_update(&buer, &guardian, &stats, records, 0, 0, &flags, 100,
                                        200, false));
    assert(buer.radius == 28 && buer.height == (int32_t)0xFFEC0000 && buer.y == 180 << 16);
    int32_t cos_x = aos_sine((uint32_t)((int16_t)buer.flames[0].angle + 0x4000));
    assert(aos_buer_guardian_update(&buer, &guardian, &stats, records, 0, 0, &flags, 100, 200,
                                    false));
    assert(buer.flames[0].x == (100 << 16) + cos_x * 28);
    /* Crouching (0x400) it eases down to -8. */
    flags |= 0x400;
    for (int frame = 0; frame < 60; ++frame)
        aos_buer_guardian_update(&buer, &guardian, &stats, records, 0, 0, &flags, 100, 200, false);
    assert(buer.height == (int32_t)0xFFF80000);
    flags &= ~0x400u;

    /* A second press ends it: the radius shrinks by 2 (no more MP drain),
     * then at 0 the soul is cleaned up. */
    int16_t mp = stats.mp;
    assert(aos_buer_guardian_update(&buer, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R,
                                    &flags, 100, 200, false));
    assert(guardian.state == 2 && buer.radius == 26);
    int frames = 0;
    while (aos_buer_guardian_update(&buer, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R,
                                    &flags, 100, 200, false))
        ++frames;
    assert(frames == 13 && !guardian.active && !(flags & 0x200) && stats.mp == mp);

    /* Facing left, the flames turn the other way. */
    guardian = (AosGuardian){0};
    buer = (AosBuerGuardian){0};
    assert(aos_guardian_try(&guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, false, &flags));
    aos_buer_guardian_update(&buer, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, &flags,
                             100, 200, true);
    assert(buer.flames[0].speed == -0x200 && buer.flames[0].angle == 0xFE00);
    puts("aos_guardian_buer: ok");
    return 0;
}
