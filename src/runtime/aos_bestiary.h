/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-neutral controller and identified names for Aria's Enemy page. */
#ifndef AOS_BESTIARY_H
#define AOS_BESTIARY_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_stats.h"

enum {
    AOS_BESTIARY_MENU_RECORDS = 0x70,
    AOS_BESTIARY_NATIVE_RECORDS = 0x71,
    AOS_BESTIARY_PAGE_RECORDS = 10,
};

enum {
    AOS_BESTIARY_NONE,
    AOS_BESTIARY_MOVED,
    AOS_BESTIARY_ACCEPTED,
    AOS_BESTIARY_BACK,
    AOS_BESTIARY_REFUSED,
};

typedef struct {
    uint8_t cursor;
    uint8_t top;
    bool detail;
    bool soul_view;
} AosBestiaryMenu;

void aos_bestiary_init(AosBestiaryMenu *menu);
unsigned aos_bestiary_update(AosBestiaryMenu *menu, const AosStats *stats,
                             uint16_t new_input, uint16_t repeated_input);
unsigned aos_bestiary_discovered_count(const AosStats *stats);
const char *aos_bestiary_name(uint8_t enemy);
bool aos_bestiary_is_boss(uint8_t enemy);

#endif /* AOS_BESTIARY_H */
