#include "aos_object11.h"

void aos_object11_create(AosObject11 *o) {
    o->helper = true;
    o->removed_save = true;   /* EntityDelete on the + 0x124C slot */
    o->held_ticks = 0;
}

void aos_object11_update(AosObject11 *o, bool held) {
    /* sub_080512FC: + 0x59 bit 4 is set once at spawn; + 0x14 counts
     * the frames the + 0x42C bit-1 hold lasts, capped at 0x34BBD. */
    if (held && o->held_ticks < 0x34BBD)
        o->held_ticks++;
}
