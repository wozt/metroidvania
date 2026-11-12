#include "aos_object27.h"

void aos_object27_create(AosObject27 *o, uint8_t variant) {
    o->phase = 0;
    o->timer = 0;
    o->variant = variant;
    o->spin = 0;
    o->angle = 0;
    o->attack_armed = true;     /* + 0x72 bits 1-2 preset */
    o->done = true;             /* + 0x58 bit 0x20 at create */
    o->preset_refreshed = false;
}

static void spin_step(AosObject27 *o) {
    /* sub_0803E238(0, + 0x48, flags): the angular application is
     * exposed as the accumulator. */
    o->angle += o->spin;
}

void aos_object27_update(AosObject27 *o, bool target_far) {
    switch (o->phase) {
    case 0:
        if (target_far) {
            /* The + 0x14 entity + 0x18 u16 clears (approximated
             * by the input). */
            o->phase = 1;
            o->spin = 0xC000;
            spin_step(o);
        }
        break;
    case 1:
        if (o->spin <= 0xFFFF) {
            o->spin += 0x800;
        } else {
            o->spin = 0x40000;
            o->phase = 2;
            o->timer = 0x1E;
            o->preset_refreshed = true; /* sub_08042848 */
            o->attack_armed = false;    /* + 0x72 &= ~2 */
        }
        spin_step(o);
        break;
    case 2:
        /* + 0xD counts down; the transition fires when the OLD
         * u8 value is <= 1 (the stored value wraps). */
        if (o->timer <= 1) {
            o->timer = (uint8_t)(o->timer - 1);
            o->phase = 3;
            o->attack_armed = true;     /* + 0x72 |= 2 */
        } else {
            o->timer = (uint8_t)(o->timer - 1);
        }
        break;
    case 3:
        if (o->spin > 0xC000) {
            o->spin -= 0x800;
            spin_step(o);
        } else {
            o->phase = 0;
            o->done = true;             /* + 0x58 |= 0x20 */
        }
        break;
    }
    /* sub_0803F17C + sub_080421AC run every update natively. */
}
