#include "aos_object18.h"

void aos_object18_create(AosObject18 *o, int param0) {
    o->state = 0;
    o->timer = 0;
    o->flags58 = 0;
    o->finished = false;
    o->armed = param0 != 0;
    o->song = -1;
    o->ring_fx = 0;
    o->gore_fx = 0;
    o->child_fx = 0;
    o->drop_count = 0;
    o->heart = false;
}

void aos_object18_update(AosObject18 *o, bool near, bool attack,
                         bool event_flag, int32_t room_x,
                         int32_t room_y, AosRandom random) {
    o->song = -1;
    o->ring_fx = 0;
    o->gore_fx = 0;
    o->child_fx = 0;
    o->drop_count = 0;
    o->heart = false;

    if (!near) {
        /* Probe miss: + 0x58 bit 0x20 and the frame ends. */
        o->flags58 |= 0x20;
        return;
    }

    if (o->state == 0) {
        o->flags58 &= ~0x21;
        if (!attack)
            return;
        if (o->armed) {
            /* + 0x30 s16 nonzero: straight to the state-2 child
             * spawner, no scatter. */
            o->state = 2;
            o->timer = 0x20;
            o->ring_fx = 4;     /* sub_0805379C: sb 0..3 */
            o->song = 0xA2;
            o->flags58 |= 0x20;
            return;
        }
        if (event_flag) {
            /* gEwramData + 0x13266 set: one sub_08043C10 big
             * heart at (x, y - 8), no pop. */
            o->heart = true;
        } else {
            /* Eight sub_08043F70 money pickups of variants
             * i / 2; natively the loop stops at the first
             * failed create, approximated as all eight. */
            for (int i = 0; i < 8; ++i) {
                int jx = (int)(random() & 0x1F) - 0x10;
                int jy = (int)(random() & 0xF);
                o->drops[i].drop_x = (int16_t)(room_x + jx);
                o->drops[i].drop_y = (int16_t)(room_y - 8 - jy);
                o->drops[i].variant = (uint8_t)(i / 2);
            }
            o->drop_count = 8;
        }
        o->ring_fx = 4;
        o->song = 0xA2;
        o->flags58 |= 0x20;
        o->timer = 0x28;
        o->state = 1;
        return;
    }

    if (o->state == 1) {
        if (o->timer > 0x1E && !(o->timer & 3))
            o->gore_fx = 1;     /* one sub_0806D5C0 burst */
    } else if (o->state == 2) {
        if (!(o->timer & 3)) {
            (void)random();     /* sub_0802D5EC(0x20) x jitter */
            (void)random();     /* sub_0802D5EC(0x10) - 0x18 y */
            o->child_fx = 1;    /* type-0x1B child, counted */
        }
    } else {
        return;
    }

    /* Shared + 0xD countdown; the 0xFF wrap sets + 0x59 bit 3. */
    o->timer--;
    if (o->timer == 0xFF)
        o->finished = true;
}
