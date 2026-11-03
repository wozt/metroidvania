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

bool aos_yellow_soul_ported(uint32_t routine) {
    switch (routine) {
    case AOS_YELLOW_FLAG_ROUTINE: case AOS_YELLOW_STAT_ROUTINE:
    case AOS_YELLOW_RESIST_ROUTINE: case AOS_YELLOW_ZOMBIE_ROUTINE:
    case AOS_YELLOW_LUBICANT_ROUTINE: case AOS_YELLOW_WOODEN_GOLEM_ROUTINE:
    case AOS_YELLOW_BASILISK_ROUTINE: case AOS_YELLOW_DOUBLE_ROUTINE:
        return true;
    default:
        return false;
    }
}

void aos_yellow_soul_update(AosYellowSoul *soul, AosStats *stats,
                            const AosYellowSoulRecord records[AOS_YELLOW_SOUL_RECORDS],
                            int16_t hp, int16_t max_hp) {
    if (soul->active && soul->soul != stats->yellow_soul) {
        stats->flags &= ~(uint32_t)AOS_YELLOW_FLAGS_MASK;
        soul->active = false;
    }
    if (stats->yellow_soul >= AOS_YELLOW_SOUL_RECORDS) return;
    const AosYellowSoulRecord *record = &records[stats->yellow_soul];
    if (!soul->active) {
        if (!record->routine) return;
        *soul = (AosYellowSoul){.active = true, .soul = stats->yellow_soul};
    }
    int16_t *current = stats->current;
    switch (record->routine) {
    case AOS_YELLOW_FLAG_ROUTINE:
        stats->flags |= record->argument;
        break;
    case AOS_YELLOW_STAT_ROUTINE:
        if (record->parameter < AOS_STAT_COUNT)
            current[record->parameter] = (int16_t)(current[record->parameter] + record->argument);
        aos_stats_derive(stats);
        break;
    case AOS_YELLOW_RESIST_ROUTINE:
        stats->resist |= (uint16_t)record->argument;
        break;
    case AOS_YELLOW_ZOMBIE_ROUTINE:
        if (stats->status & 1) {
            for (int stat = AOS_STAT_STR; stat <= AOS_STAT_INT; ++stat) {
                int16_t v = current[stat];
                current[stat] = (int16_t)(v + (int16_t)((v << 2) + (v >> 1)));
            }
            aos_stats_derive(stats);
        }
        break;
    case AOS_YELLOW_LUBICANT_ROUTINE: {
        int32_t lost = ((int32_t)(uint16_t)max_hp - hp) << 8, whole = (int32_t)(uint16_t)max_hp << 8;
        if (whole) {
            current[AOS_STAT_STR] = (int16_t)(current[AOS_STAT_STR] +
                                              current[AOS_STAT_STR] * lost / whole);
            current[AOS_STAT_CON] = (int16_t)(current[AOS_STAT_CON] +
                                              ((current[AOS_STAT_CON] * lost / whole) >> 3));
            current[AOS_STAT_INT] = (int16_t)(current[AOS_STAT_INT] +
                                              current[AOS_STAT_INT] * lost / whole);
        }
        aos_stats_derive(stats);
        break;
    }
    case AOS_YELLOW_WOODEN_GOLEM_ROUTINE:
        if ((++soul->timer & 0x1F) == 0) {
            stats->mp++;
            if (stats->mp > (int16_t)stats->max_mp) stats->mp = (int16_t)stats->max_mp;
        }
        break;
    case AOS_YELLOW_BASILISK_ROUTINE:
        stats->def = (int16_t)(stats->def + stats->def / 4);
        stats->atk = (int16_t)(stats->atk - stats->atk / 2);
        break;
    case AOS_YELLOW_DOUBLE_ROUTINE:
        if (record->argument == 0) stats->atk = (int16_t)(stats->atk * 2);
        else stats->def = (int16_t)(stats->def * 2);
        break;
    default:
        break;
    }
}

void aos_yellow_soul_hit(AosStats *stats, int damage) {
    if (damage <= 0) return;
    if (stats->flags & 0x40000u) {
        uint32_t gold = stats->gold + (uint32_t)damage * 5u;
        stats->gold = gold > 999999u ? 999999u : gold;
    }
    if (stats->flags & 0x80000u) {
        /* Arc Demon: sub_08022028(damage * 2), at least 1, up to max MP. */
        int gain = damage * 2;
        int mp = stats->mp + (gain ? gain : 1);
        stats->mp = (int16_t)(mp > (int)stats->max_mp ? stats->max_mp : mp);
    }
}

