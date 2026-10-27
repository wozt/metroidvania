/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_pickup.h"

#include "aos_enemy.h"

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

/* (s16)(value >> 16) with the native rounding toward zero of negatives. */
static int16_t whole(int32_t value) {
    return (int16_t)(value < 0 ? -((-value) >> 16) : value >> 16);
}

/* sub_0802D344: the angle from the pickup to (x, y), 16.16 room pixels. */
static uint16_t angle_to(const AosPickup *pickup, int32_t x, int32_t y) {
    return aos_arctan2((int16_t)(whole(x) - (pickup->x >> 16)),
                       (int16_t)(whole(y) - (pickup->y >> 16)));
}

AosPickup aos_pickup_create_soul(uint8_t soul_type, uint8_t soul_index, int32_t x, int32_t y,
                                 int32_t soma_x, int32_t soma_y, AosRandom random) {
    AosPickup pickup = {.type = (uint8_t)(soul_type + AOS_PICKUP_SOUL), .variant = soul_index,
                        .x = x * 65536, .y = y * 65536};
    int32_t target_y = soma_y - (int32_t)((random() & 0x7F) << 12) - 0x80000;
    pickup.heading = (uint32_t)(angle_to(&pickup, soma_x, target_y) + 0x8000) & 0xFFFF;
    pickup.turn = 0;
    pickup.speed = 0x30000;
    pickup.blink = 200;
    return pickup;
}

void aos_pickup_soul_move(AosPickup *pickup, int32_t soma_x, int32_t soma_y) {
    if (pickup->state > 1) return;
    /* sub_0802D408: one turn step along the shorter way (an exact match
     * still steps down). */
    int32_t difference = angle_to(pickup, soma_x, soma_y - 0x100000) - (int32_t)pickup->heading;
    bool up = difference > 0 ? difference <= 0x7FFF : difference < -0x8000;
    pickup->heading = (uint32_t)((int32_t)pickup->heading + (up ? pickup->turn : -pickup->turn)) &
                      0xFFFF;
    pickup->turn += 0x2A;
    if (pickup->turn > 0x4000) pickup->turn = 0x4000;
    pickup->speed += 0x1800;
    if (pickup->speed > 0x80000) pickup->speed = 0x80000;
    pickup->vx = (aos_sine(pickup->heading + 0x4000) >> 8) * (pickup->speed >> 8);
    pickup->vy = (aos_sine(pickup->heading) >> 8) * (pickup->speed >> 8);
    pickup->x += pickup->vx;
    pickup->y += pickup->vy;
}

void aos_pickup_soul_finish(AosPickup *pickup, AosRandom random) {
    if (!(pickup->blink & 1)) {
        /* The spark's position offsets, sub_0802D5EC(0x200000) each. */
        (void)random();
        (void)random();
    }
    if (pickup->state == 0) {
        if (--pickup->blink == 0) {
            pickup->blinking = true;
            pickup->blink = 0x40;
            pickup->state = 1;
        }
    } else if (pickup->state == 1) {
        if (--pickup->blink == 0) pickup->removed = true;
    } else {
        pickup->blink++;
    }
}

bool aos_pickup_collect_soul(AosPickup *pickup, AosStats *stats) {
    int32_t type = pickup->type - AOS_PICKUP_SOUL, index = pickup->variant;
    bool fresh = aos_souls_total(stats, type, index) == 0;
    if (fresh && type == AOS_SOUL_ABILITY) aos_souls_set_ability(stats, index, true);
    aos_souls_add(stats, type, index, 1);
    aos_souls_count(stats, 1);
    pickup->removed = true;
    return fresh;
}

const AosBox aos_heart_box = {-4, -4, 8, 8};

AosBox aos_pickup_box_of(const AosPickup *pickup) {
    return pickup->type == AOS_PICKUP_HEART ? aos_heart_box : aos_pickup_box;
}

AosPickup aos_pickup_create_heart(bool big, bool pop, int32_t x, int32_t y) {
    AosPickup pickup = {.type = AOS_PICKUP_HEART, .variant = big ? 15 : 3,
                        .x = x * 65536, .y = y * 65536, .small_heart = !big};
    if (pop) pickup.vy = (int32_t)0xFFFE0000;
    return pickup;
}

void aos_pickup_small_heart_update(AosPickup *pickup, const AosCollision *layer) {
    if (pickup->removed) return;
    switch (pickup->state) {
    case 0:
        if (pickup->vy >= 0) {
            pickup->state = 1;
            break;
        }
        pickup->vy += 0x2000;
        pickup->y += pickup->vy;
        break;
    case 1: {
        pickup->vy = 0x4000;
        pickup->vx = aos_sine((uint32_t)(pickup->blink << 9) + 0x4000) >> 1;
        pickup->x += pickup->vx;
        pickup->y += pickup->vy;
        pickup->blink++;
        int depth = aos_floor_depth(layer, pickup->x >> 16, (pickup->y >> 16) + 4, 0, false);
        if (depth) {
            pickup->y -= depth;
            pickup->blink = 200;
            pickup->state = 2;
        }
        break;
    }
    case 2:
        if (--pickup->blink == 0) {
            pickup->blink = 0x40;
            pickup->state = 3;
        }
        break;
    case 3:
        pickup->blinking = true;
        if (--pickup->blink == 0) pickup->removed = true;
        break;
    default: break;
    }
}

void aos_pickup_collect_heart(AosPickup *pickup, AosStats *stats) {
    pickup->removed = true;
    int32_t share = (int32_t)(((uint32_t)stats->max_mp << 16) / 100) * pickup->variant;
    int32_t gain = share < 0 ? -((-share) >> 16) : share >> 16;
    if (gain == 0) gain = 1;
    stats->mp = (int16_t)(stats->mp + gain);
    if (stats->mp > (int16_t)stats->max_mp) stats->mp = (int16_t)stats->max_mp;
}
