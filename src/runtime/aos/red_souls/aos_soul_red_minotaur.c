/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_minotaur.h"

#include "../math/aos_math.h"

#define CHARGE_CAP 0x1FFFF
#define SPIN_UP_STEP 0x2000
#define ANGLE_STEP_INIT (-0x800)
#define ACCEL 0x40
#define TIMER_INIT 0x50
#define ORBIT_RADIUS 80

AosRedMinotaur aos_red_minotaur_create(int32_t soma_x, int32_t soma_y,
                                       bool facing_left) {
    AosRedMinotaur m;
    m.x = soma_x;
    m.y = soma_y - 0x1A0000; /* sub_08031EA4(source, 0, -0x1A) */
    m.angle = facing_left ? -0x4000 : 0x4000;
    m.step = facing_left ? -ANGLE_STEP_INIT : ANGLE_STEP_INIT;
    m.accel = facing_left ? -ACCEL : ACCEL;
    m.charge = 0;
    m.facing_left = facing_left;
    m.state = 1; /* state 0 falls through to state 1 in the same update */
    m.timer = TIMER_INIT;
    m.hidden = false;
    m.outside = false;
    return m;
}

bool aos_red_minotaur_update(AosRedMinotaur *m) {
    if (m->state == 1) {
        if (m->charge <= CHARGE_CAP) {
            m->charge += SPIN_UP_STEP;
        } else {
            m->state = 2;
        }
    } else if (m->state == 2) {
        m->angle = (int16_t)(m->angle + m->step);
        m->step = (int16_t)(m->step + m->accel);
        if (m->timer == 0) {
            m->state = 3;
        } else {
            m->timer--;
        }
    } else if (m->state == 3) {
        m->angle = (int16_t)(m->angle + m->step);
        m->step = (int16_t)(m->step - 2 * m->accel);
        if (m->charge > 0) {
            m->charge -= SPIN_UP_STEP;
        } else {
            m->state = 4;
        }
    } else if (m->state == 4) {
        return false;
    }
    /* The shared tail: sub_0806D128(e, 4) or + 0x59 bit 4 forces the
     * delete mark (+ 0x59 bit 4, state 4, + 0x58 bit 5). */
    if (m->outside || m->hidden) {
        m->hidden = true;
        m->state = 4;
    }
    return true;
}

void aos_minotaur_blade_init(AosMinotaurBlade *blade) {
    blade->x = 0;
    blade->y = 0;
    blade->state = 1; /* state 0 installs the box, then enters 1 */
    blade->attacking = true; /* + 0x72 |= 2 | 4 at init */
    blade->alive = true;
}

bool aos_minotaur_blade_update(AosMinotaurBlade *blade,
                               const AosRedMinotaur *anchor) {
    if (!anchor || anchor->state == 4) {
        blade->alive = false;
        return false;
    }
    /* sub_08026858's child-state machine: armed at init, disarmed
     * while the anchor spins up (state 2), re-armed in state 3. */
    if (anchor->state == 2 && blade->state == 1) {
        blade->attacking = false;
        blade->state = 2;
    } else if (anchor->state == 3 && blade->state == 2) {
        blade->attacking = true;
    }
    /* x = px + sin(angle) * 80; y = py + sin(angle - 0x4000) * 80 —
     * a 90-degree offset reads as -cos(angle): a circular orbit. */
    blade->angle = anchor->angle;
    blade->x = anchor->x + aos_sine((uint16_t)anchor->angle) * ORBIT_RADIUS;
    blade->y = anchor->y +
               aos_sine((uint16_t)(anchor->angle - 0x4000)) *
                   ORBIT_RADIUS;
    return true;
}
