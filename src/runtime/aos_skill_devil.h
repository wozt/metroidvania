/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_DEVIL_H
#define AOS_SKILL_DEVIL_H
#include <stdbool.h>
#include <stdint.h>

enum { AOS_DEVIL_FIRES = 3, AOS_DEVIL_TIMER = 0x40 };

/* A target of the homing list gEwramData + 0x13194, as the fire reads it. */
typedef struct {
    bool present;
    int32_t x, y;   /* room pixels, 16.16 */
    int16_t hp;     /* + 0x34 */
} AosSkillTarget;

/* One fire of SkillTinyDevilUse (SkillTinyDevilUpdate). */
typedef struct {
    int32_t x, y;       /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;     /* + 0x48, + 0x4C */
    int32_t speed;      /* + 0x24 */
    int16_t angle;      /* + 0x2C */
    uint8_t state;      /* + 0x0A: 0 created, 1 animating, 2 homing, 3 straight */
    uint8_t timer;      /* + 0x0D */
    bool struck;        /* + 0x18: its hit callback sub_0802D9B4 */
    int target;         /* the homing list slot it chases, -1 for none */
} AosDevilFire;

/* SkillTinyDevilUse's fire `index` (0-2) at the caster's position: angle
 * index * 0x1800 - 0x1800, mirrored (-0x8000 - angle) facing left. */
AosDevilFire aos_devil_fire_create(int index, int32_t x, int32_t y, bool facing_left);
/* The end of its animation (state 1): the slot chased is the first filled
 * one of the 4 from (random & 3) on; with one it homes (state 2), without
 * it flies straight (state 3); the 64-frame timer starts. `slots` tells
 * which homing list entries are filled. */
void aos_devil_fire_launch(AosDevilFire *fire, uint32_t random, const bool slots[4]);
/* One update of states 2 and 3: the speed gains 0x4000 while at most
 * 0x4FFFF; homing turns 0x100 a frame toward the target (ArcTan2 of the
 * whole-pixel offset), the velocity follows the angle, it moves, and the
 * timer runs out, the target's death or its own hit end the homing. */
void aos_devil_fire_update(AosDevilFire *fire, const AosSkillTarget *target);
/* Whether the fire's whole position is outside the screen margins (mx,
 * my) of entry 2 around the camera (deleted then). */
bool aos_devil_fire_outside(const AosDevilFire *fire, int cam_x, int cam_y, int mx, int my);
/* The still frame drawn after the animation: (angle + 0x4000) >> 12 & 15
 * (rounded toward zero for negative sums). */
uint8_t aos_devil_fire_frame(const AosDevilFire *fire);
#endif
