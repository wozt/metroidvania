/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SAVE_H
#define AOS_SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AOS_SAVE_SCHEMA "metroidvania-aos-save-v6"
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
} AosSaveData;

void aos_save_init_new(AosSaveData *save, unsigned slot);
bool aos_save_load(const char *path, AosSaveData *save, char *error, size_t error_size);
bool aos_save_store(const char *path, const AosSaveData *save, char *error, size_t error_size);

#endif
