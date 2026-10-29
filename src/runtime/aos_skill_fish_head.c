/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_fish_head.h"

#include "aos_audio.h"

AosFishHeadSoul aos_fish_head_soul_create(int32_t soma_x, int32_t soma_y,
                                          bool facing_left) {
    int32_t vx = facing_left ? -0x20000 : 0x20000;
    return (AosFishHeadSoul){
        .x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (facing_left ? -8 : 8)) << 16),
        .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 26) << 16),
        .vx = vx,
        .ax = vx >> 5,
        .facing_left = facing_left,
    };
}

bool aos_fish_head_soul_update(AosFishHeadSoul *soul, int cam_x, int cam_y,
                               int margin_x, int margin_y) {
    if (!soul || soul->state == 2) return false;
    if (!soul->started) {
        soul->started = true;
        soul->state = 1;
        aos_audio_play_song(0xD8);
    }
    soul->vx += soul->ax;
    if (soul->ax >= 0 ? soul->vx > 0x80000 : soul->vx < -0x80000)
        soul->vx = soul->ax >= 0 ? 0x80000 : -0x80000;
    soul->x += soul->vx;
    int sx = (soul->x >> 16) - cam_x;
    int sy = (soul->y >> 16) - cam_y;
    if (soul->struck || sx < -margin_x || sx > margin_x + 0xF0 ||
        sy < -margin_y || sy > margin_y + 0xA0) {
        soul->state = 2;
        return false;
    }
    return true;
}
