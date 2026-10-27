/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_stats.h"

#include <stddef.h>

/* sUnk_080E1DCC, sUnk_080E1DE0, sUnk_080E1DF4 and sUnk_080E1E08
 * (src/code_08033CAC.c), indexed by level / 5. */
static const uint8_t str_gain[20] = {1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                     3, 3, 2, 2, 1, 1, 1, 1, 1, 1};
static const uint8_t con_gain[20] = {1, 1, 1, 2, 2, 2, 2, 3, 3, 2,
                                     2, 2, 2, 1, 1, 1, 1, 1, 1, 1};
static const uint8_t int_gain[20] = {1, 1, 2, 2, 2, 2, 2, 2, 3, 3,
                                     3, 2, 2, 2, 1, 1, 1, 1, 1, 1};
static const uint8_t mp_gain[20] = {5, 5, 6, 8, 10, 11, 12, 12, 13, 13,
                                    13, 14, 15, 16, 16, 16, 17, 18, 19, 20};

void aos_stats_init(AosStats *stats) {
    stats->level = 1;
    if (stats->character == 0) {
        stats->max_hp = 320;
        stats->max_mp = 80;
        stats->base[AOS_STAT_STR] = 10;
        stats->base[AOS_STAT_CON] = 12;
        stats->base[AOS_STAT_INT] = 11;
        stats->base[AOS_STAT_LCK] = 9;
    } else {
        stats->max_hp = 800;
        stats->max_mp = 500;
        stats->base[AOS_STAT_STR] = 15;
        stats->base[AOS_STAT_CON] = 12;
        stats->base[AOS_STAT_INT] = 10;
        stats->base[AOS_STAT_LCK] = 20;
    }
}

int32_t aos_stats_experience_for_level(int32_t level) {
    return level * (level + 1) * (level * 2 + 8 + level);
}

int32_t aos_stats_level_up(AosStats *stats) {
    int32_t gained = 0;
    while (stats->level < AOS_STATS_MAX_LEVEL &&
           (uint32_t)aos_stats_experience_for_level(stats->level + 1) <= stats->experience) {
        unsigned band = stats->level / 5;
        stats->max_hp = (uint16_t)(stats->max_hp + 12);
        stats->max_mp = (uint16_t)(stats->max_mp + mp_gain[band]);
        stats->base[AOS_STAT_STR] = (uint16_t)(stats->base[AOS_STAT_STR] + str_gain[band]);
        stats->base[AOS_STAT_CON] = (uint16_t)(stats->base[AOS_STAT_CON] + con_gain[band]);
        /* Every fourth level gains a single INT point instead. */
        stats->base[AOS_STAT_INT] = (uint16_t)(stats->base[AOS_STAT_INT] +
                                               (stats->level & 3 ? int_gain[band] : 1));
        stats->base[AOS_STAT_LCK]++;
        stats->level++;
        gained++;
    }
    return gained;
}

int32_t aos_stats_experience_to_next(const AosStats *stats) {
    if (stats->level >= AOS_STATS_MAX_LEVEL) return 0;
    return aos_stats_experience_for_level(stats->level + 1) - (int32_t)stats->experience;
}

void aos_stats_set_level(AosStats *stats, int32_t level) {
    aos_stats_init(stats);
    stats->experience = (uint32_t)aos_stats_experience_for_level(level);
    aos_stats_level_up(stats);
}

void aos_stats_julius_bonus(AosStats *stats) {
    if (stats->character == 0) return;
    stats->base[AOS_STAT_STR] = (uint16_t)(stats->base[AOS_STAT_STR] + 25);
    stats->base[AOS_STAT_CON] = (uint16_t)(stats->base[AOS_STAT_CON] + 20);
    stats->base[AOS_STAT_INT] = (uint16_t)(stats->base[AOS_STAT_INT] + 18);
}

