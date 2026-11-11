/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_NIGHTMARE_H
#define AOS_SOUL_RED_NIGHTMARE_H
#include <stdbool.h>
#include <stdint.h>

/* SkillNightmareUse's rising wisp. State 1 jitters the position —
 * x by (rng & 0x7FFFF) - 0x40000, y by (rng & 0x7FFFF) - 0x140000 —
 * and spawns a sub_08045CEC(x, y, 0, 7) trail each frame while + 0x24
 * ramps by 0x1000 to 0xFFFF; past the cap the state-2 init restores
 * the palette row, clears + 0x59 bit 4 and touches two partner
 * entities through sub_0802383C(7)/(3). Both states integrate the
 * + 0x48/+ 0x50 and + 0x4C/+ 0x54 drifts under sub_0806D3D8's
 * 0x80000 cap, animate, shape the + 0x64/+ 0x65 frame attack box and
 * delete on the sub_0806D128(e, 3) margin probe or the + 0x59
 * bit-4 mark. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (16.16) */
    int32_t vx, ax;         /* + 0x48, + 0x50: facing-signed drift */
    int32_t vy, ay;         /* + 0x4C, + 0x54: stay 0 in the native */
    int32_t charge;         /* + 0x24: 0 -> 0x10000 ramp */
    bool facing_left;       /* + 0x58 bit 6, inverted at init */
    uint8_t state;          /* + 0x0A: 1 jitter, 2 drift */
    bool marked;            /* + 0x59 bit 4: injected delete mark */
    bool outside;           /* sub_0806D128(e, 3): injected */
    bool trail;             /* this update spawned the trail effect */
} AosNightmare;

/* The entity as the state-0 init leaves it: sub_08031EA4(source, 8,
 * 0), vx -+0x4000 with ax -+0x2000 signed by the source facing (the
 * + 0x58 bit 6 is then inverted), charge 0, state 1 on the same
 * frame; songs 0x19A and 0xE1 are requested by the caller. The
 * sub_0803E654 variant pick's negative half (cast fizzle) is not
 * ported — the cast always succeeds here (documented divergence). */
AosNightmare aos_nightmare_create(int32_t soma_x, int32_t soma_y,
                                  bool facing_left);
/* One update; false once the entity is deleted (the margin probe or
 * the + 0x59 bit-4 mark fired). `rng` supplies the two jitter draws. */
bool aos_nightmare_update(AosNightmare *nightmare, uint32_t (*rng)(void));
#endif
