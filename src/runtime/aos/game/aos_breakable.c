/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_breakable.h"

#include <string.h>

void aos_breakable_create(AosBreakable *b, int param0, int param1) {
    memset(b, 0, sizeof *b);
    b->param0 = (uint8_t)param0;
    b->param1 = (uint8_t)param1;
    b->song = -1;
    b->state = 0;
    /* sub_0803F2C8(e, 3, 0, 1) + sub_0803F17C: animation 3 from
     * 0x0820ED60 plays from the first frame; the port's draw picks
     * the exported animation directly, so no anim state is kept. */
}

void aos_breakable_update(AosBreakable *b, bool near, bool hit, bool swing,
                          bool mp_full_soma) {
    b->song = -1;
    ++b->anim_tick;         /* sub_08046100 frame step, once per update */
    b->drop_kind = AOS_BREAKABLE_DROP_NONE;
    b->drop_big = false;
    b->drop_roll = false;
    b->gore_fx = 0;
    /* sub_08046100 runs before the update natively (drawFunc order is
     * approximated): the bounce only marks this frame's draw. */
    b->swing_bounce = swing;
    if (!near) {
        /* Probe miss: + 0x58 |= 0x20, update ends. */
        b->flags58 |= 0x20;
        return;
    }
    if (b->state == 0) {
        /* + 0x58 bits 0 and 5 cleared, then the hit check. */
        b->flags58 &= ~(uint8_t)0x21;
        if (hit) {
            b->song = 0xA2;
            b->flags58 |= 0x20;
            b->timer = 0x28;
            b->state = 1;
            /* sub_08045F9C case 0 (the + 0x36 selector reads 0 — no
             * record field or create writes it): full MP on Soma
             * drops sub_08043F70 of variant (RandomNumberGenerator
             * () & 1) + param1, anything else a sub_08043C10 heart
             * with big = (param1 != 0), pop = 0. */
            if (mp_full_soma) {
                b->drop_kind = AOS_BREAKABLE_DROP_MONEY;
                b->drop_roll = true;
            } else {
                b->drop_kind = AOS_BREAKABLE_DROP_HEART;
                b->drop_big = b->param1 != 0;
            }
        }
        return;
    }
    if (b->state == 1) {
        if (b->timer > 0x1E && (b->timer & 3) == 0)
            b->gore_fx++;
        if (--b->timer == 0xFF) {
            /* + 0x59 |= 8: the prop is deleted. */
            b->finished = true;
        }
    }
}
