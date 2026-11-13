#include "aos_object35.h"

void aos_object35_create(AosObject35 *o, int16_t param0,
                         int16_t param1, int32_t pos_x,
                         int32_t pos_y, bool global_bit,
                         bool global_bit_preset, bool sprite_ok,
                         bool *deleted) {
    *o = (AosObject35){0};
    *deleted = false;
    o->param0 = param0;
    o->param1 = param1;
    o->pos_x = pos_x;                       /* +0x40 */
    o->pos_y = pos_y;                       /* +0x44 */
    o->song = -1;
    if (!sprite_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    o->flags59 = 2;                         /* +0x59 bit 1 */
    o->flags58 &= ~(uint8_t)0x41;           /* clear bits 0/6 */
    if (param0 == 0) {
        /* +0x65 = 2, sub_0805A50C child, +0xB = 0 (or 2 with
         * the preset bit's +0x44 lift). */
        o->sel = 2;
        o->child_count = 1;
        o->state = global_bit_preset ? 2 : 0;
        if (global_bit_preset) o->pos_y += 0x60000;
    } else {
        /* +0x65 = 1, +0xB = 0xA. */
        o->sel = 1;
        o->state = 0xA;
    }
    o->anim = 0x12;
    o->world_x = o->pos_x;                  /* +0x10 = +0x40 */
    o->world_y = o->pos_y;                  /* +0x14 = +0x44 */
    (void)global_bit;
}

void aos_object35_update(AosObject35 *o, bool linked_self,
                         bool global_bit) {
    o->song = -1;
    o->deactivate = false;
    o->flash = false;
    o->particle_count = 0;

    /* Object35Update: sub_08021278(1) only for param0 == 0. */
    if (o->param0 == 0) o->deactivate = true;
    if (o->flag_a != 0) return;

    switch (o->state) {
    case 0:
        if (linked_self) {
            if (++o->counter > 7) {
                o->state = 1;
                o->vel_y = 0;
                o->timer = 0;
                /* set the +0x344 global bit, PlaySong 0x133. */
                o->song = 0x133;
            }
        } else {
            o->counter = 0;
        }
        break;
    case 1: {
        /* sub_0806D3CC: +0x44 += +0x4C. */
        o->pos_y += o->vel_y;
        o->vel_y = 0x10000;                 /* re-armed each frame */
        uint8_t old = o->timer;
        o->timer = (uint8_t)(old + 1);
        if (old > 4) {
            o->state = 2;
            o->flag_1c = 0;
        }
        break;
    }
    case 10:
        if (global_bit) {
            o->state = 11;
            o->vel_y = 0x8000;
            o->timer = 0;
            o->song = 0x113;
        }
        break;
    case 11: {
        /* sub_0806D3CC integration. */
        o->pos_y += o->vel_y;
        o->flash = true;                    /* gUnk_03002CB0 bit 15 */
        o->flag_1c = 1;
        uint8_t old = o->timer;
        o->timer = (uint8_t)(old + 1);
        /* Every eighth frame fires sub_0806D9CC; the RNG
         * velocity pair is not modelled. */
        if ((old & 7) == 0) o->particle_count = 1;
        if (old > 0x5F) o->state = 12;
        break;
    }
    case 12:
        /* gUnk_03002CB0 &= 0x7FFF — flash window closed. */
        break;
    default:
        break;                             /* states 2-9: external */
    }
}
