/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_WEREWOLF_H
#define AOS_SOUL_RED_WEREWOLF_H
#include <stdbool.h>
#include <stdint.h>

#include "../../aos/core/aos_enemy.h"
#include "../../aos/core/aos_collision.h"

/* SkillWerewolfUse's werewolf; its walker state lives in an AosEnemy. */
typedef struct {
    AosEnemy body;      /* x, y, vx, vy, ay, mirrored (+ 0x58 bit 6) */
    uint8_t state;      /* + 0x0A: 1 walking */
} AosWerewolf;

/* State 0: placed by sub_08031EA4 at (0x14, -0xA) from the source's
 * facing; sub_0802393C must find a floor under it within its five 8-pixel
 * probes or the cast deletes itself. */
AosWerewolf aos_werewolf_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* sub_0802393C: floor_depth (sub_08001D94) probes at +8..+0x28 below the
 * entity's whole position; any nonzero depth means ground. */
bool aos_werewolf_ground_ok(const AosWerewolf *werewolf, const AosCollision *layer);
/* The second half of sub_0802393C: drop the entity onto the floor the
 * probes found, through the same walker snap the walk uses. */
void aos_werewolf_land(AosWerewolf *werewolf, const AosCollision *layer,
                        const AosProbes *probes);
/* One update: sub_0806CAF8 at -0x4000 with the 0x084F13B4 probes and
 * mode 4 walks it along its facing; the native margin check is absent —
 * it lives until its animation ends (handled by the caller). */
void aos_werewolf_update(AosWerewolf *werewolf, const AosCollision *layer,
                          const AosProbes *probes);
#endif
