#include "aos_object34.h"

void aos_object34_create(AosObject34 *o, int16_t param0,
                         uint16_t param1, int32_t scroll_x,
                         int32_t scroll_y, int32_t x, int32_t y) {
    o->deleted = false;
    o->offset = 0x4000;                 /* + 0x18 */
    o->accel = param1;                  /* + 0x1C = + 0x32 u16 */
    o->child_count = 3;                 /* sub_08059E4C x3 */
    if (param0 == 0) {
        o->phase = 0;
        o->vel = 0x100;
    } else {
        o->phase = 1;
        o->vel = (int16_t)0xFF00;        /* -256 */
    }
    /* + 0x10/+ 0x14 = entity pos + scroll; + 0x24/+ 0x28 and
     * + 0x40/+ 0x44 start as the screen copies. */
    o->world_x = x + scroll_x;
    o->world_y = y + scroll_y;
    o->screen_x = x;
    o->screen_y = y;
}

void aos_object34_update(AosObject34 *o, int32_t scroll_x,
                         int32_t scroll_y) {
    /* Screen copies refreshed from world - scroll. */
    o->screen_x = o->world_x - scroll_x;
    o->screen_y = o->world_y - scroll_y;

    if (o->phase == 0) {
        /* + 0x1A -= + 0x1C floored at -0x100 (s16), then
         * + 0x18 += + 0x1A; flip when + 0x18 << 16 >
         * 0x3FFF0000 i.e. the s16.16 product exceeds it. */
        int32_t v = o->vel - (int16_t)o->accel;
        if (v < -0x100) v = -0x100;
        o->vel = (int16_t)v;
        o->offset = (uint16_t)(o->offset + o->vel);
        if (((int32_t)(int16_t)o->offset << 16) > (int32_t)0x3FFF0000)
            o->phase = 1;
    } else {
        /* + 0x1A += + 0x1C ceiled at 0x100, then the same add;
         * flip when the product exceeds 0x40000000. */
        int32_t v = o->vel + (int16_t)o->accel;
        if (v > 0x100) v = 0x100;
        o->vel = (int16_t)v;
        o->offset = (uint16_t)(o->offset + o->vel);
        if (((int32_t)(int16_t)o->offset << 16) > (int32_t)0x40000000)
            o->phase = 0;
    }

    /* Epilogue: + 0x40/+ 0x44 = world - scroll (same values as
     * the screen copies just refreshed). */
}
