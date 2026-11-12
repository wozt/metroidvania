#include "aos_object16.h"

void aos_object16_create(AosObject16 *o, int param) {
    o->param = (uint8_t)param;
    o->state = 0;
    o->tick = 0;
    o->counter = 0xC0;
    o->zoom = 0xF0000;
    o->scroll = 0x40;
    o->scroll_acc = 0;
    o->forced = -1;
    o->walk_dir = 0;
    o->song = -1;
    o->fade = false;
    o->dialogue = false;
    o->particles = 0;
}

void aos_object16_update(AosObject16 *o, bool input_flag0, bool near,
                         bool soma_left,
                         int32_t (*sine)(uint32_t phase)) {
    o->forced = -1;
    o->walk_dir = 0;
    o->song = -1;
    o->fade = false;
    o->dialogue = false;
    o->particles = 0;

    /* + 0x20 += + 0x24 every frame (the scroll accumulator the
     * probe block precomputes). */
    o->scroll_acc += o->scroll;

    switch (o->state) {
    case 0:
        if (input_flag0) {
            /* + 0x42C bit 0 set, sub_0800C5A8(0), input locked. */
            o->state = 1;
            o->forced = 0;
        }
        break;
    case 1:
        /* Direction toward the + 0x13110 entity: entity fx <=
         * soma fx -> 0x20 else 0x10. */
        o->forced = 1;
        o->walk_dir = soma_left ? 0x20 : 0x10;
        if (near)
            o->state = 2;
        break;
    case 2:
        o->forced = 0;
        /* sub_080235C8 + sub_0802DFF0 side effects unmodelled. */
        o->state = 3;
        o->song = 0x157;
        break;
    case 3:
        o->forced = 0;
        if (o->counter > 0) {
            int32_t s = sine(((uint32_t)o->counter << 6) + 0x400000);
            if (s < 0)
                s += 0xF;
            o->zoom = 0x1000 - ((uint32_t)(s >> 4) & 0xFFFFFFFFu);
            o->counter--;
            o->scroll += 2;
            if (o->counter == 0x40) {
                o->song = 0x1AE;
                o->fade = true;     /* sub_0803D18C/sub_0803CDF0 */
            }
        } else {
            o->state = 4;
        }
        break;
    case 4:
        o->forced = 0;
        /* + 0x42C &= ~2, |= 0x8000; the 0x08522ABC record. */
        o->dialogue = true;
        break;
    }

    /* Tail: BgAffineSet on + 0x1C (exposed as `zoom`), then a
     * type-0x49 particle on + 0xD parity. */
    o->tick = (uint8_t)(o->tick + 1);
    if (o->tick & 1)
        o->particles = 1;
}
