/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_shot.h"

AosShot aos_shot_create(int32_t soma_x, int32_t soma_y, bool facing_left) {
    return (AosShot){
        .x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (facing_left ? -8 : 8)) << 16),
        .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16),
        .vx = facing_left ? -0x40000 : 0x40000, .facing_left = facing_left};
}

bool aos_shot_update(AosShot *shot, int cam_x, int cam_y, int mx, int my) {
    shot->state = 1;
    shot->x += shot->vx;
    int sx = (shot->x >> 16) - cam_x, sy = (shot->y >> 16) - cam_y;
    return !(sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0);
}
