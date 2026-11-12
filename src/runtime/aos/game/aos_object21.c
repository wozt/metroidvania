#include "aos_object21.h"

/* sub_0806D460(e, 0x70000): + 0x4C += + 0x54, clamp to
 * +/- 0x70000, + 0x44 += + 0x4C. */
static void physics_step(AosObject21 *o) {
    o->vel += o->accel;
    if (o->vel > 0x70000)
        o->vel = 0x70000;
    else if (o->vel < -0x70000)
        o->vel = -0x70000;
    o->pos_y += o->vel;
    o->pos_y_px = (int16_t)(o->pos_y >> 16);
}

void aos_object21_create(AosObject21 *o, uint16_t room_ox,
                         uint16_t room_oy, int cam_byte,
                         bool *deleted) {
    *deleted = false;
    o->state = 0;
    o->timer = 0;
    o->vel = 0;
    o->accel = 0x2800;          /* create: 0xA0 << 6 */
    o->pos_x = 0;
    o->pos_y = (int32_t)(0x18 - (int)room_oy) << 16;
    o->pos_x_px = (uint16_t)(0x40 - room_ox);
    o->pos_y_px = (int16_t)(0x18 - (int)room_oy);
    o->flag6d = 0;
    o->cam_byte = (uint8_t)cam_byte;
    o->deleted = false;
    o->song = -1;
    o->anim_seq = -1;
}

void aos_object21_update(AosObject21 *o, int16_t room_oy,
                         uint8_t flag6d) {
    o->song = -1;
    o->anim_seq = -1;
    o->flag6d = flag6d;
    int16_t upper = (int16_t)(0x7F - room_oy);

    switch (o->state) {
    case 0:
        physics_step(o);
        if (o->flag6d == 0 &&
            o->pos_y_px >= (int16_t)(0x24 - room_oy))
            o->anim_seq = 3;    /* sub_0803F2C8(3, 0, 1) */
        if (o->pos_y_px < upper)
            break;
        o->pos_y_px = upper;
        o->vel = -0x14000;      /* -1.25 px/frame */
        o->state = 1;
        o->timer = 0;
        o->song = 0x7F;
        break;
    case 1:
        physics_step(o);
        if (o->pos_y_px < upper)
            break;
        o->pos_y_px = upper;
        o->vel = 0;
        o->state = 2;
        o->timer = 0;
        break;
    case 2:
        if (o->timer > 0x3B) {  /* old + 0xD > 0x3B */
            o->state = 3;
            o->timer = 0;
        } else {
            o->timer++;
        }
        break;
    case 3:
        o->vel = -0x10000;      /* -1.0 px/frame */
        o->pos_y += o->vel;
        o->pos_y_px = (int16_t)(o->pos_y >> 16);
        if (o->flag6d == 3 &&
            o->pos_y_px <= (int16_t)(0x24 - room_oy))
            o->anim_seq = 0;    /* sub_0803F2C8(0, 0, 1) */
        if (o->pos_y_px > (int16_t)(0x18 - room_oy))
            break;
        o->pos_y_px = (int16_t)(0x18 - room_oy);
        o->state = 4;
        o->timer = 0;
        break;
    case 4:
        if (o->timer > 0x3B) {
            o->state = 0;
            o->timer = 0;
            o->vel = 0;
        } else {
            o->timer++;
        }
        break;
    }
}
