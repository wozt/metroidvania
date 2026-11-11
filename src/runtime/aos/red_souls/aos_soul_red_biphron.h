/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_BIPHRON_H
#define AOS_SOUL_RED_BIPHRON_H
#include <stdbool.h>
#include <stdint.h>

#include "../../aos/core/aos_enemy.h"
#include "../../aos/core/aos_collision.h"

/* SkillBiphronUse's accelerating walker; its motion state lives in an
 * AosEnemy like the Werewolf's. */
typedef struct {
    AosEnemy body;      /* x, y, vx, vy, mirrored (+ 0x58 bit 6) */
    int32_t speed;      /* + 0x24: starts 0x8000, + 0x1000/frame, cap 0x1FFFF */
    uint8_t timer;      /* + 0x0D: totem beat, counts down from 0x40 */
    bool spawned;       /* the 0x40-tick lifetime ran */
} AosBiphron;

/* State 0 places the entity at (0x14, -0xA) from the source's facing and
 * sub_0802393C must find its floor or the cast deletes itself; on success
 * + 0x24 = 0x8000, song 0x1A4 plays and + 0x0D = 0x40. */
AosBiphron aos_biphron_create(int32_t soma_x, int32_t soma_y,
                              bool soma_facing_left);
bool aos_biphron_ground_ok(const AosBiphron *biphron, const AosCollision *layer);
void aos_biphron_land(AosBiphron *biphron, const AosCollision *layer,
                      const AosProbes *probes);

/* One update: + 0x24 ramps toward 0x1FFFF, sub_0806CAF8 walks it at
 * -+ 0x24 with the 0x084F13B4 probes (mode 4), a wall result&3 flips the
 * facing bit, every eighth + 0x0D tick asks for a sub_080286BC totem
 * (signalled through `totem`), and the 0x40-tick countdown ends it.
 * Returns false once deleted. */
bool aos_biphron_update(AosBiphron *biphron, const AosCollision *layer,
                        const AosProbes *probes, bool *totem);
#endif
