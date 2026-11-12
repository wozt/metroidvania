#include "aos_object24.h"

void aos_object24_create(AosObject24 *o, uint32_t r1, uint32_t r2) {
    o->phase = (uint8_t)(r1 & 0xF);
    o->variant = (uint8_t)(r2 % 3);      /* __umodsi3 by 3 */
    o->tick = 0;
    o->anim = 0;
    o->hidden = false;
    o->advanced = false;
}

void aos_object24_update(AosObject24 *o, uint32_t rng) {
    o->advanced = (rng & 0xFF) <= 0xDF;
    if (o->advanced)
        o->anim++;                      /* sub_0803F17C */
    o->hidden = (o->tick & 1) != 0;     /* old + 0xD parity */
    o->tick = (uint8_t)(o->tick + 1);
}
