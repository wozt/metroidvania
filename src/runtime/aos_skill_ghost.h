/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_GHOST_H
#define AOS_SKILL_GHOST_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_skill_devil.h"

enum { AOS_GHOST_SOULS = 2, AOS_GHOST_TIMER = 0x40, AOS_GHOST_SEQUENCE = 1 };

/* One of the two spirits created by SkillGhostUse (sub_0802A49C). */
typedef struct {
    int32_t x, y;       /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;     /* + 0x48, + 0x4C */
    int32_t speed;      /* + 0x24 */
    int16_t angle;      /* + 0x2C */
    uint8_t state;      /* + 0x0A: 0/1 appearing, 2 homing, 3 wandering, 4 leaving */
    uint8_t timer;      /* + 0x0D */
    bool struck;        /* + 0x18: its hit callback sub_0802D9B4 */
    int target;         /* homing-list slot, -1 when wandering */
} AosGhostSoul;

/* Create spirit `index` (0 or 1) around the caster. Its initial angle is
 * (random & 0xFFF) - 0x7800 + index * 0x6000 and its radius is 16 pixels. */
AosGhostSoul aos_ghost_soul_create(int index, int32_t x, int32_t y, uint32_t random);

/* At the end of sequence 1, select the nearest of the four homing-list
 * targets. With none, enter the native random-point flight. */
void aos_ghost_soul_launch(AosGhostSoul *ghost, const AosSkillTarget targets[4]);

/* Update homing, random-point or leaving flight. Targeted flight turns by
 * 0x300 per frame and lasts at most 64 frames. */
void aos_ghost_soul_update(AosGhostSoul *ghost, const AosSkillTarget *target,
                           uint32_t (*random)(void));

/* Whether the spirit is outside screen-margin entry 2 around the camera. */
bool aos_ghost_soul_outside(const AosGhostSoul *ghost, int cam_x, int cam_y, int mx, int my);

#endif
