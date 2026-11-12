#include "aos_object30.h"

void aos_object30_create(AosObject30 *o, bool *deleted) {
    /* sub_08058FE8's triple is the shared Object2F art
     * (special_47); the runtime asset table decides the load. */
    *deleted = false;
    o->gate = 0;
    o->timer = 0;
    o->spawn_fx = 0;
    o->deleted = false;
}

void aos_object30_update(AosObject30 *o, uint32_t global_flags,
                         AosRandom random) {
    o->spawn_fx = 0;
    if (o->gate == 0) {
        /* + 0xB == 0: latch on global bit 0x1000. */
        if (global_flags & 0x1000)
            o->gate = 1;
        return;
    }
    if (o->gate != 1)
        return;
    /* + 0xB == 1: the OLD u8 + 0xD at 0 or 1 fires the child
     * and reloads the timer to (RNG & 0x7F) + 0x20. */
    uint8_t old = o->timer;
    o->timer = (uint8_t)(old - 1);
    if (old <= 1) {
        o->timer = (uint8_t)((random() & 0x7F) + 0x20);
        o->spawn_fx = 1;
    }
}
