/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow Soma motion rules; comments name the cvaos routine and
 * the code path each function reproduces. */
#include "aos_soma.h"

/* sub_0801B0D8 prologue. */
void aos_soma_integrate(AosSoma *soma) {
    soma->x += soma->vx + soma->extra_vx;
    if (soma->vy > AOS_FALL_CAP) soma->vy = AOS_FALL_CAP;
    soma->y += soma->vy;
    soma->extra_vx = 0;
}

/* sub_0801B0D8 states 0 and 1: left is tested first, then right; without a
 * direction the friction moves vx toward zero and stops on a sign change. */
void aos_soma_steer(AosSoma *soma, uint16_t held, int32_t speed) {
    if (held & AOS_KEY_LEFT) {
        soma->facing_left = true;
        soma->vx = -speed;
        soma->friction = 0;
    } else if (held & AOS_KEY_RIGHT) {
        soma->facing_left = false;
        soma->vx = speed;
        soma->friction = 0;
    } else {
        soma->friction = soma->vx >= 0 ? -AOS_FRICTION : AOS_FRICTION;
    }
    int32_t old = soma->vx;
    int32_t updated = old + soma->friction;
    soma->vx = updated;
    if ((old < 0 && updated > 0) || (old >= 0 && updated < 0) ||
        (old == 0 && updated > 0)) {
        soma->vx = 0;
        soma->friction = 0;
    }
}

/* sub_08019180, normal jump branch: grounded, or airborne for at most three
 * frames. The flag 0x800000 branch and the extra jumps are not ported. */
bool aos_soma_jump(AosSoma *soma, uint16_t pressed) {
    if (!(pressed & AOS_KEY_JUMP)) return false;
    if ((soma->flags & AOS_FLAG_AIRBORNE) && soma->air_frames > 3) return false;
    soma->flags = (soma->flags | AOS_FLAG_AIRBORNE) & 0xFFEFFBFFu;
    soma->air_frames = 16;
    soma->gravity_mod = 0;
    soma->vy = (soma->flags & AOS_FLAG_HEAVY) ? AOS_SLOWED_JUMP_VELOCITY
                                              : AOS_JUMP_VELOCITY;
    return true;
}

/* sub_08018020 entry. */
void aos_soma_leave_ground(AosSoma *soma) {
    if (soma->flags & 0x00100002u) return;
    soma->flags |= AOS_FLAG_AIRBORNE;
    soma->vy = 0;
    soma->gravity_mod = (int32_t)0xFFFFF000;   /* -0.0625 */
}

/* sub_08018B98 (a label inside sub_08018020), without the ceiling probe. */
void aos_soma_gravity(AosSoma *soma) {
    if (!(soma->flags & 0x1Eu)) return;
    soma->flags &= 0xEFEFFBFFu;
    if (soma->flags & AOS_FLAG_HEAVY) {
        soma->vy += 0x6000;                    /* 0.375 */
        soma->gravity_mod = 0;
    }
    if (soma->air_frames <= 15) soma->air_frames++;
    if (soma->vy <= 0x1FFF) soma->vy += 0x2000; /* 0.125 */
    soma->vy += 0x1A00;                         /* 0.1015625 */
    if (soma->vy > 0) {
        soma->vy += soma->gravity_mod;
        soma->gravity_mod += 0x400;             /* 0.015625 */
        if (soma->gravity_mod > 0x1000) soma->gravity_mod = 0x1000;
    }
}

/* sub_0801938C: jump release, apex float and slowed falls, then the start
 * of sub_08018020 (ledge start and gravity). */
void aos_soma_air(AosSoma *soma, uint16_t held, bool slow_fall) {
    if (soma->flags & 0x1Eu) {
        if (!(soma->flags & 0x18u)) {
            if (soma->vy < (int32_t)0xFFFFC000 && !(held & AOS_KEY_JUMP)) {
                soma->vy = (int32_t)0xFFFFC000;            /* -0.25 */
                soma->gravity_mod = (int32_t)0xFFFFE000;   /* -0.125 */
            }
            if ((uint32_t)(soma->vy + 0x1BFFF) <= 0x1DFFEu && (held & AOS_KEY_JUMP)) {
                soma->gravity_mod += (int32_t)0xFFFFF800;  /* -0.03125 */
                if (soma->gravity_mod < (int32_t)0xFFFFE000)
                    soma->gravity_mod = (int32_t)0xFFFFE000;
            }
        }
        if ((slow_fall || (soma->flags & AOS_FLAG_SLOW_FALL)) && soma->vy > 0x2800)
            soma->vy += (int32_t)0xFFFFD800;               /* -0.15625 */
    }
    aos_soma_leave_ground(soma);
    if ((soma->flags & 0x00800010u) != 0x00800000u) aos_soma_gravity(soma);
}
