#include "aos_object19.h"

/* 0x0852646C legs armed on each state's expiry, indexed by the
 * NEXT state: state 0 = right param0, 1 = down 22, 2 = left
 * param0, 3 = up 22. A zero record lifetime uses param0 << 16. */
static const int32_t leg_vx[4] = {0x10000, 0, -0x10000, 0};
static const int32_t leg_vy[4] = {0, 0x10000, 0, -0x10000};
static const int32_t leg_time[4] = {0, 0x160000, 0, 0x160000};

/* sub_08053D90-style arm: record[state] supplies + 0x48/+ 0x4C
 * and the + 0x18 lifetime (param0 << 16 when the record word is
 * 0); a set mirror negates + 0x48. Object19Create runs the same
 * arm for the + 0xA = 0 record and then arithmetic-shifts the
 * lifetime right once (asrs #1 at 0x08053CE0). */
static void arm_leg(AosObject19 *o, bool halve) {
    int32_t vx = leg_vx[o->state];
    if (o->mirror) vx = -vx;
    o->vx = vx;
    o->vy = leg_vy[o->state];
    int32_t time = leg_time[o->state];
    /* + 0x30 is an s16 field: param0 + 0xE wraps mod 0x10000
     * and sign-extends before the << 16. */
    o->lifetime = time ? time
                       : (int32_t)((uint32_t)(uint16_t)(int16_t)o->param0
                                   << 16);
    if (halve) o->lifetime >>= 1;
}

void aos_object19_create(AosObject19 *o, int param0, int param1,
                         int32_t fx, int32_t fy) {
    /* Object19Create (0x08053BFC): + 0x30 += 0xE feeds the
     * param0 << 16 leg lifetime, + 0x32 collapses to param1 bit
     * 0 (the + 0x58 bit-6 mark, the + 0x16 negate and the leg
     * vx negate all read the collapsed byte), + 0x16 = 0x400
     * fixed and negated when the mirror is set, then the
     * + 0xA-indexed record arms the first leg. The hurt slot,
     * the 0x0C0CFAFA presets and the sub_0803E654 palette roll
     * stay under special_sprites. */
    o->param0 = param0 + 0xE;
    o->mirror = (uint8_t)(param1 & 1);
    o->state = 0;
    o->lifetime = 0;
    o->vx = 0;
    o->vy = 0;
    o->fx = fx;
    o->fy = fy;
    o->angle = 0;
    o->angle_rate = o->mirror ? (uint16_t)-0x400 : (uint16_t)0x400;
    o->flags58 = (uint8_t)(o->mirror << 6);
    arm_leg(o, true);
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
        arm_leg(o, false);
    }
}
