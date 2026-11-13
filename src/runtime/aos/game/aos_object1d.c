#include "aos_object1d.h"

void aos_object1d_create(AosObject1D *o, int16_t param0,
                         int32_t pos_x, int32_t pos_y, int sum,
                         bool *deleted) {
    *o = (AosObject1D){0};
    *deleted = false;
    o->param0 = param0;
    o->off_x = (int16_t)pos_x;              /* +0x42 seed */
    o->off_y = (int16_t)pos_y;              /* +0x46 seed */
    o->placements = 4;                      /* sub_08002200 x4 */
    o->anim = 0x11;
    if (param0 == 0) {
        o->sel = 0xB;
        o->frame_count = 0xC;
        if (sum == 1) {
            o->off_x = (int16_t)(o->off_x - 0xD0);
            o->facing = 1;
            o->flags58 &= (uint8_t)~0x41;
        } else if (sum == -1) {
            o->facing = -1;
            o->flags58 |= 0x40;
        } else if (sum == 0) {
            o->deleted = true;
            *deleted = true;
            return;
        }
    } else {
        o->sel = 0x14;
        o->frame_count = 0xA;
        if (sum == 1) {
            o->off_x = 0x10;
            o->off_y = 0x70;
            o->facing = 1;
            o->flags58 &= (uint8_t)~0xB1;
        } else if (sum == -1) {
            o->off_x = 0xE0;
            o->off_y = 0x70;
            o->facing = 0;
            o->flags58 |= 0x40;
        } else if (sum == 0) {
            o->deleted = true;
            *deleted = true;
            return;
        }
    }
}
