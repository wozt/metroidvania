/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_flame_demon.h"

#include <stdint.h>

static int32_t cap_speed(int32_t v) {
    /* sub_0806D3D8(e, 0x80000): clamp each axis to +/-0x80000. */
    if (v > 0x80000) return 0x80000;
    if (v < -0x80000) return -0x80000;
    return v;
}

AosFlameSpirit aos_flame_bolt_create(int32_t x, int32_t y, uint32_t angle,
                                     bool mirrored) {
    AosFlameSpirit bolt = {.x = x, .y = y, .state = 1, .mirrored = mirrored};
    bolt.vx = (int32_t)((int64_t)aos_sine(angle + 0x4000u) * 2);
    bolt.vy = (int32_t)((int64_t)aos_sine(angle) * 2);
    if (mirrored) bolt.vx = -bolt.vx;
    return bolt;
}

AosFlameSpirit aos_flame_spirit_create(int32_t x, int32_t y, bool mirrored) {
    AosFlameSpirit wisp = {.x = x, .y = y, .state = 1, .mirrored = mirrored,
                           .lord = true, .spin_rate = (int16_t)0x234};
    wisp.vx = mirrored ? -0x18000 : 0x18000;
    wisp.vy = mirrored ? -0x400 : 0x400;
    if (mirrored) wisp.spin_rate = (int16_t)-wisp.spin_rate;
    return wisp;
}

bool aos_flame_spirit_update(AosFlameSpirit *spirit, bool anim_finished,
                             bool outside, bool *puff) {
    if (spirit->state == 2) return false;
    if (puff) *puff = false;
    spirit->state = 1;
    spirit->vx = cap_speed(spirit->vx);
    spirit->vy = cap_speed(spirit->vy);
    spirit->x += spirit->vx;
    spirit->y += spirit->vy;
    if (spirit->lord) {
        /* + 0x2C += + 0x2E rotation accumulator (sub_0803E058 applies it
         * to the affine draw — visual only here). */
        spirit->spin = (uint16_t)(spirit->spin + (uint16_t)spirit->spin_rate);
        spirit->timer = (uint8_t)(spirit->timer + 1);
        if ((spirit->timer & 7) == 0 && puff) *puff = true;
    }
    /* The native tail: sub_0806D128 margins (n = 2 bolts / 3 wisp) or the
     * animation end moves the entity to state 2, deleted next update. */
    if (anim_finished || outside) spirit->state = 2;
    return true;
}
