/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-neutral native slices of Aria's pause hub and soul menus. */
#ifndef AOS_SOUL_MENU_H
#define AOS_SOUL_MENU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_stats.h"

enum {
    AOS_SOUL_MENU_RED,
    AOS_SOUL_MENU_BLUE,
    AOS_SOUL_MENU_YELLOW,
    AOS_SOUL_MENU_ABILITY,
    AOS_SOUL_MENU_CATEGORIES,
};

enum {
    AOS_PAUSE_SOUL_SET,
    AOS_PAUSE_EQUIP,
    AOS_PAUSE_ITEM_USE,
    AOS_PAUSE_ABILITY,
    AOS_PAUSE_SLEEP,
    AOS_PAUSE_CONFIG,
    AOS_PAUSE_ENEMY,
    AOS_PAUSE_ENTRIES,
};

enum {
    AOS_SOUL_MENU_PAGE_HUB,
    AOS_SOUL_MENU_PAGE_SOUL_SET,
    AOS_SOUL_MENU_PAGE_ABILITY,
    AOS_SOUL_MENU_PAGE_EQUIP,
    AOS_SOUL_MENU_PAGE_ITEM_USE,
    AOS_SOUL_MENU_PAGE_UNAVAILABLE,
};

enum {
    AOS_EQUIP_WEAPON,
    AOS_EQUIP_ARMOR,
    AOS_EQUIP_ACCESSORY,
    AOS_EQUIP_SLOTS,
};

enum {
    AOS_SOUL_MENU_OPENED = 1,
    AOS_SOUL_MENU_CLOSED = 2,
    AOS_SOUL_MENU_CHANGED = 4,
    AOS_SOUL_MENU_MOVED = 8,
    AOS_SOUL_MENU_ACCEPTED = 16,
    AOS_SOUL_MENU_BACK = 32,
    AOS_SOUL_MENU_UNAVAILABLE = 64,
    AOS_SOUL_MENU_ITEM_USED = 128,
    AOS_SOUL_MENU_REFUSED = 256,
};

typedef struct {
    uint8_t icon;
    uint8_t effect;
    uint16_t amount;
} AosItemUseRecord;

typedef struct {
    int16_t hp;
    int16_t max_hp;
} AosItemVitals;

typedef struct {
    bool open;
    uint8_t page;
    uint8_t hub_cursor;
    uint8_t category;
    uint8_t equip_slot;
    uint8_t equip_choice;
    bool equip_selecting;
    uint8_t item_cursor;
    uint8_t last_item;
    /* Colour cursors are native equipped values: 0 None, then index + 1.
     * The ability cursor is its zero-based index. */
    uint8_t cursor[AOS_SOUL_MENU_CATEGORIES];
} AosSoulMenu;

/* START opens the seven-entry pause hub reconstructed from sub_08047764.
 * Soul Set, Equip, Item Use and Ability enter usable source-adapted pages; the
 * three unported pages enter an explicit unavailable page. Soul Set changes
 * colour and selects owned souls. Equip selects owned records for three
 * slots. Item Use applies ROM-exported effects to owned consumables. Ability
 * toggles owned abilities. B returns to the hub and START closes the menu.
 * The return value is an AOS_SOUL_MENU_* event mask. */
unsigned aos_pause_menu_update(AosSoulMenu *menu, AosStats *stats,
                               const AosEquipmentTables *equipment,
                               const AosItemUseRecord items[AOS_ITEM_RECORDS],
                               AosItemVitals *vitals,
                               uint16_t new_input, uint16_t repeated_input);

/* Compatibility entry for the soul/equipment-only controller tests and
 * hosts that have not loaded item metadata. */
static inline unsigned aos_soul_menu_update(AosSoulMenu *menu, AosStats *stats,
                                            const AosEquipmentTables *equipment,
                                            uint16_t new_input, uint16_t repeated_input) {
    return aos_pause_menu_update(menu, stats, equipment, NULL, NULL,
                                 new_input, repeated_input);
}

int aos_soul_menu_count(uint8_t category);
int aos_soul_menu_inventory_index(const AosSoulMenu *menu);
uint8_t aos_soul_menu_quantity(const AosSoulMenu *menu, const AosStats *stats);
bool aos_soul_menu_enabled(const AosSoulMenu *menu, const AosStats *stats);
const char *aos_soul_menu_category_name(uint8_t category);
const char *aos_soul_menu_name(uint8_t category, uint8_t cursor);
const char *aos_pause_entry_name(uint8_t entry);
const char *aos_equipment_slot_name(uint8_t slot);
const char *aos_equipment_name(uint8_t slot, uint8_t item);
uint8_t aos_equipment_current(const AosStats *stats, uint8_t slot);
int aos_equipment_choice_count(const AosStats *stats, uint8_t slot);
uint8_t aos_equipment_choice_at(const AosStats *stats, uint8_t slot, int ordinal);
uint8_t aos_equipment_quantity(const AosStats *stats, uint8_t slot, uint8_t item);
const char *aos_item_name(uint8_t item);
int aos_item_choice_count(const AosStats *stats);
uint8_t aos_item_choice_at(const AosStats *stats, int ordinal);
bool aos_item_use(AosStats *stats, const AosItemUseRecord items[AOS_ITEM_RECORDS],
                  AosItemVitals *vitals, uint8_t item);

#endif /* AOS_SOUL_MENU_H */
