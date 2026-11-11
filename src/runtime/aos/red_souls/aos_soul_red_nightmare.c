/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_nightmare.h"

#define CHARGE_CAP 0xFFFF
#define CHARGE_STEP 0x1000
#define DRIFT_CAP 0x80000

AosNightmare aos_nightmare_create(int32_t soma_x, int32_t soma_y,
                                  bool facing_left) {
    AosNightmare n;
    n.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) +
                                        (facing_left ? -8 : 8)) << 16);
    n.y = soma_y;
    n.vx = facing_left ? -0x4000 : 0x4000;
    n.ax = facing_left ? -0x2000 : 0x2000;
    n.vy = 0;
    n.ay = 0;
    n.charge = 0;
    n.facing_left = !facing_left; /* + 0x58 bit 6 inverted at init */
    n.state = 1;
    n.marked = false;
    n.outside = false;
    n.trail = false;
    return n;
}

static int32_t clamp_drift(int32_t v, int32_t a) {
    v += a;
    if (a >= 0) {
        if (v > DRIFT_CAP) v = DRIFT_CAP;
    } else if (v < -DRIFT_CAP) {
        v = -DRIFT_CAP;
    }
    return v;
}

bool aos_nightmare_update(AosNightmare *n, uint32_t (*rng)(void)) {
    n->trail = false;
    if (n->state == 1) {
        /* The jitter: x by +-4 px, y upward by 12-20 px per frame,
         * then the sub_08045CEC(x, y, 0, 7) trail entity. */
        n->x += (int32_t)(rng() & 0x7FFFF) - 0x40000;
        n->y += (int32_t)(rng() & 0x7FFFF) - 0x140000;
        n->trail = true;
        if (n->charge > CHARGE_CAP) {
            /* The state-2 init: palette row restored, + 0x59 bit 4
             * cleared, the sub_0802383C(7)/(3) partner touches — all
             * visual or bookkeeping, not ported. */
            n->state = 2;
        } else {
            n->charge += CHARGE_STEP;
        }
    }
    /* sub_0806D3D8(e, 0x80000) in both states: velocity += accel
     * clamped to +-0x80000, then position += velocity. */
    n->vx = clamp_drift(n->vx, n->ax);
    n->vy = clamp_drift(n->vy, n->ay);
    n->x += n->vx;
    n->y += n->vy;
    /* The shared tail: the + 0x64/+ 0x65 frame attack box and the
     * sub_0806D128(e, 3) / + 0x59 bit-4 delete are the caller's. */
    return !(n->outside || n->marked);
}
