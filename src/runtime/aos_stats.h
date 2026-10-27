/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_STATS_H
#define AOS_STATS_H
#include <stdbool.h>
#include <stdint.h>

/* The level fields of gEwramData->unk_1325C (include/structs/ewram.h) and
 * the bestiary bitset at 0x133A0. */
enum { AOS_ITEM_RECORDS = 0x20 };

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
    uint32_t dropped[2][AOS_STATS_ENEMY_WORDS]; /* 0x133B0 common, 0x133C0 rare */
    /* equippedWeapon, equippedArmor, equippedAccessory (0xFF: none). */
    uint8_t weapon, armor, accessory;
    /* sub_0804AD9C's results: equipment ATK and DEF (0x131FC, 0x131FE),
     * effective STR, CON, INT, LCK (currentStats), resistances (0x131FA),
     * then currentATK and currentDEF (sub_08021C44). */
    int16_t equipment_atk, equipment_def;
    int16_t current[AOS_STAT_COUNT];
    uint16_t resist;
    int16_t atk, def;
    /* itemInventory, weaponInventory, armorInventory + accessoryInventory:
     * counts 0-9 (0x13294, 0x132B4, 0x132EF). */
    uint8_t item_counts[AOS_ITEM_RECORDS];
    uint8_t weapon_counts[59];
    uint8_t armor_counts[45];
} AosStats;

/* One equipment record (metadata/equipment.tsv). */
typedef struct {
    uint8_t atk, def;
    int8_t stats[AOS_STAT_COUNT];
    uint16_t resist;
} AosEquipmentRecord;

enum { AOS_WEAPON_RECORDS = 59, AOS_ARMOR_RECORDS = 45, AOS_EQUIPMENT_NONE = 0xFF };

typedef struct {
    AosEquipmentRecord weapons[AOS_WEAPON_RECORDS];
    AosEquipmentRecord armor[AOS_ARMOR_RECORDS];   /* armor and accessories */
} AosEquipmentTables;

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
/* The drop fields of an enemy record (0x080E9644, 0x24 bytes). */
typedef struct {
    uint16_t common_item;  /* + 0x08 */
    uint16_t rare_item;    /* + 0x0A */
    uint8_t soul_rarity;   /* + 0x12 */
    uint8_t common_rate;   /* + 0x15 */
    uint8_t rare_rate;     /* + 0x16 */
    uint8_t soul_type;     /* + 0x17 */
    uint8_t soul_index;    /* + 0x18, index + 1; 0 means no soul */
} AosEnemyDrops;

typedef enum {
    AOS_DROP_NONE,
    AOS_DROP_SOUL,         /* sub_0804459C(x, y, type, index, 0) */
    AOS_DROP_ITEM,         /* sub_08044054(x, y, 0xFF, item, 0) */
    AOS_DROP_PICKUP_43F70, /* sub_08043F70(x, y, variant): variant 0-6 */
    AOS_DROP_PICKUP_43C10, /* sub_08043C10(x, y, variant, 1): variant 0-1 */
} AosDropKind;

typedef struct {
    AosDropKind kind;
    uint8_t soul_type, soul_index; /* AOS_DROP_SOUL: index 0-based */
    uint16_t item;                 /* AOS_DROP_ITEM */
    bool rare;                     /* AOS_DROP_ITEM: the + 0x0A item */
    uint8_t variant;               /* the pickups */
} AosDrop;

/* What a player-side state supplies to the drop rolls. */
typedef struct {
    uint8_t accessory;   /* equippedAccessory, sub_0804B314(1) */
    int16_t luck;        /* currentStats[3] at 0x131F8 */
    bool soul_owned;     /* SoulInventory_GetSoulTotal(type, index) != 0 */
    bool soul_flag_a1;   /* gEwramData + 0xA1 & 0xF0 */
    uint8_t ground;      /* sub_08002028 at the drop point */
} AosDropContext;

/* The drop part of sub_080683BC (from 0x08068476), with the same random
 * draws in the same order: a soul when the record has one, else (unless
 * the drop point's collision blocks it) the rare or common item, else, for
 * records with an item, one of two pickups. Item drops set the record's
 * bit in the item bitsets at 0x133B0 (common) or 0x133C0 (rare). */
AosDrop aos_stats_roll_drop(AosStats *stats, uint8_t enemy_id, const AosEnemyDrops *record,
                            const AosDropContext *context, uint32_t (*random)(void));
/* sub_08014548's new-game equipment: weapon 0, armor 0, no accessory. */
void aos_stats_new_game_equipment(AosStats *stats);
/* sub_08014548's new-game inventory: one weapon 0 and one armor 0. */
void aos_stats_new_game_inventory(AosStats *stats);
/* sub_0804AD9C: sums the weapon, armor and accessory records
 * (sub_0804ACD4; resistances from armor and accessory only), adds the base
 * stats, then sets currentATK = STR + equipment ATK and currentDEF =
 * CON / 2 + equipment DEF, at least 0 (sub_08021FA4, sub_08021C44). An
 * index outside its table counts as nothing. */
void aos_stats_refresh(AosStats *stats, const AosEquipmentTables *tables);
/* Whether the bestiary bit of an enemy is set. */
bool aos_stats_enemy_defeated(const AosStats *stats, uint8_t enemy_id);
#endif
