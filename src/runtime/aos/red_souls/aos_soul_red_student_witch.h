/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_STUDENT_WITCH_H
#define AOS_SOUL_RED_STUDENT_WITCH_H
#include <stdbool.h>
#include <stdint.h>

#include "../../aos/core/aos_enemy.h"
#include "../../aos/core/aos_collision.h"

/* SkillStudentWitchUse's running ghost, shared by red soul 15 (dropped by
 * both the Student Witch and the Basilisk): its walker state lives in an
 * AosEnemy. */
typedef struct {
    AosEnemy body;      /* x, y, vx, vy, mirrored (+ 0x58 bit 6) */
    uint8_t state;      /* + 0x0A: 0 init, 1 walking, 2 lingering, 3 ending */
    uint8_t contacted;  /* + 0x0B: the first-contact song already played */
    uint8_t linger;     /* + 0x0D: counts 0x10 down in state 2 */
} AosStudentWitch;

/* State 0 places it through sub_08031EA4(entity, source, 8, -0x1A),
 * presets attack boxes and enters state 1. */
AosStudentWitch aos_student_witch_create(int32_t soma_x, int32_t soma_y,
                                         bool soma_facing_left);
/* One update. State 1 walks at -0x20000 with the 0x084F12BC probes
 * (mode 8): result&8 plays song 0x1A0 once, result&3 lingers 0x10
 * frames; the sub_0806D128(e, 4) margin probe forces state 3 and state 3
 * deletes the entity. `song` is raised on the first-contact request.
 * Returns false once deleted. */
bool aos_student_witch_update(AosStudentWitch *witch, const AosCollision *layer,
                              const AosProbes *probes, bool outside, bool *song);
#endif