int32_t aos_stats_reward_enemy(AosStats *stats, uint8_t *entity_flags, uint8_t mode,
                               uint8_t enemy_id, uint16_t experience) {
    if (*entity_flags & AOS_ENEMY_REWARDED) return -1;
    *entity_flags |= AOS_ENEMY_REWARDED;
    if ((mode & 3) || stats->character != 0) return -1;
    if ((unsigned)(enemy_id >> 5) < AOS_STATS_ENEMY_WORDS)
        stats->defeated[enemy_id >> 5] |= 1u << (enemy_id & 31);
    uint32_t gain = experience;
    if (stats->flags & AOS_STATS_FLAG_EXPERIENCE_BONUS) gain = gain * 0x133 >> 8;
    if (!gain) return 0;
    stats->experience += gain;
    if (stats->experience > AOS_STATS_MAX_EXPERIENCE)
        stats->experience = AOS_STATS_MAX_EXPERIENCE;
    return aos_stats_level_up(stats);
}

bool aos_stats_enemy_defeated(const AosStats *stats, uint8_t enemy_id) {
    return (unsigned)(enemy_id >> 5) < AOS_STATS_ENEMY_WORDS &&
           (stats->defeated[enemy_id >> 5] >> (enemy_id & 31) & 1u);
}

/* (s16)value >> 4, the arithmetic shift of the native luck term. */
static int32_t luck_term(int16_t luck) { return luck >> 4; }

AosDrop aos_stats_roll_drop(AosStats *stats, uint8_t enemy_id, const AosEnemyDrops *record,
                            const AosDropContext *context, uint32_t (*random)(void)) {
    AosDrop drop = {AOS_DROP_NONE, 0, 0, 0, false, 0};
    if (record->soul_rarity && record->soul_index) {
        uint32_t chance = 3;
        if (!context->soul_owned) chance = context->soul_flag_a1 ? 7 : 6;
        if (context->accessory == 0x28) chance += 8;
        int32_t range = record->soul_rarity * 8 - (luck_term(context->luck) - 0x20);
        if (range <= 15) range = 16;
        if ((random() >> 2) % (uint32_t)range < chance) {
            drop.kind = AOS_DROP_SOUL;
            drop.soul_type = record->soul_type;
            drop.soul_index = (uint8_t)(record->soul_index - 1);
            return drop;
        }
    }
    /* A non-zero ground type without bit 3 refuses every other drop. */
    if (context->ground && !(context->ground & 8)) return drop;
    bool rare = (random() >> 1 & 0xFF) < (context->accessory == 0x27 ? 0x60u : 0x40u);
    uint16_t item = rare ? record->rare_item : record->common_item;
    if (item) {
        uint32_t chance = context->accessory == 0x27 ? 8 : 4;
        int32_t range = (rare ? record->rare_rate : record->common_rate) * 4 -
                        (luck_term(context->luck) - 0x10);
        if (range <= 15) range = 16;
        if ((random() >> 3) % (uint32_t)range < chance) {
            drop.kind = AOS_DROP_ITEM;
            drop.item = item;
            drop.rare = rare;
            if ((unsigned)(enemy_id >> 5) < AOS_STATS_ENEMY_WORDS)
                stats->dropped[rare][enemy_id >> 5] |= 1u << (enemy_id & 31);
            return drop;
        }
    }
    if (!(record->common_item | record->rare_item)) return drop;
    uint8_t base = context->accessory == 0x2B ? 3 : 0;
    uint32_t roll = random() << 19 >> 22;
    if (roll <= (base ? 0x3Fu : 7u)) {
        drop.kind = AOS_DROP_PICKUP_43F70;
        drop.variant = (uint8_t)(base + (random() >> 1 & 3));
        return drop;
    }
    roll = random() << 18 >> 22;
    if (roll <= (context->accessory == 0x1D ? 0x3Fu : 7u)) {
        drop.kind = AOS_DROP_PICKUP_43C10;
        drop.variant = (random() & 0xC0) == 0;
    }
    return drop;
}

void aos_stats_new_game_equipment(AosStats *stats) {
    stats->weapon = 0;
    stats->armor = 0;
    stats->accessory = AOS_EQUIPMENT_NONE;
}

