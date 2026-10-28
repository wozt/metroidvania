/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_SPEAR_H
#define AOS_SKILL_SPEAR_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_collision.h"

/* SkillWingedSkeletonUse's spear. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    int32_t gravity;        /* + 0x54 */
    int16_t angle, step;    /* + 0x2C, + 0x2E */
    int32_t tip_x, tip_y;   /* + 0x24, + 0x28: the point 16 pixels along it */
    bool facing_left;       /* + 0x58 bit 6 */
    uint8_t state;          /* + 0x0A: 0 created, 1 flying, 2 stuck, 3 deleted next */
    uint8_t timer;          /* + 0x0D */
    bool blinking;          /* + 0x0F */
} AosSpear;

/* State 0 setup, placed by sub_08031EA4 at Soma's facing and (8, -0x1A):
 * 4 pixels a frame ahead and 1.125 up, gravity 0x2000, the angle at
 * -0x800 turning 0x100 a frame (all mirrored facing left). */
AosSpear aos_spear_create(int32_t soma_x, int32_t soma_y, bool facing_left);
/* One update: outside the screen margins (mx, my) of entry 4 it goes to
 * state 3; otherwise it turns while its angle is at most 0x3FFF, takes the
 * tip 16 pixels along the angle (turned by 0x8000 facing left), moves with
 * the gravity (limit 8 pixels a frame) and sticks (state 2, 32 frames) when
 * the cell at its moved whole position plus the tip is solid and not a
 * special cell (sub_08001A00, sub_08001F3C). Stuck, it blinks and counts
 * down to state 3. State 3 on entry deletes it (false). */
bool aos_spear_update(AosSpear *spear, const AosCollision *layer, int cam_x, int cam_y, int mx,
                      int my);
#endif
