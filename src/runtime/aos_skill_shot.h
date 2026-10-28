/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_SHOT_H
#define AOS_SKILL_SHOT_H
#include <stdbool.h>
#include <stdint.h>

/* SkillDurgaUse's straight shot. */
typedef struct {
    int32_t x, y;       /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx;         /* + 0x48 */
    bool facing_left;   /* + 0x58 bit 6 */
    uint8_t state;      /* + 0x0A: 0 created, 1 flying */
} AosShot;

/* State 0 of SkillDurgaUse: placed by sub_08031EA4 at Soma's facing and
 * (8, -0x1A) (whole pixels), 4 pixels a frame toward the facing. */
AosShot aos_shot_create(int32_t soma_x, int32_t soma_y, bool facing_left);
/* One update: states 0 and 1 move it (sub_0806D3D8 with no acceleration
 * or gravity); outside the screen margins (mx, my) of entry 3 it is
 * deleted in the same update (false). */
bool aos_shot_update(AosShot *shot, int cam_x, int cam_y, int mx, int my);
#endif
