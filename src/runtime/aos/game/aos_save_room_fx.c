/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_save_room_fx.h"

static void fx_reset(AosSaveRoomFx *c) {
    c->sparkle_bursts = 0;
    c->parent_mark32 = false;
    c->parent_mark33 = false;
}

void aos_save_fx_orbit_a_update(AosSaveRoomFx *c,
                                uint8_t parent_phase6d,
                                uint8_t parent_rec30) {
    fx_reset(c);
    switch (c->state) {
    case 0:
        /* Parent +0x24 bit 1 arms the child. */
        if (c->phase24 & 2)
            c->state = 1;
        break;
    case 1:
        /* +0x6D picks animation 1 (phase 0) or 5 (phase 4). */
        c->anim5c = (parent_phase6d == 4) ? 5 : 1;
        c->state = 2;
        break;
    case 2:
        /* Phases 2/7 need the parent +0x30 record == 1. */
        if ((parent_phase6d == 2 || parent_phase6d == 7) &&
            parent_rec30 == 1) {
            c->flags59 |= 0x8;
            c->phase24 = 0x100000;
            c->parent_mark33 = true;
        }
        break;
    }
}

void aos_save_fx_orbit_b_update(AosSaveRoomFx *c,
                                int32_t cam_dx, int32_t cam_dy,
                                int32_t (*sin_cos)(int32_t angle)) {
    fx_reset(c);
    if (c->state == 0) {
        /* Angle (+0x20 << 0xD) through the sine table, scaled
         * by the +0x24 high word; +0x24 period grows by the
         * +0x28-derived step, +0x28/+0x2C integrate. */
        int32_t a_sin = sin_cos((c->aux20 & 0xFF) << 0xD);
        int32_t a_cos = sin_cos(((c->aux20 & 0xFF) << 0xD) +
                                (0x80 << 7));
        int32_t scale = c->phase24 >> 16;
        c->pos44 = a_sin * scale + 0x500000;      /* 0xA0<<15 */
        c->pos40 = a_cos * scale + 0x760000;       /* 0xEC<<15 */
        int32_t step = (c->phase28 > 0x1DFFFF)
            ? ((c->phase28 >> 16) * 3) << 0xD : 0;
        c->phase24 += step;
        c->phase28 += 0x40000;
        c->phase2c += 0x800;
        if (c->phase24 > 0xFFFFFF) {
            c->state = 1;
            c->timer_d = 2;
            c->anim5c = 9;
            c->parent_mark33 = true;
            c->phase24 = 0x100000;
            c->phase28 = 0;
        }
    } else if (c->state == 1) {
        /* Second orbit leg: angle + phase28, *0x26 scale,
         * positions biased by the camera deltas, period
         * burns 0x30000 a tick. */
        int32_t a_sin = sin_cos(((c->aux20 & 0xFF) << 0xD) +
                                c->phase28);
        int32_t a_cos = sin_cos(((c->aux20 & 0xFF) << 0xD) +
                                c->phase28 + (0x80 << 7));
        c->pos40 = a_cos * 0x26 + 0x780000;       /* 0xEC<<15 */
        c->pos44 = a_sin * 0x26 + 0x500000;       /* 0xA0<<15 */
        /* sub_0806CDB8/sub_0806CDFC camera offsets. */
        c->pos40 += cam_dx << 16;
        c->pos44 += cam_dy << 16;
        c->phase24 -= 0x30000;
        c->phase28 += 0x200;
        if (c->phase24 <= 0) {
            c->parent_mark32 = true;
            c->flags59 |= 0x8;
        }
    }
    /* Common tail: +0xD = (+0xD + 1) & 3; every fourth tick
     * scatters one sparkle at +/- 0x10. */
    c->timer_d = (uint8_t)((c->timer_d + 1) & 0xFF);
    if ((c->timer_d & 3) == 0) {
        c->sparkle_bursts++;
        /* The caller performs aos_save_fx_sparkle_spawn with
         * the child's +0x40/+0x44 and spread 0x10. */
    }
}

void aos_save_fx_particle_update(AosSaveRoomFx *c) {
    fx_reset(c);
    switch (c->state) {
    case 0:
        /* Sprite attach 0x082052E8/0x08257964 — approximated;
         * +0x4C = -0x10000 initial fall. */
        c->timer_d = 0xC;
        c->flags59 |= 0x2;
        c->vel4c = -0x10000;
        c->state = 1;
        break;
    case 1:
        c->pos40 += c->vel48;
        c->pos44 += c->vel4c;
        c->vel48 += c->acc50;
        c->vel4c += c->acc54;
        if ((uint8_t)(c->timer_d - 1) == 0) {
            c->flags59 |= 0x8;
            c->state = 2;
            c->timer_d = 0;
        } else {
            c->timer_d--;
        }
        break;
    case 2:
        c->pos44 -= c->vel4c;
        c->timer_d++;
        if (c->timer_d > 8)
            c->flags59 |= 0x8;
        break;
    }
}

void aos_save_fx_particle_oscillate(AosSaveRoomFx *c) {
    /* +0x40/+0x44 advance then retreat by the velocities
     * around the two animation calls. */
    c->pos40 += c->vel48;
    c->pos44 += c->vel4c;
    c->pos40 -= c->vel48;
    c->pos44 -= c->vel4c;
}

bool aos_save_fx_sparkle_spawn(AosSaveRoomFx *out,
                               int32_t x, int32_t y,
                               int32_t spread, uint32_t rng,
                               uint8_t palette_nib) {
    /* sub_080553A0: (rng & (spread-1)) - (spread >> 1) << 16
     * offsets on each axis (spread is a power of two minus
     * one mask: the ROM passes 0x10). */
    int32_t jx = (int32_t)((rng & (uint32_t)(spread - 1)) -
                           ((uint32_t)spread >> 1)) << 16;
    int32_t jy = (int32_t)(((rng >> 16) & (uint32_t)(spread - 1)) -
                           ((uint32_t)spread >> 1)) << 16;
    out->kind = 2;
    out->state = 0;
    out->pos40 = x + jx;
    out->pos44 = y + jy;
    /* sub_08055414: velocities are caller-supplied +0x50/+0x54
     * of the parent plus random +/- 0x100000. */
    out->vel48 = (int32_t)((rng & 0x1F) << 16) - 0x100000;
    out->vel4c = (int32_t)(((rng >> 8) & 0x1F) << 16) - 0x100000;
    out->flags58 = (uint8_t)((palette_nib & 0xF) << 3);
    out->link1c = 0;    /* parent token filled by caller */
    return true;
}
