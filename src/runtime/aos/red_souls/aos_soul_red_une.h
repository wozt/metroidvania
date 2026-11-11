/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_UNE_H
#define AOS_SOUL_RED_UNE_H
#include <stdbool.h>
#include <stdint.h>

#include "aos/core/aos_enemy.h"
#include "aos/core/aos_collision.h"

/* SkillUneUse's bouncing walker: sub_0806CAF8 walks it at -+ 0x24 with
 * the 0x084F13AA probes; a wall hit (result & 3) negates and halves the
 * speed, a floor contact (result & 8) plays song 0xE0 and moves to the
 * 0x80-frame linger, which then plays song 0xE8. The native projectile
 * table lookups (gEwramData + 0x1017C through + 0x64/+ 0x65 and
 * sub_08042848) and the + 0x6C bit-4 hit flag are not ported. */
typedef struct {
    AosEnemy body;      /* walker entity (x, y, vx, vy, mirrored, ground) */
    uint8_t state;      /* + 0x0A: 0 init, 1 walk, 2 linger, 3 fire, 4 delete */
    uint8_t timer;      /* + 0x0D: 0x80 linger in state 2 */
    int32_t speed;      /* + 0x24: 0xA0000, negated and halved on walls */
    int32_t vy;         /* + 0x4C: -0x18000 hop velocity, + 0x54 gravity */
    bool shot1;         /* the state-1->2 contact shot request (not ported) */
    bool shot2;         /* the state-3 shot request (not ported) */
} AosUne;

/* State 0 places it through sub_08031EA4(entity, source, 8, -0x1A) and
 * enters state 1 with + 0x24 = 0xA0000, + 0x4C = -0x18000 (hop
 * velocity), + 0x54 = 0x2000 gravity, + 0x59 |= 2 and + 0x72 |= 6. */
AosUne aos_une_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* One walker step (mode 0) plus the state machine; `hit` and `outside`
 * stand for the + 0x6C bit-4 flag and the sub_0806D128(e, 4) margin
 * probe. Returns false once deleted. */
bool aos_une_update(AosUne *une, const AosCollision *layer,
                    const AosProbes *probes, bool hit, bool outside);
#endif
