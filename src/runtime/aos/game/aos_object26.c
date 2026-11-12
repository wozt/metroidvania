#include "aos_object26.h"

void aos_object26_create(AosObject26 *o) {
    o->counter_a = 0x1E;    /* + 0x18 */
    o->counter_b = 0x5A;    /* + 0x1A */
    o->done = true;         /* + 0x58 bit 0x20 at create */
}

void aos_object26_update(AosObject26 *o, bool soma_inside) {
    /* sub_08057F4C: + 0x58 bit 0x20 is set on hit and miss alike;
     * only a hit moves the counters (u16 wraps like strh). */
    o->done = true;
    if (soma_inside) {
        o->counter_a = (uint16_t)(o->counter_a + 1);
        o->counter_b = (uint16_t)(o->counter_b + 1);
    }
}
