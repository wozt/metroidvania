/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_STATS_H
#define AOS_STATS_H
#include <stdbool.h>
#include <stdint.h>

/* The level fields of gEwramData->unk_1325C (include/structs/ewram.h) and
 * the bestiary bitset at 0x133A0. */
enum {
    AOS_STAT_STR,
    AOS_STAT_CON,
    AOS_STAT_INT,
    AOS_STAT_LCK,
    AOS_STAT_COUNT,
    AOS_STATS_MAX_LEVEL = 99,
    AOS_STATS_MAX_EXPERIENCE = 99999999,
    AOS_STATS_ENEMY_WORDS = 4,
    /* unk_13260 bit 25 scales enemy experience by 0x133 / 256. */
    AOS_STATS_FLAG_EXPERIENCE_BONUS = 0x2000000,
    /* The guard bit of an enemy entity's byte 0x3E. */
    AOS_ENEMY_REWARDED = 2,
};

typedef struct {
    uint8_t character; /* 0 Soma, otherwise Julius */
    uint8_t level;
    uint16_t max_hp, max_mp;
    uint16_t base[AOS_STAT_COUNT];
    uint32_t experience;
    uint32_t gold;
    uint32_t flags; /* unk_13260 */
    uint32_t defeated[AOS_STATS_ENEMY_WORDS];
} AosStats;

/* sub_08033DA4: level 1 and the character's starting HP, MP and STR, CON,
 * INT, LCK. Experience, gold and the bestiary are kept. */
void aos_stats_init(AosStats *stats);
/* sub_08033DFC: the experience a level requires, x(x + 1)(3x + 8). */
int32_t aos_stats_experience_for_level(int32_t level);
/* sub_08033CAC: applies every level the experience reaches, up to 99, with
 * its HP (+12), MP, STR, CON, INT and LCK gains. Returns the levels gained.
 * The derived-stat refresh it ends with (sub_0804AD9C, equipment) is the
 * caller's. */
int32_t aos_stats_level_up(AosStats *stats);
/* sub_08033E10: experience still needed for the next level, 0 at 99. */
int32_t aos_stats_experience_to_next(const AosStats *stats);
/* sub_08033E38: restarts at level 1 with the experience of `level`, then
 * levels up to it. */
void aos_stats_set_level(AosStats *stats, int32_t level);
/* sub_08033E6C: Julius' STR +25, CON +20, INT +18. */
void aos_stats_julius_bonus(AosStats *stats);

/* The experience part of sub_080683BC, run once per defeated enemy:
 * `entity_flags` is the entity's byte 0x3E (its rewarded bit is set on the
 * first call; a second call does nothing), `mode` gEwramData + 0x4CC (bits
 * 0-1 skip the reward, as do Julius' modes), `experience` the enemy record's
 * u16 at 0x10. Marks the enemy in the bestiary, adds the (possibly scaled)
 * experience capped at 99999999 and levels up. Returns the levels gained, or
 * -1 when no reward was given. */
int32_t aos_stats_reward_enemy(AosStats *stats, uint8_t *entity_flags, uint8_t mode,
                               uint8_t enemy_id, uint16_t experience);
/* Whether the bestiary bit of an enemy is set. */
bool aos_stats_enemy_defeated(const AosStats *stats, uint8_t enemy_id);
#endif
