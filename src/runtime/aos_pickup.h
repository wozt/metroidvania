/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_PICKUP_H
#define AOS_PICKUP_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_collision.h"
#include "aos_combat.h"
#include "aos_stats.h"

/* One record of the money table 0x085250E4 (12 bytes), exported as
 * metadata/money.tsv. */
typedef struct {
    uint32_t value;     /* + 0x0: gold */
    uint32_t text;      /* + 0x4: message of sub_0800EF98 */
    uint8_t anim;       /* + 0x8: sprite sequence of sub_0803B924 */
    uint8_t palette;    /* + 0x9 */
    uint16_t lifted;    /* + 0xA: non-zero lifts the sprite 8 pixels */
} AosMoneyKind;

enum {
    AOS_MONEY_KINDS = 7,
    AOS_PICKUP_MONEY = 1,
    AOS_PICKUP_ITEM = 2,    /* itemInventory, records 0x08505B3C */
    AOS_PICKUP_WEAPON = 3,  /* weaponInventory, records 0x08505D3C */
    AOS_PICKUP_ARMOR = 4,   /* armor and accessories, records 0x085063B0 */
};

typedef enum {
    AOS_COLLECT_TAKEN,      /* counted (at most 9) and removed */
    AOS_COLLECT_FULL,       /* more than 8 held: it stays on the ground */
} AosCollectResult;

/* A pickup entity of sub_08043F70 (entities 0x49-0xCF). */
typedef struct {
    uint8_t type;       /* + 0x36: 1 money */
    uint16_t variant;   /* + 0x32 */
    int32_t x, y;       /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vy;         /* + 0x4C */
    int16_t offset;     /* + 0x56 */
    uint8_t state;      /* + 0x0A: 0 falling, 1 resting, 2 blinking */
    uint16_t timer;     /* + 0x18 */
    uint8_t blink;      /* + 0x0D */
    uint16_t cooldown;  /* + 0x1A */
    bool blinking;      /* + 0x0F */
    bool removed;       /* + 0x59 bit 3 */
} AosPickup;

/* The pickup's collision box (sub_08042884 with 0x1010F8F8). */
extern const AosBox aos_pickup_box;

/* sub_08043F70: a money pickup of `variant` (0-6) at room pixel (x, y). */
AosPickup aos_pickup_create_money(const AosMoneyKind kinds[AOS_MONEY_KINDS], uint8_t variant,
                                  int32_t x, int32_t y);
/* The global item numbering of sub_08044054 with category 0xFF: 1-32
 * items (index id - 1), 33-91 weapons (id - 0x21), 92-136 armor and
 * accessories (id - 0x5C). False for 0 and larger numbers. */
bool aos_item_decode(uint16_t id, uint8_t *category, uint8_t *index);
/* sub_08044054 for categories 2-4: an item pickup at room pixel (x, y). */
AosPickup aos_pickup_create_item(uint8_t category, uint8_t index, int32_t x, int32_t y);
/* Cases 2-4 of sub_080441F8: below 9 held the count rises (to at most 9)
 * and the pickup is removed; above 8 it stays, and the refusal sound 0x13B
 * plays only when its 0x20-frame cooldown has run out (*refusal_sound). */
AosCollectResult aos_pickup_collect_item(AosPickup *pickup, AosStats *stats,
                                         bool *refusal_sound);
/* sub_08043DD0 for types up to 4: falls (0x2000 per frame, at most
 * 0x40000), lands on the floor probe of sub_08001D94, rests 300 frames,
 * then blinks 64 frames and is removed. */
void aos_pickup_update(AosPickup *pickup, const AosCollision *layer);
/* The money case of sub_080441F8: removes the pickup and adds its value to
 * the gold, capped at 999999. Returns the message id it shows. */
uint32_t aos_pickup_collect_money(AosPickup *pickup, const AosMoneyKind kinds[AOS_MONEY_KINDS],
                                  uint32_t *gold);
#endif
