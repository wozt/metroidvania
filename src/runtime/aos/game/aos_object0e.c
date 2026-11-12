#include "aos_object0e.h"

/* Object0ECreate @ 0x08050DCC / Object0EUpdate @ 0x08050CF6 — the
 * config record is 12 bytes: byte 0 selects the resource triple,
 * byte 2 the animation (always 0 in the shipped table), bytes 3-7
 * the debris/content parameters, + 8 the attack preset. */

/* 0x085262BC has 15 records (param0 0-14); byte 0 indexes the
 * 0x08526214 resource table (12 records) and byte 3 is the content
 * child count handed to sub_08050A3C. */
static const uint8_t cfg_res[15] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13
};
static const uint8_t cfg_count[15] = {
    3, 3, 3, 3, 3, 3, 3, 2, 1, 1, 1, 1, 1, 5, 5
};
/* 0x08526370[param1]: 14 records of {selector, argument} bytes handed
 * to sub_08045F9C; every selector case converges on the same break
 * path, so the port only needs the hit boolean (see header). */

void aos_object0e_create(AosObject0e *o, int param0, int param1,
                         bool *deleted) {
    *deleted = false;
    int cfg = param0 > 14 ? 14 : param0;
    o->param0 = (uint8_t)param0;
    o->param1 = (uint8_t)param1;
    o->variant = cfg_res[cfg];
    o->state = 0;
    o->timer = 0;
    o->flags58 = 0;
    o->finished = false;
    o->fx = 0;
    o->fy = 0;
    o->song = -1;
    o->content_fx = 0;
    o->gore_fx = 0;
    if (cfg_res[cfg] >= 14) {
        /* Native load failure deletes the entity. */
        *deleted = true;
    }
}

void aos_object0e_update(AosObject0e *o, bool near, bool hit,
                         int32_t fx, int32_t fy) {
    o->fx = fx;
    o->fy = fy;
    if (!near) {
        o->flags58 |= 0x20;
        return;
    }
    if (o->state == 0) {
        o->flags58 &= ~(uint8_t)0x21;
        if (hit) {
            /* sub_08050A3C releases cfg-count content children sharing
             * the prop resource set; sub_08045F9C dispatches on the
             * 0x08526370[param1] byte pair — all cases converge on the
             * same break path here. */
            int cfg = o->param0 > 14 ? 14 : o->param0;
            o->content_fx = cfg_count[cfg];
            o->song = 0xA2;
            o->flags58 |= 0x20;
            o->timer = 0x28;
            o->state = 1;
        }
        return;
    }
    /* state 1: gore burst every fourth frame while +0xD > 0x1E. */
    if (o->timer > 0x1E && (o->timer & 3) == 0) {
        o->gore_fx += 4;
    }
    o->timer--;
    if ((uint8_t)o->timer == 0xFF) {
        o->finished = true;    /* +0x59 bit 3 */
    }
}
