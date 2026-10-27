/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* Records 2 and 3 as on the ROM: the bat's (5 MP, 3 at once) and
     * another with a skill (8 MP, 3 at once); record 4 has no skill. */
    AosRedSoulRecord records[AOS_RED_SOUL_RECORDS] = {{0}};
    records[2] = (AosRedSoulRecord){0x08026C55, 0, 5, 3, 0};
    records[3] = (AosRedSoulRecord){0x08025785, 0, 8, 3, 3};
    records[5] = (AosRedSoulRecord){0x08024275, 0, 22, 1, 6};
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
    puts("Aria red soul tests passed");
    return 0;
}
