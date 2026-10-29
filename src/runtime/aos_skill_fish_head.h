/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_FISH_HEAD_H
#define AOS_SKILL_FISH_HEAD_H

#include <stdbool.h>
#include <stdint.h>

/* SkillFishHeadUse, 0x0802B350; red soul 29. */
typedef struct {
    int32_t x, y;       /* +0x40, +0x44, 16.16 room pixels */
    int32_t vx, ax;     /* +0x48, +0x50 */
    bool facing_left;   /* +0x58 bit 6 */
    bool struck;        /* +0x18, set by sub_0802D9B4 */
    bool started;
    uint8_t state;      /* +0x0A: 1 flying, 2 removed */
} AosFishHeadSoul;

/* State 0 places sequence 0 at Soma's facing and (8, -26), with an initial
 * horizontal speed of 2.0 and acceleration of 1/16 pixel per frame. */
AosFishHeadSoul aos_fish_head_soul_create(int32_t soma_x, int32_t soma_y,
                                          bool facing_left);

/* sub_0806D3D8 accelerates toward 8.0 pixels per frame and then moves. The
 * soul is removed in the update where it is struck or leaves margin entry 2. */
bool aos_fish_head_soul_update(AosFishHeadSoul *soul, int cam_x, int cam_y,
                               int margin_x, int margin_y);

#endif
