#include "aos_object2d.h"
#include <stdlib.h>

static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

static void preset(AosObject2D *o, int p, bool gate) {
    if (gate)
        o->preset = p;              /* sub_0803F2C8(p, 0, 0) */
}

void aos_object2d_create(AosObject2D *o) {
    o->phase = 0xA;
    o->flags = 0;
    o->marks = 0;
    o->anim = 1;                    /* create-time sub_0803F17C */
    o->preset = 2;                  /* sub_0803F2C8(2, 0, 0) */
    o->attack = 0;
}

void aos_object2d_update(AosObject2D *o, int32_t dx, int32_t dy) {
    bool in_window = iabs(dx) <= 0x4FFFFF && iabs(dy) <= 0x5FFFFF;
    bool bit0 = (o->marks & 1) != 0;

    if (o->phase == 0xA) {
        /* Dormant preset swaps. */
        switch (o->flags) {
        case 0: preset(o, 1, bit0); break;
        case 1: preset(o, 2, bit0); break;
        case 3: case 4: preset(o, 0, bit0); break;
        }
        if (in_window)
            o->phase = 0xC;
    } else if (o->phase == 0xC) {
        /* Active preset swaps. */
        switch (o->flags) {
        case 1: preset(o, 3, bit0); break;
        case 2: preset(o, 4, bit0); break;
        case 3: case 4: preset(o, 0, bit0); break;
        }
        if (!in_window) {
            o->phase = 0xA;
        } else {
            o->attack++;            /* sub_0806D128(ent, 3) */
        }
    }
    o->anim = (uint8_t)(o->anim + 1);   /* sub_0803F17C */
}