/* sub_0806D3D8 with its 8.0 clamp. */
static void gas_move(AosCatoblepasGas *gas) {
    const int32_t cap = 0x80000;
    gas->vx += gas->ax;
    if (gas->ax >= 0 ? gas->vx > cap : gas->vx < -cap) gas->vx = gas->ax >= 0 ? cap : -cap;
    gas->vy += gas->ay;
    if (gas->ay >= 0 ? gas->vy > cap : gas->vy < -cap) gas->vy = gas->ay >= 0 ? cap : -cap;
    gas->x += gas->vx;
    gas->y += gas->vy;
}

bool aos_catoblepas_guardian_update(AosCatoblepasGuardian *catoblepas, AosGuardian *guardian,
                                    AosStats *stats,
                                    const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                    uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                    int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                    int32_t soma_vx, bool *hold_still, bool *pose) {
    static const int32_t rise[3] = {0, (int32_t)0xFFFFE000, 0x2000};   /* 0x080E1A64 */
    *pose = false;
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed) ||
        (*soma_flags & 0x1001A2u) != 0x100000u) {
        aos_guardian_end(guardian, soma_flags);
        for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) catoblepas->gases[k].alive = false;
        return false;
    }
    *hold_still = true;
    if (guardian->state == 0) {
        *pose = true;
        guardian->state = 1;
    }
    catoblepas->facing_left = soma_facing_left;
    catoblepas->x = (int16_t)(soma_x + (soma_facing_left ? -0x10 : 0x10));
    catoblepas->y = (int16_t)(soma_y - 0x1C);
    if (catoblepas->timer == 0) {
        AosCatoblepasGas *gas = NULL;
        for (int k = 0; k < AOS_CATOBLEPAS_GASES && !gas; ++k)
            if (!catoblepas->gases[k].alive) gas = &catoblepas->gases[k];
        if (gas) {
            if (catoblepas->sound) aos_audio_play_song(0x8E);
            catoblepas->sound = !catoblepas->sound;
            bool left = catoblepas->facing_left;
            *gas = (AosCatoblepasGas){
                .alive = true, .facing_left = left,
                .x = (int32_t)((uint32_t)(uint16_t)catoblepas->x << 16),
                .y = (int32_t)((uint32_t)(uint16_t)catoblepas->y << 16),
                .vx = (left ? -0x18000 : 0x18000) + soma_vx, .ax = left ? -0x400 : 0x400,
                .vy = rise[catoblepas->cycle] + 0x8000, .ay = 0x200, .timer = 0x1A};
        }
        if (++catoblepas->cycle > 2) catoblepas->cycle = 0;
        catoblepas->timer = 5;
    }
    catoblepas->timer--;
    /* sub_08030D6C: each cloud moves, then counts its 26 frames down. */
    for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) {
        AosCatoblepasGas *gas = &catoblepas->gases[k];
        if (!gas->alive) continue;
        gas_move(gas);
        if (--gas->timer == 0) gas->alive = false;
    }
    return true;
}

bool aos_pillar_guardian_update(AosCatoblepasGuardian *pillar, AosGuardian *guardian,
                                AosStats *stats,
                                const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                int32_t soma_vx, bool *hold_still, bool *pose,
                                uint32_t (*random)(void)) {
    static const int32_t rise[3] = {0, (int32_t)0xFFFFE000, 0x2000};   /* 0x080E1A64 */
    *pose = false;
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed) ||
        (*soma_flags & 0x1001A2u) != 0x100000u) {
        aos_guardian_end(guardian, soma_flags);
        for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) pillar->gases[k].alive = false;
        return false;
    }
    *hold_still = true;
    if (guardian->state == 0) {
        *pose = true;
        guardian->state = 1;
    }
    pillar->facing_left = soma_facing_left;
    pillar->x = (int16_t)(soma_x + (soma_facing_left ? -0x10 : 0x10));
    pillar->y = (int16_t)(soma_y - 0x1C);
    if (pillar->timer == 0) {
        AosCatoblepasGas *fire = NULL;
        for (int k = 0; k < AOS_CATOBLEPAS_GASES && !fire; ++k)
            if (!pillar->gases[k].alive) fire = &pillar->gases[k];
        if (fire) {
            if (pillar->step == 0 || (pillar->step == 2 && (random() & 1)))
                aos_audio_play_song(0x91);
            if (++pillar->step > 4) pillar->step = 0;
            bool left = pillar->facing_left;
            *fire = (AosCatoblepasGas){
                .alive = true, .facing_left = left,
                .x = (int32_t)((uint32_t)(uint16_t)pillar->x << 16),
                .y = (int32_t)((uint32_t)(uint16_t)pillar->y << 16),
                .vx = (left ? -0x18000 : 0x18000) + soma_vx, .ax = left ? -0x400 : 0x400,
                .vy = rise[pillar->cycle] + 0x14000, .ay = -0x1800, .timer = 0x1A};
        }
        if (++pillar->cycle > 2) pillar->cycle = 0;
        pillar->timer = 5;
    }
    pillar->timer--;
    /* sub_0802E638: each fire moves, then counts its 26 frames down. */
    for (int k = 0; k < AOS_CATOBLEPAS_GASES; ++k) {
        AosCatoblepasGas *fire = &pillar->gases[k];
        if (!fire->alive) continue;
        gas_move(fire);
        if (--fire->timer == 0) fire->alive = false;
    }
    return true;
}

