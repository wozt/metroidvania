#include "aos_object2c.h"

void aos_object2c_create(AosObject2C *o) {
    static const uint16_t y_off[4] = {0x1E, 0x3E, 0x56, 0x66};
    static const uint16_t x_step[4] = {0x4000, 0x3000, 0x2000, 0x1000};
    o->deleted = false;
    o->child_count = 4;
    for (int i = 0; i < 4; ++i) {
        o->y_off[i] = y_off[i];
        o->x_step[i] = x_step[i];
        o->layer[i] = (uint8_t)i;
    }
}

void aos_object2c_update(AosObject2C *o) {
    /* Object2CUpdate is a bare frame natively — the fountain's
     * parent entity never moves; its four children do the work
     * through sub_0805866C (not spawned in the port). */
    (void)o;
}
