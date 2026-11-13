/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_flyer.h"

static void ballistic(AosFlyer *f) {
    f->x += f->vx;
    f->y += f->vy;
    f->vx += f->ax;
    f->vy += f->ay;
}

void aos_flyer_step_65858(AosFlyer *f) {
    ballistic(f);
    if (!f->probe_inside) f->flags59 |= 8;
}

void aos_flyer_step_658ac(AosFlyer *f, bool *pal_restored) {
    ballistic(f);
    f->spin += f->spin_rate;
    /* sub_0803E058(spin, affine, affine, 0, {0, pal_nibble}) — a
     * draw-side request, not tracked here. */
    if (!f->probe_inside) {
        if (pal_restored) *pal_restored = true; /* sub_0803E6BC(nibble) */
        f->flags59 |= 8;
    }
}

void aos_flyer_step_6592c(AosFlyer *f) {
    ballistic(f);
    f->spin += f->spin_rate;
    if (!f->probe_inside) f->flags59 |= 8;
}
