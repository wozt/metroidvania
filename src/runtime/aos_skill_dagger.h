/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_DAGGER_H
#define AOS_SKILL_DAGGER_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_collision.h"

enum {
    AOS_HARPY_DAGGERS = 2,
    AOS_DAGGER_FRAME = 1,           /* still frame of resource 24 */
    AOS_DAGGER_TRAIL_IMAGES = 3,    /* sub_0802DC60: frames 3-5 */
    AOS_DAGGER_TRAIL_FIRST_FRAME = 3,
    AOS_DAGGER_HISTORY = 10,
};

/* One dagger of SkillHarpyUse (sub_0802CAC0). */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx;             /* + 0x48 */
    bool facing_left;       /* + 0x58 bit 6 */
    uint8_t state;          /* + 0x0A: 0 created, 1 flying, 2-3 stuck, 4 deleted next */
    uint8_t timer;          /* + 0x0D */
    bool blinking;          /* + 0x0F */
    bool struck;            /* + 0x18: sub_0802D9B4, its hit callback */
} AosDagger;

/* Its trail entity (sub_0802C9E8, updated by sub_0802C944 after it). */
typedef struct {
    int16_t history[AOS_DAGGER_HISTORY][2];  /* + 0x10: room (x, y) */
} AosDaggerTrail;

/* SkillHarpyUse's dagger `index` at the thrower's position: index 0 faces
 * right and index 1 left (+ 0x58 bit 6 = index & 1), 5 pixels a frame. */
AosDagger aos_dagger_create(int index, int32_t x, int32_t y);
/* One update: it moves; struck, it goes to state 4; against a wall 6
 * pixels ahead (sub_08002058 facing right, sub_0800207C facing left) to
 * state 2, which blinks 16 frames and goes to state 4; outside the screen
 * margins (mx, my) of entry 4, to state 4. State 4 on entry deletes it
 * (false). */
bool aos_dagger_update(AosDagger *dagger, const AosCollision *layer, int cam_x, int cam_y,
                       int mx, int my);
/* sub_0802C9E8's history: entries 1-9 at the dagger's whole position
 * (entry 0 is written by the first update). */
AosDaggerTrail aos_dagger_trail_create(const AosDagger *dagger);
/* sub_0802C944: the history shifts and entry 0 takes the dagger's whole
 * position; sub_0802DC60 draws frames 3, 4, 5 at entries 1, 2, 3. */
void aos_dagger_trail_update(AosDaggerTrail *trail, const AosDagger *dagger);
#endif
