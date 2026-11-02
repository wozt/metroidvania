/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul.h"

#include "aos_audio.h"

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

enum { SOMA_GUARDIAN = 0x200, MEDUSA_AIRBORNE_MASK = 0x01008082 };

bool aos_guardian_try(AosGuardian *guardian, const AosStats *stats,
                      const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS], uint16_t held,
                      uint16_t pressed, bool locked, uint32_t *soma_flags) {
    if (locked || guardian->active || !(held & AOS_KEY_R) ||
        stats->blue_soul >= AOS_BLUE_SOUL_RECORDS)
        return false;
    const AosBlueSoulRecord *record = &records[stats->blue_soul];
    if ((record->flags & AOS_BLUE_SOUL_TOGGLE) && !(pressed & AOS_KEY_R)) return false;
    if (!record->routine || !record->mp_per_second) return false;
    if (stats->mp < (int32_t)(stats->max_mp >> 7) + 1) return false;
    *guardian = (AosGuardian){.active = true, .soul = stats->blue_soul,
                              .interval = (uint8_t)(60 / record->mp_per_second)};
    *soma_flags |= SOMA_GUARDIAN;
    return true;
}

bool aos_guardian_upkeep(AosGuardian *guardian, AosStats *stats,
                         const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS], uint16_t held,
                         uint16_t pressed) {
    const AosBlueSoulRecord *record = &records[guardian->soul];
    guardian->ticks++;
    if (guardian->interval && guardian->ticks % guardian->interval == 0) {
        stats->mp--;
        if (stats->mp <= 0) {
            stats->mp = 0;
            return true;
        }
    }
    if (!(record->flags & AOS_BLUE_SOUL_TOGGLE)) return !(held & AOS_KEY_R);
    return guardian->state != 0 && (pressed & AOS_KEY_R);
}

void aos_guardian_end(AosGuardian *guardian, uint32_t *soma_flags) {
    guardian->active = false;
    *soma_flags &= ~(uint32_t)SOMA_GUARDIAN;
}

bool aos_medusa_guardian_update(AosMedusaGuardian *medusa, AosGuardian *guardian,
                                AosStats *stats,
                                const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                int32_t *soma_vy, int32_t *soma_gravity, bool *hold_still) {
    if (guardian->state == 0) {
        if ((*soma_flags & MEDUSA_AIRBORNE_MASK) != 2) {
            aos_guardian_end(guardian, soma_flags);
            return false;
        }
        medusa->vy = *soma_vy < 0 ? 0 : *soma_vy;
        medusa->gravity = 0x2800;
        aos_audio_play_song(0x8C);
        guardian->state = 1;
    }
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed)) {
        aos_guardian_end(guardian, soma_flags);
        *soma_vy = medusa->vy;
        *soma_gravity = medusa->gravity;
        return false;
    }
    if ((*soma_flags & MEDUSA_AIRBORNE_MASK) != 2) {
        *soma_vy = medusa->vy;
        *soma_gravity = medusa->gravity;
        aos_guardian_end(guardian, soma_flags);
        return false;
    }
    *hold_still = true;
    *soma_vy = 0;
    *soma_gravity = 0;
    return true;
}

int32_t aos_guardian_power(const AosStats *stats, uint16_t power, bool reduced) {
    uint32_t value = power + (uint32_t)(power >> 3) * (uint32_t)(stats->current[AOS_STAT_INT] >> 1);
    if (reduced) value = value * 8 / 10;
    return (int32_t)(uint16_t)value;
}

bool aos_cagnazzo_guardian_update(AosCagnazzoGuardian *cagnazzo, AosGuardian *guardian,
                                  AosStats *stats,
                                  const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                  uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                  int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed)) {
        aos_guardian_end(guardian, soma_flags);
        return false;
    }
    if (guardian->state == 0) {
        aos_audio_play_song(0x1AA);
        guardian->state = 1;
    }
    cagnazzo->facing_left = soma_facing_left;
    cagnazzo->x = (int16_t)(soma_x + (soma_facing_left ? -0x10 : 0x10));
    cagnazzo->y = (int16_t)(soma_y - 0x20);
    return true;
}
