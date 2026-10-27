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
    uint8_t field_8;    /* + 0x8: sub_0803B924's attribute bits (unk_53E_3) */
    uint8_t field_9;    /* + 0x9: sub_0803F2C8's sequence (lifted), else
                         * unk_549; its meaning is not traced */
    uint16_t lifted;    /* + 0xA: non-zero lifts the sprite 8 pixels */
} AosMoneyKind;

enum {
    AOS_MONEY_KINDS = 7,
    AOS_PICKUP_MONEY = 1,
    AOS_PICKUP_ITEM = 2,    /* itemInventory, records 0x08505B3C */
    AOS_PICKUP_WEAPON = 3,  /* weaponInventory, records 0x08505D3C */
    AOS_PICKUP_ARMOR = 4,   /* armor and accessories, records 0x085063B0 */
    AOS_PICKUP_SOUL = 5,    /* types 5-8: soul type + 5 (sub_0804459C) */
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
    /* Souls (sub_080446D4): heading (+ 0x18, 0x10000 per turn), turn step
     * (+ 0x1C), speed (+ 0x20) and velocity (+ 0x48, + 0x4C). */
    uint32_t heading;
    int32_t turn, speed;
    int32_t vx;
} AosPickup;

/* Soma's position (16.16) for the soul pickups' homing. */
typedef uint32_t (*AosRandom)(void);

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
/* sub_0804459C for a dropped soul (last argument 0): it starts heading away
 * from a point above Soma (y - (random & 0x7F) * 0x1000 - 8 pixels, one
 * draw) at speed 0x30000, with 200 frames to live. */
AosPickup aos_pickup_create_soul(uint8_t soul_type, uint8_t soul_index, int32_t x, int32_t y,
                                 int32_t soma_x, int32_t soma_y, AosRandom random);
/* sub_080446D4 up to its collision pass: turns toward Soma's y - 16 pixels
 * (sub_0802D408, step + 0x2A per frame up to 0x4000), speeds up by 0x1800
 * per frame up to 0x80000 and moves. */
void aos_pickup_soul_move(AosPickup *pickup, int32_t soma_x, int32_t soma_y);
/* sub_080446D4 after its collision pass: on even timer values the trail
 * spark draws two offsets (sub_0802D5EC(0x200000) twice; the spark itself is
 * not created), then 200 frames count down, 64 frames blink and the soul is
 * removed. */
void aos_pickup_soul_finish(AosPickup *pickup, AosRandom random);
/* Cases 5-8 of sub_080441F8: a soul not yet held is new (and an ability
 * soul enables its ability, sub_08032ADC); it is added (sub_08032DBC counts
 * it) and the pickup removed. Returns whether it was new. */
bool aos_pickup_collect_soul(AosPickup *pickup, AosStats *stats);
/* sub_08043DD0 for types up to 4: falls (0x2000 per frame, at most
 * 0x40000), lands on the floor probe of sub_08001D94, rests 300 frames,
 * then blinks 64 frames and is removed. */
void aos_pickup_update(AosPickup *pickup, const AosCollision *layer);
/* The money case of sub_080441F8: removes the pickup and adds its value to
 * the gold, capped at 999999. Returns the message id it shows. */
uint32_t aos_pickup_collect_money(AosPickup *pickup, const AosMoneyKind kinds[AOS_MONEY_KINDS],
                                  uint32_t *gold);
#endif
