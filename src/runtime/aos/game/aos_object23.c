#include "aos_object23.h"

void aos_object23_create(AosObject23 *o) {
    o->anim = 1;    /* the create-time sub_0803F17C step */
}

void aos_object23_update(AosObject23 *o) {
    o->anim = (uint8_t)(o->anim + 1);   /* sub_0803F17C */
}