void aos_curly_guardian_end(AosCurlyGuardian *curly, AosGuardian *guardian, uint32_t *soma_flags,
                            uint32_t *abilities) {
    aos_guardian_end(guardian, soma_flags);
    *abilities &= ~(uint32_t)AOS_TRANSFORM_FLAGS;
    curly->child = curly->attacking = false;
}

bool aos_curly_guardian_update(AosCurlyGuardian *curly, AosGuardian *guardian, AosStats *stats,
                               const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                               uint16_t held, uint16_t pressed, int32_t soma_x, int32_t soma_y,
                               bool soma_facing_left, AosTransformTarget soma) {
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed) ||
        (*soma.flags & 0x81A2u)) {
        aos_curly_guardian_end(curly, guardian, soma.flags, soma.abilities);
        return false;
    }
    curly->facing_left = soma_facing_left;
    curly->x = (int32_t)((uint32_t)(uint16_t)(soma_x >> 16) << 16);
    curly->y = (int32_t)((uint32_t)(uint16_t)(soma_y >> 16) << 16);
    if (guardian->state == 0) {
        *soma.friction = 0;
        *soma.gravity = 0;
        *soma.flags &= ~0x20u;
        guardian->state = 1;
        *soma.step = 0;
        *soma.variant = (uint8_t)records[guardian->soul].argument;
    } else if (guardian->state == 1) {
        if (*soma.step == 3) {
            curly->child = true;
            curly->child_timer = 0;
            curly->attacking = true;
            aos_audio_play_song(0x1B0);
            guardian->state = 2;
        }
    } else if (guardian->state == 2) {
        curly->timer++;
    }
    *soma.state = 9;
    *soma.abilities |= 0x1000u;
    /* sub_0802EE40: the child follows Soma, 0x93 every eighth frame. */
    if (curly->child && (++curly->child_timer & 7) == 0) aos_audio_play_song(0x93);
    return true;
}

void aos_stats_status_update(AosStats *stats, uint16_t pressed, uint16_t ticks) {
    if (stats->status_timers[2]) {
        /* Presses of A, B and the D-pad (gEwramData + 0x1E & 0xF3). */
        uint16_t keys = pressed & 0xF3;
        int count = 0;
        for (int bit = 0; bit < 8; ++bit) count += (keys >> bit) & 1;
        if (stats->status_timers[2] & count & 1) aos_audio_play_song(0xB0);
        stats->status_timers[2] = (int16_t)(stats->status_timers[2] - count);
        if (stats->status_timers[2] <= 0) {
            stats->status_timers[2] = 0;
            stats->status &= 0xFFFB;
            aos_audio_play_song(0xAF);
        }
    }
    for (int bit = 1; bit >= 0; --bit) {
        if (stats->status_timers[bit] && --stats->status_timers[bit] <= 0)
            stats->status &= (uint16_t)~(1u << bit);
    }
    if (stats->status & 1) {
        stats->current[AOS_STAT_STR] = (int16_t)(stats->current[AOS_STAT_STR] >> 2);
        stats->current[AOS_STAT_CON] = (int16_t)(stats->current[AOS_STAT_CON] >> 2);
        stats->current[AOS_STAT_INT] = (int16_t)(stats->current[AOS_STAT_INT] >> 2);
    }
    if (stats->status & 2) {
        if ((stats->status_timers[1] & 3) == 0) {
            int drain = stats->max_mp >> 7;
            if (!drain) drain = 1;
            stats->mp = (int16_t)(stats->mp - drain);
            if (stats->mp < 0) stats->mp = 0;
        }
        if (stats->accessory == 0x2C) stats->mp = (int16_t)stats->max_mp;
    } else {
        aos_stats_mp_regen(stats, ticks);
    }
}
