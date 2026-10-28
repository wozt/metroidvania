/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_needles.h"

#include "aos_audio.h"
#include "aos_enemy.h"

AosNeedles aos_needles_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    return (AosNeedles){
        .x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (soma_facing_left ? -0x14 : 0x14))
                       << 16),
        .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16),
        .facing_left = soma_facing_left, .state = 1, .timer = AOS_NEEDLES_FRAMES};
}

bool aos_needles_update(AosNeedles *needles, uint32_t (*random)(void), int cam_x, int cam_y,
                        int mx, int my, bool *burst) {
    *burst = false;
    if (needles->state == 3) return false;
    if (needles->state == 1) {
        if (--needles->timer == 0) needles->struck = true;
        if (needles->struck) {
            needles->vy = (int32_t)0xFFFF0000 - (int32_t)(random() & 0x7FFF);
            needles->gravity = 0x2000;
            *burst = true;
            int32_t step = 0;
            for (int k = 0; k < AOS_NEEDLES_PIECES; ++k) {
                needles->radius[k] = 0;
                needles->angle[k] = (uint16_t)(0xF000 + step + (int32_t)(random() & 0x3FF));
                step += 0x3333;
            }
            aos_audio_play_song(0xD7);
            needles->timer = AOS_NEEDLES_BURST_FRAMES;
            needles->state = 2;
        }
    } else if (needles->state == 2) {
        needles->vy += needles->gravity;
        if (needles->vy > 0x80000) needles->vy = 0x80000;
        needles->y += needles->vy;
        for (int k = 0; k < AOS_NEEDLES_PIECES; ++k)
            needles->radius[k] = (int16_t)(needles->radius[k] + (needles->timer << 5));
        if (needles->timer == 0) return false;
        needles->timer--;
    }
    int sx = (needles->x >> 16) - cam_x, sy = (needles->y >> 16) - cam_y;
    if (sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0) needles->state = 3;
    return true;
}

bool aos_needles_piece(const AosNeedles *needles, int k, int32_t *x, int32_t *y) {
    if ((needles->timer & 1) == (k & 1)) return false;
    int32_t r = needles->radius[k];
    r = (r < 0 ? r + 0xFF : r) >> 8;
    *x = needles->x + aos_sine((uint32_t)needles->angle[k] + 0x4000u) * r;
    *y = needles->y + aos_sine(needles->angle[k]) * r;
    return true;
}
