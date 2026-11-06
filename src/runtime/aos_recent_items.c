/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_recent_items.h"

#include <string.h>

enum {
    AOS_RECENT_WEAPON,
    AOS_RECENT_ARMOR,
    AOS_RECENT_ACCESSORY,
    AOS_RECENT_CATEGORY_WEAPON = 3,
    AOS_RECENT_CATEGORY_ARMOR = 4,
};

void aos_pickup_recent_reset(AosPickupRecent *recent) {
    if (recent) memset(recent->item, AOS_EQUIPMENT_NONE, sizeof recent->item);
}

bool aos_pickup_recent_record(AosPickupRecent *recent, uint8_t category,
                              uint8_t index) {
    if (!recent ||
        (category != AOS_RECENT_CATEGORY_WEAPON && category != AOS_RECENT_CATEGORY_ARMOR))
        return false;
    unsigned list = category == AOS_RECENT_CATEGORY_WEAPON ? AOS_RECENT_WEAPON
                    : index < 25                           ? AOS_RECENT_ARMOR
                                                           : AOS_RECENT_ACCESSORY;
    for (unsigned slot = 0; slot < AOS_PICKUP_RECENT_ITEMS; ++slot)
        if (recent->item[list][slot] == index) return false;
    for (unsigned slot = 0; slot < AOS_PICKUP_RECENT_ITEMS; ++slot)
        if (recent->item[list][slot] == AOS_EQUIPMENT_NONE) {
            recent->item[list][slot] = index;
            return true;
        }
    recent->item[list][AOS_PICKUP_RECENT_ITEMS - 1] = index;
    return true;
}

bool aos_pickup_recent_exchange(AosPickupRecent *recent, const AosStats *stats,
                                uint8_t slot, uint8_t selected) {
    if (!recent || !stats || slot >= AOS_PICKUP_RECENT_LISTS) return false;
    uint8_t *list = recent->item[slot];
    const uint8_t *inventory;
    size_t count;
    uint8_t equipped;
    if (slot == AOS_RECENT_WEAPON) {
        inventory = stats->weapon_counts;
        count = sizeof stats->weapon_counts;
        equipped = stats->weapon;
    } else {
        inventory = stats->armor_counts;
        count = sizeof stats->armor_counts;
        equipped = slot == AOS_RECENT_ARMOR ? stats->armor : stats->accessory;
    }
    if (equipped == AOS_EQUIPMENT_NONE || equipped >= count) return false;

    uint8_t kept[AOS_PICKUP_RECENT_ITEMS];
    unsigned kept_count = 0;
    for (unsigned index = 0; index < AOS_PICKUP_RECENT_ITEMS; ++index) {
        uint8_t item = list[index];
        if (item == AOS_EQUIPMENT_NONE || item == equipped ||
            (item == selected && selected < count && inventory[selected] == 1))
            continue;
        kept[kept_count++] = item;
    }
    memset(list, AOS_EQUIPMENT_NONE, AOS_PICKUP_RECENT_ITEMS);
    unsigned first = AOS_PICKUP_RECENT_ITEMS - kept_count;
    memcpy(list + first, kept, kept_count);
    if (first == 0) {
        memmove(list + 1, list, AOS_PICKUP_RECENT_ITEMS - 1);
        list[0] = equipped;
    } else {
        list[first - 1] = equipped;
    }
    return true;
}
