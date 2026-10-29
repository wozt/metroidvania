/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SAVE_H
#define AOS_SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AOS_SAVE_SCHEMA "metroidvania-aos-save-v9"
#define AOS_SAVE_SCHEMA_V8 "metroidvania-aos-save-v8"
#define AOS_SAVE_SCHEMA_V7 "metroidvania-aos-save-v7"
#define AOS_SAVE_SCHEMA_V6 "metroidvania-aos-save-v6"
#define AOS_SAVE_SCHEMA_V5 "metroidvania-aos-save-v5"
#define AOS_SAVE_SCHEMA_V4 "metroidvania-aos-save-v4"
#define AOS_SAVE_SCHEMA_V3 "metroidvania-aos-save-v3"
#define AOS_SAVE_SCHEMA_V2 "metroidvania-aos-save-v2"
#define AOS_SAVE_SCHEMA_V1 "metroidvania-aos-save-v1"
#define AOS_SAVE_SLOT_COUNT 3

/* Portable host save metadata. This is deliberately separate from the
 * original 0x47C-byte SRAM slot until every native field is understood. */
enum { AOS_SAVE_INVENTORY = 32 + 59 + 45, AOS_SAVE_WEAPONS = 32, AOS_SAVE_ARMOR = 32 + 59 };
enum { AOS_SAVE_SOUL_BYTES = 56 + 26 + 36 + 3 + 2 };

typedef struct {
    uint8_t slot;
    uint8_t area;
    uint16_t room;
    int16_t spawn_x;
    int16_t spawn_y;
    int16_t hp;
    int16_t max_hp;
    uint32_t moves;
    uint64_t play_frames;
    uint8_t name[8];
    uint8_t character;
    uint32_t challenge_flags;
    /* unk_1325C.currentExperience and currentGold (schema v3). The level and
     * its stats are derived from the experience (sub_08033E38). */
    uint32_t experience;
    uint32_t gold;
    /* equippedWeapon, equippedArmor, equippedAccessory (schema v4; older
     * files get sub_08014548's new-game set: 0, 0, none = 0xFF). */
    uint8_t weapon, armor, accessory;
    /* Inventory counts 0-9 (schema v5; older files get sub_08014548's new
     * game: one weapon 0 and one armor 0): 32 items, 59 weapons, 45 armor
     * and accessories. */
    uint8_t inventory[AOS_SAVE_INVENTORY];
    /* Schema v6: the soul inventories as their native bytes (red 2 x 28,
     * blue 2 x 13, yellow 2 x 18, ability 3), the enabled abilities (2) and
     * totalNbrSoulsCollected; older files hold no souls. */
    uint8_t souls[AOS_SAVE_SOUL_BYTES];
    uint16_t souls_collected;
    /* Schema v7: the equipped red, blue and yellow souls (index + 1, 0 none)
     * and currentMP; older files have none equipped and MP at -1, which
     * means full (sub_08014548 starts at the maximum). */
    uint8_t red_soul, blue_soul, yellow_soul;
    int16_t mp;
    /* Schema v8: native button table indices in attack, jump, ability,
     * guardian order. Older files receive 1, 0, 3, 2 (B, A, L, R). */
    uint8_t buttons[4];
    /* Schema v9: bestiary discovery plus common and rare drop discovery,
     * matching 0x133A0, 0x133B0 and 0x133C0. */
    uint32_t defeated[4];
    uint32_t dropped[2][4];
} AosSaveData;

void aos_save_init_new(AosSaveData *save, unsigned slot);
bool aos_save_load(const char *path, AosSaveData *save, char *error, size_t error_size);
bool aos_save_store(const char *path, const AosSaveData *save, char *error, size_t error_size);
/* Portable companion for the native odd SRAM slot: `file.sav` becomes
 * `file.quick.sav`; paths without that suffix receive `.quick`. */
bool aos_save_quick_path(char *out, size_t size, const char *regular_path);

#endif