void aos_stats_refresh(AosStats *stats, const AosEquipmentTables *tables) {
    int16_t atk = 0, def = 0, extra[AOS_STAT_COUNT] = {0};
    uint16_t resist = 0;
    const AosEquipmentRecord *records[3] = {
        stats->weapon < AOS_WEAPON_RECORDS ? &tables->weapons[stats->weapon] : NULL,
        stats->armor < AOS_ARMOR_RECORDS ? &tables->armor[stats->armor] : NULL,
        stats->accessory < AOS_ARMOR_RECORDS ? &tables->armor[stats->accessory] : NULL,
    };
    for (int slot = 0; slot < 3; ++slot) {
        const AosEquipmentRecord *record = records[slot];
        if (!record) continue;
        atk = (int16_t)(atk + record->atk);
        def = (int16_t)(def + record->def);
        for (int stat = 0; stat < AOS_STAT_COUNT; ++stat)
            extra[stat] = (int16_t)(extra[stat] + record->stats[stat]);
        if (slot) resist |= record->resist;
    }
    stats->equipment_atk = atk;
    stats->equipment_def = def;
    for (int stat = 0; stat < AOS_STAT_COUNT; ++stat)
        stats->current[stat] = (int16_t)(extra[stat] + stats->base[stat]);
    stats->resist = resist;
    stats->atk = (int16_t)(stats->current[AOS_STAT_STR] + atk);
    stats->def = (int16_t)((stats->current[AOS_STAT_CON] >> 1) + def);
    if (stats->atk < 0) stats->atk = 0;
    if (stats->def < 0) stats->def = 0;
}

void aos_stats_new_game_inventory(AosStats *stats) {
    stats->weapon_counts[0] = 1;
    stats->armor_counts[0] = 1;
}

/* The array a soul type uses for a group (the switch of each routine). */
static uint8_t *soul_group(AosStats *stats, uint8_t group, int32_t type) {
    switch (type) {
    case AOS_SOUL_RED: return stats->red_souls[group];
    case AOS_SOUL_BLUE: return stats->blue_souls[group];
    case AOS_SOUL_YELLOW: return stats->yellow_souls[group];
    case AOS_SOUL_ABILITY: return stats->ability_souls;
    default: return NULL;
    }
}

static uint8_t nibble(const uint8_t *souls, int32_t index) {
    uint8_t value = souls[index >> 1];
    return index & 1 ? (value >> 4) & 0xF : value & 0xF;
}

void aos_souls_add_group(AosStats *stats, uint8_t group, int32_t type, int32_t index,
                         int32_t amount) {
    uint8_t *souls = soul_group(stats, group, type);
    int8_t base = souls ? (int8_t)nibble(souls, index) : 0;
    int8_t total = (int8_t)(base + amount);
    if (total < 0) total = 0;
    else if (total > 9) total = 9;
    if (!souls) return;
    uint8_t *cell = &souls[index >> 1];
    *cell = index & 1 ? (uint8_t)((*cell & 0x0F) | total << 4)
                      : (uint8_t)((*cell & 0xF0) | total);
}

uint8_t aos_souls_total(const AosStats *stats, int32_t type, int32_t index) {
    AosStats *view = (AosStats *)stats;
    const uint8_t *first = soul_group(view, 0, type), *second = soul_group(view, 1, type);
    return (uint8_t)((first ? nibble(first, index) : 0) + (second ? nibble(second, index) : 0));
}

void aos_souls_add(AosStats *stats, int32_t type, int32_t index, int32_t amount) {
    const uint8_t *second = soul_group(stats, 1, type);
    uint8_t held = second ? nibble(second, index) : 0;
    if (held && amount > 0 && (int16_t)(aos_souls_total(stats, type, index) + amount) > 9)
        aos_souls_add_group(stats, 1, type, index, -amount);
    aos_souls_add_group(stats, 0, type, index, amount);
}

void aos_souls_set_ability(AosStats *stats, int32_t index, bool enabled) {
    uint8_t *bits = &stats->abilities[index >> 3];
    if (enabled) *bits |= (uint8_t)(1u << (index & 7));
    else *bits &= (uint8_t)~(1u << (index & 7));
}

void aos_souls_count(AosStats *stats, int32_t count) {
    if (count <= 0) return;
    uint32_t total = stats->souls_collected + (uint32_t)count;
    stats->souls_collected = (uint16_t)(total > 999 ? 999 : total);
}
