#include "aos_object10.h"

/* 0x08526390: three 8-byte {limit, item} records scanned at
 * create. The first record whose limit covers the progress word
 * and whose item is unowned supplies + 0x18; item 1 is the
 * fallback. */
static const struct { uint32_t limit, item; } grant_records[3] = {
    {0x3840, 0x5B}, {0x4650, 0x5A}, {0x5460, 0x45},
};

void aos_object10_create(AosObject10 *o, uint32_t progress,
                         uint8_t statue_state, int16_t item_x,
                         int16_t item_y,
                         bool (*owned)(void *ctx, uint16_t item),
                         void *owned_ctx) {
    (void)item_x;
    (void)item_y;
    o->state = 0;
    o->timer = 0;
    o->statue_state = statue_state;
    o->item_id = 1;
    for (unsigned i = 0; i < 3; ++i) {
        if (progress > grant_records[i].limit) continue;
        uint16_t item = (uint16_t)grant_records[i].item;
        if (owned && owned(owned_ctx, item)) continue;
        o->item_id = item;
        break;
    }
    o->flags58 = 0x20;          /* + 0x58 bit 0x20 */
    o->flags59 = 0;             /* + 0x59 bits 0-1 cleared */
    o->input_locked = false;
    o->save_committed = false;
    o->stats_refreshed = false;
    o->respawn_updated = false;
    o->grants = 0;
}

void aos_object10_update(AosObject10 *o, bool trigger) {
    switch (o->state) {
    case 0:
        if (trigger) {
            o->timer = 0x80;
            o->state = 1;
            o->flags58 &= 0x5E; /* + 0x58 bits 0/5/7 cleared */
        }
        break;
    case 1:
        o->input_locked = true; /* SetPlayerInput(1, 0) */
        if (o->timer == 0) {
            /* SaveData_LoadSlotFromSram(+ 0x428), sub_08043B18(+
             * 0x18), sub_080135C0(+ 0x60), the + 0x124C + 0x14 ->
             * + 0x50 respawn copy and SaveData_SaveSlotToSram. */
            o->state = 2;
            o->grants++;
            o->stats_refreshed = true;
            o->respawn_updated = true;
            o->save_committed = true;
        } else {
            o->timer = (uint8_t)(o->timer - 1);
        }
        break;
    case 2:
        o->input_locked = true;
        /* + 0x64 = 2, + 0x65 = 0 (the save-mark bytes). */
        o->state = 3;
        break;
    case 3:
        /* No SetPlayerInput call: the lock releases. */
        o->input_locked = false;
        break;
    default:
        break;
    }
}
