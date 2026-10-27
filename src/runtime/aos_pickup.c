/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_pickup.h"

enum {
    AOS_PICKUP_GRAVITY = 0x2000,
    AOS_PICKUP_MAX_FALL = 0x40000,
    AOS_PICKUP_REST = 300,
    AOS_PICKUP_BLINK = 0x40,
    AOS_GOLD_CAP = 999999,
};

/* 0x1010F8F8: x -8, y -8, 16 x 16. */
const AosBox aos_pickup_box = {-8, -8, 16, 16};

AosPickup aos_pickup_create_money(const AosMoneyKind kinds[AOS_MONEY_KINDS], uint8_t variant,
                                  int32_t x, int32_t y) {
    AosPickup pickup = {.type = AOS_PICKUP_MONEY, .variant = variant,
                        .x = x * 65536, .y = y * 65536};
    if (variant < AOS_MONEY_KINDS && kinds[variant].lifted) pickup.offset = -8;
    return pickup;
}

void aos_pickup_update(AosPickup *pickup, const AosCollision *layer) {
    if (pickup->removed || pickup->type > 4) return;
    pickup->y += pickup->vy;
    /* Money probes 8 pixels lower than the item types. */
    int32_t probe_y = (pickup->y >> 16) + pickup->offset + (pickup->type <= 1 ? 8 : 0);
    int depth = aos_floor_depth(layer, pickup->x >> 16, probe_y, 0, false);
    pickup->vy += AOS_PICKUP_GRAVITY;
    if ((uint32_t)pickup->vy > AOS_PICKUP_MAX_FALL) pickup->vy = AOS_PICKUP_MAX_FALL;
    if (pickup->cooldown) pickup->cooldown--;
    if (depth) {
        /* Only the integer half moves (strh to + 0x46). */
        pickup->y = (int32_t)((uint32_t)pickup->y + ((uint32_t)depth << 16));
        pickup->vy = 0;
        if (pickup->state == 0) {
            pickup->timer = AOS_PICKUP_REST;
            pickup->state = 1;
        }
    }
    if (pickup->state == 1) {
        if (--pickup->timer == 0) {
            pickup->blink = AOS_PICKUP_BLINK;
            pickup->state = 2;
        }
    } else if (pickup->state == 2) {
        pickup->blinking = true;
        if (--pickup->blink == 0) pickup->removed = true;
    }
}

uint32_t aos_pickup_collect_money(AosPickup *pickup, const AosMoneyKind kinds[AOS_MONEY_KINDS],
                                  uint32_t *gold) {
    pickup->removed = true;
    if (pickup->variant >= AOS_MONEY_KINDS) return 0;
    *gold += kinds[pickup->variant].value;
    if (*gold > AOS_GOLD_CAP) *gold = AOS_GOLD_CAP;
    return kinds[pickup->variant].text;
}

bool aos_item_decode(uint16_t id, uint8_t *category, uint8_t *index) {
    if (id == 0 || id > 0x88) return false;
    unsigned zero_based = id - 1u;
    if (zero_based <= 0x1F) {
        *category = AOS_PICKUP_ITEM;
        *index = (uint8_t)zero_based;
    } else if (zero_based <= 0x5A) {
        *category = AOS_PICKUP_WEAPON;
        *index = (uint8_t)(id - 0x21);
    } else {
        *category = AOS_PICKUP_ARMOR;
        *index = (uint8_t)(id - 0x5C);
    }
    return true;
}

AosPickup aos_pickup_create_item(uint8_t category, uint8_t index, int32_t x, int32_t y) {
    return (AosPickup){.type = category, .variant = index, .x = x * 65536, .y = y * 65536};
}

AosCollectResult aos_pickup_collect_item(AosPickup *pickup, AosStats *stats,
                                         bool *refusal_sound) {
    uint8_t *counts = pickup->type == AOS_PICKUP_ITEM     ? stats->item_counts
                      : pickup->type == AOS_PICKUP_WEAPON ? stats->weapon_counts
                                                          : stats->armor_counts;
    size_t limit = pickup->type == AOS_PICKUP_ITEM     ? sizeof stats->item_counts
                   : pickup->type == AOS_PICKUP_WEAPON ? sizeof stats->weapon_counts
                                                       : sizeof stats->armor_counts;
    *refusal_sound = false;
    if (pickup->variant >= limit) {
        pickup->removed = true;
        return AOS_COLLECT_TAKEN;
    }
    uint8_t *count = &counts[pickup->variant];
    if (*count > 8) {
        *refusal_sound = pickup->cooldown == 0;
        pickup->cooldown = 0x20;
        return AOS_COLLECT_FULL;
    }
    (*count)++;
    pickup->removed = true;
    return AOS_COLLECT_TAKEN;
}
