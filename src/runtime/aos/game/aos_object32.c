#include "aos_object32.h"

void aos_object32_create(AosObject32 *o, int16_t param0,
                         int16_t param1, int32_t room_x,
                         int32_t room_y, int32_t ent_x,
                         int32_t ent_y, bool sprite_ok,
                         bool *deleted) {
    *o = (AosObject32){0};
    *deleted = false;
    o->param0 = param0;
    o->param1 = param1;
    o->song = -1;
    if (!sprite_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    /* + 0x65 = param1 == 0 ? 0 : 7 — the child anim selector. */
    o->anim_sel = param1 == 0 ? 0 : 7;
    o->pos_x = ent_x + room_x;              /* + 0x10 */
    o->pos_y = ent_y + room_y;              /* + 0x14 */
}

void aos_object32_update(AosObject32 *o, bool linked_self,
                         bool player_near) {
    o->song = -1;
    o->deactivate = false;
    o->child_count = 0;

    /* sub_08059580 while + 0xA == 0. */
    switch (o->state) {
    case 0:
        if (linked_self) {
            o->state = 1;
            o->timer = 0;
            o->song = 0x153;                /* 0xA9 << 1 */
        }
        break;
    case 1: {
        uint8_t old = o->timer;
        o->timer = (uint8_t)(old + 1);
        if (old > 9) {
            o->state = 2;
            o->timer = 0;
            o->anim_sel++;
        }
        break;
    }
    case 2: {
        uint8_t old = o->timer;
        o->timer = (uint8_t)(old + 1);
        if (old > 0x13) {
            o->state = 3;
            o->timer = 0;
            o->anim_sel++;
            o->song = 0xA8;                 /* 0x151 */
        }
        break;
    }
    case 3: {
        uint8_t old = o->timer;
        o->timer = (uint8_t)(old + 1);
        if (old > 9) {
            o->state = 4;
            o->timer = 0;
            o->anim_sel++;
            o->song = 0x150;                /* 0xA8 << 1 */
        }
        break;
    }
    case 4: {
        uint8_t old = o->timer;
        o->timer = (uint8_t)(old + 1);
        if (old > 5) {
            o->flags59 |= 8;
            /* sub_080596E0 slots (slot, arg): (5,1), (6,2),
             * (4,3) always; param1 == 0 adds (4,0), (5,4). */
            o->child_count = o->param1 == 0 ? 5 : 3;
        }
        break;
    }
    }

    /* Tail: + 0x40/+ 0x44 = + 0x10/+ 0x14 - A094 origin. The
     * origin inputs are folded into pos_x/pos_y at create;
     * the runtime passes 0 rooms so the mirror is identity. */
    o->world_x = o->pos_x;
    o->world_y = o->pos_y;

    /* Object32Update proximity gate: player within
     * 0x1FFFFF on both axes -> sub_08021278(1). */
    if (player_near) o->deactivate = true;
}
