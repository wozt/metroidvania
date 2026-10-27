/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_stats.h"

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
