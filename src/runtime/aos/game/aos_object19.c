#include "aos_object19.h"

/* 0x0852646C legs armed on each state's expiry, indexed by the
 * NEXT state: state 0 = right param0, 1 = down 22, 2 = left
 * param0, 3 = up 22. A zero record lifetime uses param0 << 16. */
static const int32_t leg_vx[4] = {0x10000, 0, -0x10000, 0};
static const int32_t leg_vy[4] = {0, 0x10000, 0, -0x10000};
static const int32_t leg_time[4] = {0, 0x160000, 0, 0x160000};

void aos_object19_create(AosObject19 *o, int param0, int param1,
                         int32_t fx, int32_t fy, uint16_t angle_rate) {
    o->param0 = param0;
    o->param1 = param1;
    o->state = 0;
    o->lifetime = 0;
    o->vx = 0;
    o->vy = 0;
    o->fx = fx;
    o->fy = fy;
    o->angle = 0;
    o->angle_rate = angle_rate;
    o->flags58 = 0;
}

static void arm_leg(AosObject19 *o) {
    int32_t vx = leg_vx[o->state];
    if (o->param1 != 0) vx = -vx;
    o->vx = vx;
    o->vy = leg_vy[o->state];
    int32_t time = leg_time[o->state];
    o->lifetime = time ? time : o->param0 << 16;
}

void aos_object19_update(AosObject19 *o, bool near) {
    if (!near) {
        o->flags58 |= 0x20;
        return;
    }
    o->flags58 &= (uint8_t)~0x21;
    o->angle = (uint16_t)(o->angle + o->angle_rate);

    /* Every state integrates the velocity; a live leg counts down
     * and an expired one advances to the next leg's record. */
    o->fx += o->vx;
    o->fy += o->vy;
    if (o->lifetime > 0) {
        o->lifetime -= 0x10000;
    } else {
        o->state = (uint8_t)((o->state + 1) & 3);
        arm_leg(o);
    }
}
