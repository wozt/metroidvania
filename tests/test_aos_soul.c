/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul.h"

#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>

static uint32_t zero_random(void) { return 0; }

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
    /* Yellow souls (0x080E1B08): Zombie Officer 3 (flag 0x400000), Erinys
     * 16 (0x2000000), Mimic 17 (0x40000), Gargoyle 21 (resist 0x100),
     * Skeleton Knight 22 (STR + 4), Dead Crusader 29 (CON + 16), Red Crow
     * 30 (INT + 4), Zombie 12, Lubicant 13. */
    AosYellowSoulRecord yellow[AOS_YELLOW_SOUL_RECORDS] = {{0}};
    yellow[3] = (AosYellowSoulRecord){AOS_YELLOW_FLAG_ROUTINE, 0, 0, 0x400000};
    yellow[12] = (AosYellowSoulRecord){AOS_YELLOW_ZOMBIE_ROUTINE, 1, 0, 0};
    yellow[13] = (AosYellowSoulRecord){AOS_YELLOW_LUBICANT_ROUTINE, 1, 0, 0};
    yellow[16] = (AosYellowSoulRecord){AOS_YELLOW_FLAG_ROUTINE, 0, 0, 0x2000000};
    yellow[17] = (AosYellowSoulRecord){AOS_YELLOW_FLAG_ROUTINE, 0, 0, 0x40000};
    yellow[18] = (AosYellowSoulRecord){AOS_YELLOW_FLAG_ROUTINE, 0, 0, 0x80000};
    yellow[10] = (AosYellowSoulRecord){AOS_YELLOW_GIANT_WORM_ROUTINE, 0, 0, 0};
    yellow[14] = (AosYellowSoulRecord){AOS_YELLOW_HEADHUNTER_ROUTINE, 1, 0, 0};
    yellow[21] = (AosYellowSoulRecord){AOS_YELLOW_RESIST_ROUTINE, 0, 0, 0x100};
    yellow[22] = (AosYellowSoulRecord){AOS_YELLOW_STAT_ROUTINE, 1, 0, 4};
    yellow[29] = (AosYellowSoulRecord){AOS_YELLOW_STAT_ROUTINE, 1, 1, 0x10};
    yellow[30] = (AosYellowSoulRecord){AOS_YELLOW_STAT_ROUTINE, 1, 2, 4};
    AosStats ys = {.yellow_soul = 3, .equipment_atk = 5, .equipment_def = 2,
                   .equipped = {10, 12, 14, 9}, .equipped_resist = 0x2, .flags = 0x100};
    AosYellowSoul yellow_soul = {0};
    aos_stats_frame_reset(&ys);
    assert(ys.atk == 15 && ys.def == 8 && ys.resist == 2);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(yellow_soul.active && ys.flags == (0x100 | 0x400000));
    /* A change clears the 0x13260 bits 14-26 (the 0x100 stays). */
    ys.yellow_soul = 22;
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.flags == 0x100 && ys.current[AOS_STAT_STR] == 14 && ys.atk == 19);
    /* Every frame starts again from the equipped stats. */
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.current[AOS_STAT_STR] == 14 && ys.atk == 19);
    ys.yellow_soul = 29;
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.current[AOS_STAT_CON] == 28 && ys.def == 16 && ys.current[AOS_STAT_STR] == 10);
    ys.yellow_soul = 30;
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.current[AOS_STAT_INT] == 18);
    ys.yellow_soul = 21;
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.resist == 0x102);
    /* Zombie: only while poisoned, 5.5 times STR, CON and INT. */
    ys.yellow_soul = 12;
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.current[AOS_STAT_STR] == 10);
    ys.status = 1;
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.current[AOS_STAT_STR] == 55 && ys.current[AOS_STAT_CON] == 66 &&
           ys.current[AOS_STAT_INT] == 77 && ys.current[AOS_STAT_LCK] == 9 && ys.atk == 60);
    ys.status = 0;
    /* Lubicant at half HP: STR + 5, CON + 6 / 8 = 0, INT + 7. */
    ys.yellow_soul = 13;
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 50, 100);
    assert(ys.current[AOS_STAT_STR] == 15 && ys.current[AOS_STAT_CON] == 12 &&
           ys.current[AOS_STAT_INT] == 21);
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.current[AOS_STAT_STR] == 10);
    /* Erinys sets the experience bonus bit; Mimic turns damage into gold. */
    ys.yellow_soul = 16;
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.flags & AOS_STATS_FLAG_EXPERIENCE_BONUS);
    ys.gold = 999990;
    aos_yellow_soul_hit(&ys, 3);
    assert(ys.gold == 999990);
    ys.yellow_soul = 17;
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(!(ys.flags & AOS_STATS_FLAG_EXPERIENCE_BONUS) && (ys.flags & 0x40000));
    aos_yellow_soul_hit(&ys, 1);
    assert(ys.gold == 999995);
    aos_yellow_soul_hit(&ys, 3);
    assert(ys.gold == 999999);
    /* Arc Demon (soul 18, bit 0x80000): damage * 2 MP, up to the maximum. */
    ys.yellow_soul = 18;
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert((ys.flags & 0x80000) && !(ys.flags & 0x40000));
    ys.max_mp = 50;
    ys.mp = 10;
    aos_yellow_soul_hit(&ys, 7);
    assert(ys.mp == 24 && ys.gold == 999999);
    aos_yellow_soul_hit(&ys, 40);
    assert(ys.mp == 50);
    /* Headhunter (soul 14): 1 + souls / 16 (at most 33) to STR, CON, INT. */
    ys.yellow_soul = 14;
    ys.souls_collected = 100;
    aos_stats_frame_reset(&ys);
    int16_t str0 = ys.current[AOS_STAT_STR], int0 = ys.current[AOS_STAT_INT],
            lck0 = ys.current[AOS_STAT_LCK];
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.current[AOS_STAT_STR] == str0 + 7 && ys.current[AOS_STAT_INT] == int0 + 7 &&
           ys.current[AOS_STAT_LCK] == lck0);
    ys.souls_collected = 999;
    aos_stats_frame_reset(&ys);
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(ys.current[AOS_STAT_STR] == str0 + 33);
    /* Giant Worm (soul 10): standing still below full HP, 5 HP every 61st
     * frame; walking, a door's hold or full HP pause it. */
    ys.yellow_soul = 10;
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    int16_t hp = 50;
    for (int f = 0; f < 60; ++f)
        aos_yellow_soul_regen(&yellow_soul, &ys, yellow, &hp, 100, 0, 0, false);
    assert(hp == 50);
    aos_yellow_soul_regen(&yellow_soul, &ys, yellow, &hp, 100, 0x10000, 0, false);
    aos_yellow_soul_regen(&yellow_soul, &ys, yellow, &hp, 100, 0, 0x20, false);
    aos_yellow_soul_regen(&yellow_soul, &ys, yellow, &hp, 100, 0, 0, true);
    assert(hp == 50);
    aos_yellow_soul_regen(&yellow_soul, &ys, yellow, &hp, 100, 0, 0, false);
    assert(hp == 55 && yellow_soul.timer == 0);
    hp = 98;
    for (int f = 0; f < 61; ++f)
        aos_yellow_soul_regen(&yellow_soul, &ys, yellow, &hp, 100, 0, 0, false);
    assert(hp == 100);
    /* Unequipped: the bits go and nothing runs. */
    ys.yellow_soul = 0;
    aos_yellow_soul_update(&yellow_soul, &ys, yellow, 100, 100);
    assert(!yellow_soul.active && ys.flags == 0x100);
    /* SkillCatoblepasUse (soul 9, held, 15 MP/s): Soma grounded and still,
     * the pose on the first update, a cloud every fifth frame (0x8E with
     * every other), 26 frames each. */
    blue[9] = (AosBlueSoulRecord){0x08030E65, 15, 1, 0, 0};
    aos_audio_clear();
    guard_stats = (AosStats){.blue_soul = 9, .mp = 80, .max_mp = 80};
    guardian = (AosGuardian){0};
    soma_flags = 0x100000;
    AosCatoblepasGuardian cato = {0};
    bool pose = false;
    still = false;
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    assert(aos_catoblepas_guardian_update(&cato, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                          &soma_flags, 100, 200, false, 0x4000, &still, &pose));
    assert(pose && still && cato.x == 116 && cato.y == 172 && cato.timer == 4);
    assert(cato.gases[0].alive && cato.gases[0].vx == 0x18000 + 0x4000 + 0x400 &&
           cato.gases[0].vy == 0x8000 + 0x200 && cato.gases[0].timer == 0x19 &&
           cato.gases[0].x == (116 << 16) + cato.gases[0].vx);
    assert(!aos_audio_requested(AOS_AUDIO_START_SONG, 0x8E));
    int clouds = 1;
    for (int frame = 0; frame < 25; ++frame) {
        assert(aos_catoblepas_guardian_update(&cato, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                              &soma_flags, 100, 200, false, 0, &still, &pose));
        assert(!pose);
        if (frame % 5 == 4) ++clouds;
    }
    int alive = 0;
    for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) alive += cato.gases[k].alive;
    assert(clouds == 6 && alive == 5 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x8E) == 3);
    assert(cato.gases[1].vy == 0x6000 + 0x200 * 21 && cato.gases[2].vy == 0xA000 + 0x200 * 16);
    /* The first cloud lasts 26 frames. */
    assert(!cato.gases[0].alive || cato.gases[0].timer == 0);
    /* Facing left the clouds go left. */
    assert(aos_catoblepas_guardian_update(&cato, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                          &soma_flags, 100, 200, true, 0, &still, &pose));
    assert(cato.x == 84);
    /* Airborne, it ends and its clouds go with it. */
    soma_flags = 0x100002 | 0x200;
    assert(!aos_catoblepas_guardian_update(&cato, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                           &soma_flags, 100, 200, true, 0, &still, &pose));
    assert(!guardian.active && soma_flags == 0x100002 && !cato.gases[1].alive);
    /* SkillBonePillarUse (soul 10, held, 30 MP/s): the same rule and pose;
     * a fire every fifth frame at vy 1.25 rising by 0x1800 a frame, 0x91
     * with the first of every five (the zero random skips the third). */
    blue[10] = (AosBlueSoulRecord){0x0802E731, 30, 1, 0, 0};
    aos_audio_clear();
    guard_stats = (AosStats){.blue_soul = 10, .mp = 80, .max_mp = 80};
    guardian = (AosGuardian){0};
    soma_flags = 0x100000;
    AosCatoblepasGuardian pillar = {0};
    still = pose = false;
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    assert(aos_pillar_guardian_update(&pillar, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                      &soma_flags, 100, 200, false, 0, &still, &pose,
                                      zero_random));
    assert(pose && still && pillar.x == 116 && pillar.y == 172 && pillar.step == 1 &&
           pillar.gases[0].vy == 0x14000 - 0x1800 && pillar.gases[0].vx == 0x18000 + 0x400 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x91) == 1);
    for (int frame = 0; frame < 25; ++frame)
        assert(aos_pillar_guardian_update(&pillar, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                          &soma_flags, 100, 200, false, 0, &still, &pose,
                                          zero_random));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x91) == 2 && pillar.step == 1);
    /* Curly (soul 20, held, 30 MP/s, variant 2): it starts Soma's state 9,
     * attacks once he is transformed, and its end clears 0x13260 bits 8-13. */
    blue[20] = (AosBlueSoulRecord){0x0802EFB1, 30, 1, 0, 2};
    aos_audio_clear();
    guard_stats = (AosStats){.blue_soul = 20, .mp = 80, .max_mp = 80};
    guardian = (AosGuardian){0};
    soma_flags = 0x100000 | 0x20;
    uint32_t abilities = 0x100;
    uint8_t soma_state = 0, soma_step = 7, variant = 0;
    int32_t friction = 0x4000, soma_gravity = 0x1000;
    AosTransformTarget target = {&soma_flags, &abilities, &soma_state, &soma_step, &variant,
                                 &friction, &soma_gravity};
    AosCurlyGuardian curly = {0};
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    /* Attacking (0x20) when it starts: the first update ends it... */
    assert(!aos_curly_guardian_update(&curly, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                      100 << 16, 200 << 16, false, target));
    assert(!guardian.active && soma_state == 0);
    soma_flags = 0x100000;
    assert(aos_guardian_try(&guardian, &guard_stats, blue, AOS_KEY_R, 0, false, &soma_flags));
    assert(aos_curly_guardian_update(&curly, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                     100 << 16, 200 << 16, true, target));
    assert(soma_state == 9 && soma_step == 0 && variant == 2 && friction == 0 &&
           soma_gravity == 0 && (abilities & 0x1000) && curly.x == 100 << 16 &&
           curly.facing_left && !curly.attacking);
    soma_step = 3;
    assert(aos_curly_guardian_update(&curly, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                     100 << 16, 200 << 16, true, target));
    assert(curly.attacking && curly.child && guardian.state == 2 &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 0x1B0) == 1);
    for (int k = 0; k < 7; ++k)
        aos_curly_guardian_update(&curly, &guardian, &guard_stats, blue, AOS_KEY_R, 0,
                                  100 << 16, 200 << 16, true, target);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x93) == 1);
    /* Released: ended, bits 8-13 cleared (0x1000 with them). */
    assert(!aos_curly_guardian_update(&curly, &guardian, &guard_stats, blue, 0, 0, 100 << 16,
                                      200 << 16, true, target));
    assert(!guardian.active && abilities == 0 && !curly.child && !(soma_flags & 0x200));
    /* sub_08021994's statuses: poison quarters STR, CON and INT (Zombie's
     * soul then multiplies them by 5.5); curse drains max MP / 128 every
     * 4 frames instead of regenerating; both run out. */
    AosStats st = {.max_mp = 256, .mp = 100, .status = 3,
                   .status_timers = {2, 5, 0}, .equipped = {40, 20, 12, 9}};
    aos_stats_frame_reset(&st);
    aos_stats_status_update(&st, 0, 0);
    assert(st.current[AOS_STAT_STR] == 10 && st.current[AOS_STAT_CON] == 5 &&
           st.current[AOS_STAT_INT] == 3 && st.current[AOS_STAT_LCK] == 9);
    assert(st.status_timers[0] == 1 && st.status_timers[1] == 4 && st.mp == 98);
    aos_stats_status_update(&st, 0, 0);
    assert(st.status == 2 && st.status_timers[1] == 3 && st.mp == 98);
    for (int k = 0; k < 3; ++k) aos_stats_status_update(&st, 0, 0);
    assert(st.status == 0 && st.status_timers[1] == 0);
    /* Zombie's soul on a poisoned Soma: 40 / 4 = 10, then 10 * 5.5. */
    AosYellowSoul zs = {0};
    AosStats zp = {.yellow_soul = 12, .status = 1, .status_timers = {100, 0, 0},
                   .equipped = {40, 20, 12, 9}};
    aos_stats_frame_reset(&zp);
    aos_stats_status_update(&zp, 0, 0);
    aos_yellow_soul_update(&zs, &zp, yellow, 100, 100);
    assert(zp.current[AOS_STAT_STR] == 55);
    puts("Aria red, blue and yellow soul tests passed");
    return 0;
}
