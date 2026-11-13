#include "aos_object33.h"

void aos_object33_create(AosObject33 *o, int param, bool sprite_ok,
                         bool *deleted) {
    *o = (AosObject33){0};
    *deleted = false;
    o->param = (uint8_t)(param & 0xFF);
    if (!sprite_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    /* sub_0803F2C8(param == 1, 0, 1) + sub_0803F17C. */
    o->flags5a = 2;                     /* (+ 0x5A & ~7) | 2 */
    /* + 0x5C = 9 auxiliary timer, not consumed by the port. */
    o->byte58 = (uint8_t)(o->byte58 & 0x4A);
    if (param == 0) o->byte58 |= 0x20;
    o->state = 0;
    o->timer = 0;                       /* + 0xD starts 0: the
                                         * first tick fires. */
}

void aos_object33_update(AosObject33 *o, uint32_t (*rng)(void)) {
    o->burst_a = false;
    o->burst_b = false;
    if (o->state != 0) return;
    uint8_t old = o->timer;
    o->timer = (uint8_t)(old - 1);
    if (old > 1) return;
    o->timer = (uint8_t)((rng() & 0xF) + 0x10);
    o->burst_a = true;                  /* sub_08059928 */
    if (o->param != 0) o->burst_b = true;   /* sub_08059A8C */
}
