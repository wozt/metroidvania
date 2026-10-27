/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul.h"

/* 0x080E1918: the ground and air cast animations of the four poses. */
static const uint8_t cast_animations[4][2] = {{22, 49}, {78, 78}, {79, 79}, {80, 80}};

int aos_red_soul_try(AosRedSoulCaster *caster, AosStats *stats,
                     const AosRedSoulRecord records[AOS_RED_SOUL_RECORDS], bool locked) {
    if (locked || stats->red_soul >= AOS_RED_SOUL_RECORDS) return -1;
    const AosRedSoulRecord *record = &records[stats->red_soul];
    if (caster->active == 0 && caster->last_used != stats->red_soul)
        caster->last_used = stats->red_soul;
    if (caster->last_used != stats->red_soul || !record->skill) return -1;
    if (record->mp > stats->mp) return -1;
    /* sub_0802D2BC reads the last used soul's record. */
    if (records[caster->last_used].maximum <= caster->active) return -1;
    int slot = -1;
    for (int i = 0; i < AOS_RED_SOUL_SLOTS && slot < 0; ++i)
        if (!caster->slots[i]) slot = i;
    if (slot < 0) return -1;
    stats->mp = (int16_t)(stats->mp - record->mp);
    caster->slots[slot] = true;
    return slot;
}

uint8_t aos_red_soul_cast_animation(uint16_t pose, bool airborne) {
    return pose < 4 ? cast_animations[pose][airborne] : cast_animations[0][airborne];
}

void aos_red_soul_release(AosRedSoulCaster *caster, int slot) {
    if (slot < 0 || slot >= AOS_RED_SOUL_SLOTS || !caster->slots[slot]) return;
    caster->slots[slot] = false;
    if (caster->active) caster->active--;
}

void aos_stats_mp_regen(AosStats *stats, uint16_t ticks) {
    if (stats->accessory == 0x2C) {
        stats->mp = (int16_t)stats->max_mp;
        return;
    }
    uint16_t mask = stats->accessory == 0x29 ? 0xF : 0x3F;
    if (ticks & mask) return;
    /* sub_08022028(1). */
    stats->mp++;
    if (stats->mp > (int16_t)stats->max_mp) stats->mp = (int16_t)stats->max_mp;
}

int32_t aos_red_soul_power(const AosStats *stats, const AosRedSoulRecord *record, bool reduced) {
    uint32_t power = ((uint32_t)(stats->current[AOS_STAT_STR] * record->power) / 10u) +
                     (uint32_t)(stats->current[AOS_STAT_INT] >> 2) + record->power * 4u +
                     record->power * 2u;
    power /= 16;
    if (reduced) power = power * 8 / 10;
    return (int32_t)power;
}
