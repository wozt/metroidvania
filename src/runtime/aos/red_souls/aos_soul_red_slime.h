/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_SLIME_H
#define AOS_SOUL_RED_SLIME_H
#include <stdbool.h>
#include <stdint.h>

#include "../../aos/core/aos_enemy.h"
#include "../../aos/core/aos_collision.h"

/* SkillSlimeUse's orbiting blob plus the sub_0802C9E8 trail child: the
 * parent's walker state lives in an AosEnemy (+ 0x40/+ 0x44 position,
 * + 0x48/+ 0x4C velocity). */
typedef struct {
    AosEnemy body;
    uint16_t angle;     /* + 0x2C: (rng & 0x3FF) + 0x1000, mirrored variant */
    int32_t speed;      /* + 0x34: 0x300000 */
    uint16_t life;      /* + 0x24: counts 0x100 down */
    uint8_t state;      /* + 0x0A: 0 init, 1 active, 2 done */
    uint8_t swapped;    /* + 0x0B: the 0x20-remaining anim variant played */
} AosSlime;

/* The sub_0802C9E8 trail child: a nine-entry ring of the parent's room
 * positions (+ 0x34..+ 0x46), rendered at the oldest entry; sub_0802CD6C
 * deletes it when the parent's slot empties. */
typedef struct {
    int16_t hx[9], hy[9];    /* the + 0x34 ring, oldest first */
    uint8_t head;            /* next write slot */
    bool filled;
} AosSlimeTrail;

/* State 0 places it through sub_08031EA4(entity, source, 0xC, -0x1A),
 * draws (rng & 0x3FF) + 0x1000 into + 0x2C (mirrored: -0x8000 - angle),
 * presets + 0x34 = 0x300000, attack box 5 plus the 0x0808FCFC box and
 * enters state 1 with + 0x24 = 0x100. */
AosSlime aos_slime_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                          uint32_t random_draw);
/* One update: + 0x48/+ 0x4C = (sub_080009E4(angle + 0x4000/angle) >> 8)
 * * (+ 0x34 >> 8), sub_0802CE20-style integrate with a floor/ceiling
 * snap (approximated), + 0x24 counts down — at 0x20 remaining the anim
 * variant swaps once (+ 0x0B), at 0 the entity ends. `variant` is raised
 * on the swap. Returns false once deleted. */
bool aos_slime_update(AosSlime *slime, const AosCollision *layer, bool *variant);

/* The trail child: appends the parent's room position each update and
 * exposes the oldest entry through trail_pos. Returns false when the
 * parent is gone (sub_0802CD6C's slot check). */
void aos_slime_trail_push(AosSlimeTrail *trail, int32_t x, int32_t y);
void aos_slime_trail_pos(const AosSlimeTrail *trail, int32_t *x, int32_t *y);
bool aos_slime_trail_update(AosSlimeTrail *trail, bool parent_alive);
#endif
