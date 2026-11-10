/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_RECENT_ITEMS_H
#define AOS_RECENT_ITEMS_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_stats.h"

enum {
    AOS_PICKUP_RECENT_LISTS = 3,
    AOS_PICKUP_RECENT_ITEMS = 4,
};

/* sub_0804AE3C's four recent weapons, body armor pieces and accessories.
 * Empty entries are 0xFF. */
typedef struct {
    uint8_t item[AOS_PICKUP_RECENT_LISTS][AOS_PICKUP_RECENT_ITEMS];
} AosPickupRecent;

void aos_pickup_recent_reset(AosPickupRecent *recent);
/* category 3 is a weapon; category 4 selects body armor below index 25 and
 * accessories from index 25. Returns true only when a new entry was added. */
bool aos_pickup_recent_record(AosPickupRecent *recent, uint8_t category,
                              uint8_t index);
/* sub_0804AED4 before an equipment change: remove the selected record when
 * its only copy becomes equipped, remove the old equipment, compact toward
 * the end and insert that old equipment immediately before the survivors. */
bool aos_pickup_recent_exchange(AosPickupRecent *recent, const AosStats *stats,
                                uint8_t slot, uint8_t selected);

/* sub_08049E64's four recent souls per colour family at gEwramData
 * +0x133DC/+0x133E0/+0x133E4. Entries are the 1-based soul ids; empty
 * entries are 0xFF. Ability souls (family 3) have no native list. */
typedef struct AosRecentSouls {
    uint8_t soul[AOS_PICKUP_RECENT_LISTS][AOS_PICKUP_RECENT_ITEMS];
} AosRecentSouls;

void aos_pickup_recent_soul_reset(AosRecentSouls *recent);
/* type is the soul family 0-2; index the zero-based soul index. Returns
 * true only when a new entry was added. */
bool aos_pickup_recent_soul_record(AosRecentSouls *recent, uint8_t type,
                                   uint8_t index);

#endif /* AOS_RECENT_ITEMS_H */
