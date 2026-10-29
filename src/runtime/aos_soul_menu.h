/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-neutral native slices of Aria's pause hub and soul menus. */
#ifndef AOS_SOUL_MENU_H
#define AOS_SOUL_MENU_H

#include <stdbool.h>
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
    AOS_SOUL_MENU_PAGE_UNAVAILABLE,
};

enum {
    AOS_SOUL_MENU_OPENED = 1,
    AOS_SOUL_MENU_CLOSED = 2,
    AOS_SOUL_MENU_CHANGED = 4,
    AOS_SOUL_MENU_MOVED = 8,
    AOS_SOUL_MENU_ACCEPTED = 16,
    AOS_SOUL_MENU_BACK = 32,
    AOS_SOUL_MENU_UNAVAILABLE = 64,
};

typedef struct {
    bool open;
    uint8_t page;
    uint8_t hub_cursor;
    uint8_t category;
    /* Colour cursors are native equipped values: 0 None, then index + 1.
     * The ability cursor is its zero-based index. */
    uint8_t cursor[AOS_SOUL_MENU_CATEGORIES];
} AosSoulMenu;

/* START opens the seven-entry pause hub reconstructed from sub_08047764.
 * Soul Set and Ability enter their usable source-adapted pages; the five
 * unported pages enter an explicit unavailable page. In Soul Set, L/R changes
 * colour and Up/Down selects the next owned soul. Ability uses Up/Down. A
 * equips or toggles, B returns to the hub and START closes the pause menu.
 * The return value is an AOS_SOUL_MENU_* event mask. */
unsigned aos_soul_menu_update(AosSoulMenu *menu, AosStats *stats,
                              uint16_t new_input, uint16_t repeated_input);

int aos_soul_menu_count(uint8_t category);
int aos_soul_menu_inventory_index(const AosSoulMenu *menu);
uint8_t aos_soul_menu_quantity(const AosSoulMenu *menu, const AosStats *stats);
bool aos_soul_menu_enabled(const AosSoulMenu *menu, const AosStats *stats);
const char *aos_soul_menu_category_name(uint8_t category);
const char *aos_soul_menu_name(uint8_t category, uint8_t cursor);
const char *aos_pause_entry_name(uint8_t entry);

#endif /* AOS_SOUL_MENU_H */
