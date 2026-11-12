#include "aos_object2b.h"

void aos_object2b_create(AosObject2B *o) {
    o->anim = 1;    /* the create-time sub_0803F17C step */
}

void aos_object2b_update(AosObject2B *o) {
    o->anim = (uint8_t)(o->anim + 1);   /* sub_0803F17C */
}
