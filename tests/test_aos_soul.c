/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul.h"

#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* Records 2 and 3 as on the ROM: the bat's (5 MP, 3 at once) and
     * another with a skill (8 MP, 3 at once); record 4 has no skill. */
    AosRedSoulRecord records[AOS_RED_SOUL_RECORDS] = {{0}};
    records[2] = (AosRedSoulRecord){0x08026C55, 0, 5, 3, 0, 60, 0};
    records[3] = (AosRedSoulRecord){0x08025785, 0, 8, 3, 3, 75, 0};
    records[5] = (AosRedSoulRecord){0x08024275, 0, 22, 1, 6, 60, 1};
    AosStats stats = {.red_soul = 2, .mp = 12, .max_mp = 80};
    AosRedSoulCaster caster = {0};

    /* sub_08019478: the cast takes the first free slot and spends the MP;
     * Soma may spend his last points (cost == MP). */
    assert(aos_red_soul_try(&caster, &stats, records, false) == 0 && stats.mp == 7 &&
           caster.last_used == 2 && caster.slots[0]);
    caster.active = 1;   /* the skill entity's first update */
    assert(aos_red_soul_try(&caster, &stats, records, false) == 1 && stats.mp == 2);
    caster.active = 2;
    assert(aos_red_soul_try(&caster, &stats, records, false) == -1 && stats.mp == 2);
    stats.mp = 5;
    assert(aos_red_soul_try(&caster, &stats, records, false) == 2 && stats.mp == 0);
    /* The record's maximum (3) stops a fourth. */
    caster.active = 3;
    stats.mp = 50;
    assert(aos_red_soul_try(&caster, &stats, records, false) == -1 && stats.mp == 50);
    /* Changing the soul while some are alive keeps the last used one: the
     * new soul waits until every skill entity ended. */
    stats.red_soul = 3;
    assert(aos_red_soul_try(&caster, &stats, records, false) == -1 && caster.last_used == 2);
    aos_red_soul_release(&caster, 0);
    aos_red_soul_release(&caster, 1);
    aos_red_soul_release(&caster, 2);
    assert(caster.active == 0 && !caster.slots[0] && !caster.slots[2]);
    aos_red_soul_release(&caster, 2);
    assert(caster.active == 0);
    assert(aos_red_soul_try(&caster, &stats, records, false) == 0 && caster.last_used == 3 &&
           stats.mp == 42);
    /* No skill routine, no soul, the locks. */
    caster = (AosRedSoulCaster){0};
    stats.red_soul = 4;
    assert(aos_red_soul_try(&caster, &stats, records, false) == -1);
    stats.red_soul = 0;
    assert(aos_red_soul_try(&caster, &stats, records, false) == -1);
    stats.red_soul = 2;
    assert(aos_red_soul_try(&caster, &stats, records, true) == -1);
    /* sub_0802D2BC reads the last used soul's maximum: 8 slots at most. */
    records[2].maximum = 20;
    stats.mp = 1000;
    for (int i = 0; i < AOS_RED_SOUL_SLOTS; ++i) {
        assert(aos_red_soul_try(&caster, &stats, records, false) == i);
        caster.active++;
    }
    assert(aos_red_soul_try(&caster, &stats, records, false) == -1);

    /* sub_0802D1D8 over 0x080E1918. */
    assert(aos_red_soul_cast_animation(0, false) == 22 &&
           aos_red_soul_cast_animation(0, true) == 49 &&
           aos_red_soul_cast_animation(1, false) == 78 &&
           aos_red_soul_cast_animation(3, true) == 80);

    /* sub_08021994: one MP every 64 ticks, every 16 with accessory 0x29,
     * always full with 0x2C. */
    AosStats regen = {.mp = 10, .max_mp = 11, .accessory = 0xFF};
    aos_stats_mp_regen(&regen, 1);
    assert(regen.mp == 10);
    aos_stats_mp_regen(&regen, 64);
    assert(regen.mp == 11);
    aos_stats_mp_regen(&regen, 128);
    assert(regen.mp == 11);
    regen.mp = 0;
    regen.accessory = 0x29;
    aos_stats_mp_regen(&regen, 16);
    aos_stats_mp_regen(&regen, 40);
    assert(regen.mp == 1);
    regen.accessory = 0x2C;
    aos_stats_mp_regen(&regen, 3);
    assert(regen.mp == 11);
    /* sub_08021530(5): the bat (power 60) at STR 10, INT 11 gives
     * (600 / 10 + 11 / 4 + 360) / 16 = 26, 20 when reduced. */
    AosStats caster_stats = {0};
    caster_stats.current[AOS_STAT_STR] = 10;
    caster_stats.current[AOS_STAT_INT] = 11;
    assert(aos_red_soul_power(&caster_stats, &records[2], false) == 26);
    assert(aos_red_soul_power(&caster_stats, &records[2], true) == 20);
    caster_stats.current[AOS_STAT_STR] = 99;
    caster_stats.current[AOS_STAT_INT] = 40;
    assert(aos_red_soul_power(&caster_stats, &records[3], false) ==
           (99 * 75 / 10 + 10 + 75 * 6) / 16);
    /* Blue souls (0x080E192C): Flying Armor 1 is held (5 MP/s), Giant Bat 2
     * toggled (30 MP/s), Medusa Head 15 held (10 MP/s). */
    AosBlueSoulRecord blue[AOS_BLUE_SOUL_RECORDS] = {{0}};
    blue[1] = (AosBlueSoulRecord){0x0802E23D, 5, 1, 0, 0x100};
    blue[2] = (AosBlueSoulRecord){0x0802EB85, 30, 2, 0, 0};
    blue[15] = (AosBlueSoulRecord){0x08030CC9, 10, 1, 0, 0};
    AosStats guard_stats = {.blue_soul = 15, .mp = 80, .max_mp = 500};
    AosGuardian guardian = {0};
    uint32_t soma_flags = 2;
    /* sub_0802E0C4: R held, MP at least 500 / 128 + 1 = 4. */
    assert(!aos_guardian_try(&guardian, &guard_stats, blue, 0, AOS_KEY_R, false, &soma_flags));
    assert(!aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, true, &soma_flags));
    guard_stats.mp = 3;
    assert(!aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    guard_stats.mp = 4;
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags) &&
           guardian.active && guardian.interval == 6 && guardian.soul == 15 &&
           (soma_flags & 0x200));
    /* A second one waits for the first to end. */
    assert(!aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    /* sub_0802DF2C: one MP every 6 frames; a held soul ends on release. */
    for (int frame = 1; frame <= 5; ++frame)
        assert(!aos_guardian_upkeep(&guardian, &guard_stats, blue, AOS_KEY_R, 0));
    assert(guard_stats.mp == 4);
    assert(!aos_guardian_upkeep(&guardian, &guard_stats, blue, AOS_KEY_R, 0) &&
           guard_stats.mp == 3);
    assert(aos_guardian_upkeep(&guardian, &guard_stats, blue, 0, 0));
    aos_guardian_end(&guardian, &soma_flags);
    assert(!guardian.active && !(soma_flags & 0x200) && soma_flags == 2);
    /* At 0 MP it ends whatever the button. */
    guard_stats.mp = 1;
    guardian = (AosGuardian){.active = true, .soul = 15, .interval = 6, .ticks = 5};
    assert(aos_guardian_upkeep(&guardian, &guard_stats, blue, AOS_KEY_R, 0) &&
           guard_stats.mp == 0);
    /* A toggled soul needs the press, ignores the release and ends with a
     * second press once its routine has left state 0. */
    guard_stats = (AosStats){.blue_soul = 2, .mp = 80, .max_mp = 80};
    guardian = (AosGuardian){0};
    assert(!aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, AOS_KEY_R, false,
                            &soma_flags) && guardian.interval == 2);
    assert(!aos_guardian_upkeep(&guardian, &guard_stats, blue, AOS_KEY_R, AOS_KEY_R));
    guardian.state = 1;
    assert(!aos_guardian_upkeep(&guardian, &guard_stats, blue, 0, 0) && guard_stats.mp == 79);
    assert(aos_guardian_upkeep(&guardian, &guard_stats, blue, AOS_KEY_R, AOS_KEY_R));
    /* No routine (record 0): nothing starts. */
    guard_stats.blue_soul = 0;
    guardian = (AosGuardian){0};
    assert(!aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, AOS_KEY_R, false,
                             &soma_flags));

    /* SkillMedusaHeadUse: Soma rising at -2.0 is held with vy 0 kept. */
    aos_audio_clear();
    guard_stats = (AosStats){.blue_soul = 15, .mp = 80, .max_mp = 80};
    guardian = (AosGuardian){0};
    soma_flags = 2;
    AosMedusaGuardian medusa = {0};
    int32_t vy = (int32_t)0xFFFE0000, gravity = (int32_t)0xFFFFE000;
    bool still = false;
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    assert(aos_medusa_guardian_update(&medusa, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                      &soma_flags, &vy, &gravity, &still));
    assert(medusa.vy == 0 && medusa.gravity == 0x2800 && vy == 0 && gravity == 0 && still &&
           guardian.state == 1 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x8C) == 1);
    /* Released: Soma gets the kept vy and gravity back. */
    vy = 0x1A00;
    still = false;
    assert(!aos_medusa_guardian_update(&medusa, &guardian, &guard_stats, blue, 0, 0,
                                       &soma_flags, &vy, &gravity, &still));
    assert(vy == 0 && gravity == 0x2800 && !still && !guardian.active &&
           !(soma_flags & 0x200) && aos_audio_requested(AOS_AUDIO_START_SONG, 0x8C) == 1);
    /* Falling at 3.0: the fall speed is kept; landing ends it. */
    vy = 0x30000;
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    medusa = (AosMedusaGuardian){0};
    assert(aos_medusa_guardian_update(&medusa, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                      &soma_flags, &vy, &gravity, &still) &&
           medusa.vy == 0x30000);
    soma_flags = 0x100000 | 0x200;
    assert(!aos_medusa_guardian_update(&medusa, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                       &soma_flags, &vy, &gravity, &still));
    assert(vy == 0x30000 && gravity == 0x2800 && soma_flags == 0x100000);
    /* Started on the ground (or against a ceiling, flag 0x8000), it ends on
     * its first update without a sound. */
    aos_audio_clear();
    soma_flags = 2 | 0x8000;
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    assert(!aos_medusa_guardian_update(&medusa, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                       &soma_flags, &vy, &gravity, &still) &&
           !aos_audio_requested(AOS_AUDIO_START_SONG, 0x8C) && !guardian.active);
    /* SkillCagnazzoUse (soul 11, toggled, 5 MP/s): sound 0x1AA once, placed
     * 16 ahead and 32 above Soma; a second press ends it. */
    blue[11] = (AosBlueSoulRecord){0x0802E4ED, 5, 2, 0, 0};
    aos_audio_clear();
    guard_stats = (AosStats){.blue_soul = 11, .mp = 80, .max_mp = 80};
    guardian = (AosGuardian){0};
    soma_flags = 0x100000;
    AosCagnazzoGuardian cagnazzo = {0};
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, AOS_KEY_R, false,
                            &soma_flags) && guardian.interval == 12);
    assert(aos_cagnazzo_guardian_update(&cagnazzo, &guardian, &guard_stats, blue, AOS_KEY_R,
                                        AOS_KEY_R, &soma_flags, 100, 200, false));
    assert(cagnazzo.x == 116 && cagnazzo.y == 168 && !cagnazzo.facing_left &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x1AA) == 1);
    assert(aos_cagnazzo_guardian_update(&cagnazzo, &guardian, &guard_stats, blue, 0, 0,
                                        &soma_flags, 100, 200, true));
    assert(cagnazzo.x == 84 && cagnazzo.facing_left &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x1AA) == 1);
    assert(!aos_cagnazzo_guardian_update(&cagnazzo, &guardian, &guard_stats, blue, AOS_KEY_R,
                                         AOS_KEY_R, &soma_flags, 100, 200, true) &&
           !guardian.active && soma_flags == 0x100000);
    /* sub_08021530(6): 12 + 12 / 8 * (INT / 2); 8 / 10 when reduced. */
    guard_stats.current[AOS_STAT_INT] = 21;
    assert(aos_guardian_power(&guard_stats, AOS_CAGNAZZO_POWER, false) == 22);
    assert(aos_guardian_power(&guard_stats, AOS_CAGNAZZO_POWER, true) == 17);
    puts("Aria red and blue soul tests passed");
    return 0;
}
