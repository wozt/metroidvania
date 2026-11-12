#ifndef AOS_OBJECT10_H
#define AOS_OBJECT10_H
#include <stdbool.h>
#include <stdint.h>

/* Object10Create (0x080510B0) / Object10Update (0x080511F0),
 * dispatch index 16 (0x10): the save point statue. Create picks
 * one of three sprite triples through sub_0806E0D0 by the
 * gEwramData + 0x4D0 word (2 and 3 select the 0x08268450 /
 * 0x08268458 sets, anything else the 0x081604A0 set), then scans
 * the three 0x08526390 {limit, item} records: the first record
 * whose limit covers the + 0x124C entity's + 0x14 progress word
 * AND whose item is not already owned supplies + 0x18 (default
 * item 1). sub_08044054 then registers that global item id as a
 * scripted pickup at the entity's + 0x42/+ 0x46 room pixels with
 * the + 0xB mark set (the aos_pickup_create_scripted_item path),
 * clears + 0x59 bits 0-1, sets + 0x58 bit 0x20, stores 0x80/0x50
 * into + 0x42/+ 0x46 and installs callback sub_08050F1C.
 *
 * Object10Update runs a four-state save sequence while the
 * + 0x42C bit-0x400 trigger holds: state 0 latches + 0xD = 0x80,
 * enters 1 and clears + 0x58 bits 0/5/7; state 1 locks player
 * input through SetPlayerInput(1, 0), counts + 0xD down and on
 * expiry loads the SRAM slot byte at + 0x428, calls
 * sub_08043B18(+ 0x18) (the inventory-count increment for the
 * create-selected item), runs sub_080135C0 on the + 0x60 save
 * block, copies the + 0x124C entity's + 0x14 into its + 0x50
 * respawn field when it is zero or larger, and writes the slot
 * back through SaveData_SaveSlotToSram; state 2 keeps the lock,
 * stores 2 into + 0x64 and clears + 0x65; state 3 is inert. */
typedef struct {
    uint8_t state;          /* +0xA: 0 idle, 1 save, 2 mark, 3 done */
    uint8_t timer;          /* +0xD: 0x80 save countdown */
    uint32_t item_id;       /* +0x18: create-selected grant item */
    uint8_t statue_state;   /* + 0x4D0 word: 2/3 pick variant art */
    uint8_t flags58;        /* +0x58: bit 0x20 set, 0/5/7 cleared */
    uint8_t flags59;        /* +0x59: bits 0-1 cleared at create */
    bool input_locked;      /* SetPlayerInput(1, 0) in states 1-2 */
    bool save_committed;    /* state-1 expiry: SRAM slot written */
    bool stats_refreshed;   /* sub_080135C0 save-block refresh */
    bool respawn_updated;   /* + 0x124C + 0x14 -> + 0x60 + 0x50 */
    unsigned grants;        /* sub_08043B18 inventory increments */
} AosObject10;

/* `progress` is the + 0x124C entity's + 0x14 word; `owned` is
 * the inventory-membership probe behind the record scan (NULL
 * selects the first covered record). `item_x`/`item_y` are the
 * + 0x42/+ 0x46 room pixels the scripted pickup uses. */
void aos_object10_create(AosObject10 *o, uint32_t progress,
                         uint8_t statue_state, int16_t item_x,
                         int16_t item_y,
                         bool (*owned)(void *ctx, uint16_t item),
                         void *owned_ctx);

/* `trigger` approximates the + 0x42C bit-0x400 activation input. */
void aos_object10_update(AosObject10 *o, bool trigger);
#endif
