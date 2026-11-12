/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_man_eater.h"

#define DRIFT_CAP 0x80000

static int32_t clamp_drift(int32_t v) {
    if (v > DRIFT_CAP) return DRIFT_CAP;
    if (v < -DRIFT_CAP) return -DRIFT_CAP;
    return v;
}

AosManEater aos_man_eater_create(int32_t soma_x, int32_t soma_y,
                                 bool facing_left, uint32_t (*rng)(void)) {
    AosManEater m;
    /* sub_08031EA4(source, 8, -0x1A): 8 px along the facing, 0x1A px
     * up; the + 0x58 facing bit is the source's (not inverted). */
    m.x = ((soma_x >> 16) + (facing_left ? -8 : 8)) << 16;
    m.y = soma_y - (0x1A << 16);
    m.vx = facing_left ? -0x12000 : 0x12000;
    m.vy = 0;
    m.tick = 0;
    m.timer = 0;
    m.flag_f = 0;
    /* Two RNG draws natively; the second feeds __umodsi3(3). */
    (void)rng();
    m.variant = (uint8_t)(rng() % 3);
    m.base = (uint8_t)(m.variant * 5);
    m.frame = m.base;
    m.hit = false;
    m.state = 1;
    m.outside = false;
    m.marked = false;
    m.released = false;
    return m;
}

static void release(AosManEater *m) {
    /* gEwramData + 0x13124[+ 0x1A * 4] = 0 and + 0x131BE--: the
     * per-slot table and counter are global bookkeeping with no
     * modelled reader — exposed through `released`. */
    m->released = true;
}

bool aos_man_eater_update(AosManEater *m, uint32_t (*rng)(void)) {
    (void)rng;
    switch (m->state) {
    case 1:
        /* + 0x65 = + 0xC + (+ 0x24 / 10) before the counter grows. */
        m->frame = (uint8_t)(m->base + m->tick / 10);
        m->vx = clamp_drift(m->vx);
        m->vy = clamp_drift(m->vy);
        m->x += m->vx;
        m->y += m->vy;
        m->tick++;
        if (m->tick > 0x1D) {
            m->timer = 0x30;
            m->state = 2;
        }
        break;
    case 2:
        m->vx = clamp_drift(m->vx);
        m->vy = clamp_drift(m->vy);
        m->x += m->vx;
        m->y += m->vy;
        if (m->timer == 0) {
            m->flag_f = 1;
            m->timer = 0x10;
            m->state = 3;
        } else {
            m->timer--;
        }
        break;
    case 3:
        m->vx = clamp_drift(m->vx);
        m->vy = clamp_drift(m->vy);
        m->x += m->vx;
        m->y += m->vy;
        if (m->timer == 0) {
            release(m);
            return false;
        }
        m->timer--;
        break;
    case 4:
        m->vx = clamp_drift(m->vx);
        m->vy = clamp_drift(m->vy);
        m->x += m->vx;
        m->y += m->vy;
        if (m->timer == 0) {
            /* _0802C884: back into state 3 with a fresh countdown. */
            m->flag_f = 1;
            m->timer = 0x10;
            m->state = 3;
        } else {
            m->timer--;
        }
        break;
    case 5:
        release(m);
        return false;
    }
    /* The shared tail: a contact while the state is <= 2 sticks the
     * bite — vx zeroed, + 0x65 = + 0xC + 4, state 4, + 0xD = 0x10. */
    if (m->state <= 2 && m->hit) {
        m->vx = 0;
        m->frame = (uint8_t)(m->base + 4);
        m->state = 4;
        m->timer = 0x10;
    }
    /* sub_0806D128(e, 2) or the + 0x59 bit-4 mark deletes through the
     * same bookkeeping as state 5. */
    if (m->outside || m->marked) {
        release(m);
        return false;
    }
    return true;
}
