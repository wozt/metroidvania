#include "aos_object2f.h"

static const uint16_t anchor_x[3] = {0x30, 0x78, 0xA8};
static const uint16_t anchor_y[3] = {0x38, 0x48, 0x30};

void aos_object2f_create(AosObject2F *o, uint16_t special_37e,
                          int16_t param0) {
    /* + 0x37E bit 2: EntityDelete */
    if (special_37e & 4) {
        o->deleted = true;
        return;
    }
    o->deleted = false;
    unsigned idx = param0 >= 0 && param0 < 3 ? (unsigned)param0 : 0;
    o->anchor_x = anchor_x[idx];
    o->anchor_y = anchor_y[idx];
    o->anim = 1;                        /* the create-time sub_0803F17C step */
    o->stopped = 0;                     /* + 0xB */
    o->mark = 0x20;                     /* + 0x58 bit 0x20 */
}

void aos_object2f_update(AosObject2F *o, bool r_pressed) {
    /* sub_08058D44: + 0xA == 0 gate is applied by the caller's
     * dispatch; here the + 0xB == 0 && + 0x42C bit-0x1000 test
     * latches + 0xB and clears the + 0x58 bit 0x20 mark. */
    if (!o->stopped && r_pressed) {
        o->stopped = 1;
        o->mark &= (uint8_t)~0x20;
    }
    o->anim = (uint8_t)(o->anim + 1);   /* sub_0803F17C */
}
