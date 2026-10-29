/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-neutral first native slice of Aria's in-game soul equipment menu. */
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
    AOS_SOUL_MENU_OPENED = 1,
    AOS_SOUL_MENU_CLOSED = 2,
    AOS_SOUL_MENU_CHANGED = 4,
};

typedef struct {
    bool open;
    uint8_t category;
    /* Colour cursors are native equipped values: 0 None, then index + 1.
     * The ability cursor is its zero-based index. */
    uint8_t cursor[AOS_SOUL_MENU_CATEGORIES];
} AosSoulMenu;

/* START opens the source-adapted soul page. While open, L/R changes its
 * family, repeated Up/Down selects the next owned soul (None is always
 * available for colours), A equips or toggles and B/START closes it. The
 * return value is an AOS_SOUL_MENU_* event mask. */
unsigned aos_soul_menu_update(AosSoulMenu *menu, AosStats *stats,
                              uint16_t new_input, uint16_t repeated_input);

int aos_soul_menu_count(uint8_t category);
int aos_soul_menu_inventory_index(const AosSoulMenu *menu);
uint8_t aos_soul_menu_quantity(const AosSoulMenu *menu, const AosStats *stats);
bool aos_soul_menu_enabled(const AosSoulMenu *menu, const AosStats *stats);
const char *aos_soul_menu_category_name(uint8_t category);
const char *aos_soul_menu_name(uint8_t category, uint8_t cursor);

#endif /* AOS_SOUL_MENU_H */
