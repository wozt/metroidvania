/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_FLAME_DEMON_H
#define AOS_SOUL_RED_FLAME_DEMON_H
#include <stdbool.h>
#include <stdint.h>

#include "../../aos/math/aos_math.h"

/* The fire spirits of the Demon Lord (red 43) and Flame Demon (red 44)
 * souls: sub_080292B4 volley bolts and SkillFlameDemonUpdate's rotating
 * wisp share the 16.16 velocity fields + 0x48 / + 0x4C (the + 0x50 field
 * is modelled as the wisp's vy). */
typedef struct {
    int32_t x, y;           /* room pixels, 16.16 */
    int32_t vx, vy;
    uint8_t state;          /* + 0x0A */
    uint8_t timer;          /* + 0x0D: the wisp's fire-puff beat */
    uint16_t spin;          /* + 0x2C rotation accumulator */
    int16_t spin_rate;      /* + 0x2E: 0x234, negated when mirrored */
    bool mirrored;          /* + 0x58 bit 6 */
    bool lord;              /* SkillFlameDemonUpdate vs sub_080292B4 */
} AosFlameSpirit;

/* sub_080292B4 volley bolt: placed on the caster, velocity from the
 * parent's sine fan (vx mirrored with the facing). */
AosFlameSpirit aos_flame_bolt_create(int32_t x, int32_t y, uint32_t angle,
                                     bool mirrored);
/* SkillFlameDemonUpdate wisp: vx +/-0x18000 by facing, the + 0x50 field
 * as vy +/-0x400 (documented approximation), spin rate 0x234. */
AosFlameSpirit aos_flame_spirit_create(int32_t x, int32_t y, bool mirrored);

/* One update: integrates the velocity (sub_0806D3D8 caps it at
 * 0x80000), the wisp accumulates + 0x2C and beats a fire puff every
 * eighth frame (signalled through `puff`). Returns false once the
 * entity is deleted: animation end or the margin probe
 * (sub_0806D128 with n = 2 for bolts, 3 for the wisp). */
bool aos_flame_spirit_update(AosFlameSpirit *spirit, bool anim_finished,
                             bool outside, bool *puff);
#endif
