#include "aos_object2e.h"

void aos_object2e_create(AosObject2E *o) {
    o->triggered = 0;
    o->anim = 1;                    /* create-time sub_0803F17C */
    o->children = 0;
    o->song = -1;
}

void aos_object2e_update(AosObject2E *o, bool hit) {
    if (!o->triggered && hit) {
        o->triggered = 1;
        o->children = 3;            /* sub_080589A4 x3 */
        o->song = 0x78;
    }
}
