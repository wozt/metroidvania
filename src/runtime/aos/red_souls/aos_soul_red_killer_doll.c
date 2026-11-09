/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_killer_doll.h"

AosDecoy aos_decoy_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    return (AosDecoy){
        .x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (soma_facing_left ? -8 : 8)) << 16),
        .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x10) << 16),
        .vx = soma_facing_left ? -0x18000 : 0x18000, .vy = (int32_t)0xFFFD8000, .state = 1,
        .timer = AOS_DECOY_FRAMES};
}

bool aos_decoy_update(AosDecoy *decoy, const AosCollision *layer) {
    if (decoy->state == 1) {
        int32_t brake = 0x100;
        decoy->x += decoy->vx;
        decoy->y += decoy->vy;
        int32_t x = decoy->x >> 16, y = decoy->y >> 16;
        decoy->vy += 0x2000;
        if (decoy->vy > 0x80000) decoy->vy = 0x80000;
        if (decoy->vy >= 0) {
            int depth = aos_floor_depth(layer, x, y + 1, 0, false);
            if (depth) {
                decoy->y = (int32_t)((uint32_t)(uint16_t)((decoy->y >> 16) + 2 + depth) << 16) |
                           (decoy->y & 0xFFFF);
                decoy->vy = 0;
                brake = 0x4000;
            }
        } else {
            int depth = aos_ceiling_depth(layer, x, y - 0x20, 0, false);
            if (depth) {
                decoy->y += (int32_t)((uint32_t)depth << 16);
                decoy->vy = 0;
            }
        }
        int wall = 0;
        if (decoy->vx > 0) {
            wall = aos_wall_push_left(layer, x + 8, y - 0x10);
            decoy->vx -= brake;
            if (decoy->vx <= 0) decoy->vx = 0;
        } else if (decoy->vx < 0) {
            wall = aos_wall_push_right(layer, x - 8, y - 0x10);
            decoy->vx += brake;
            if (decoy->vx >= 0) decoy->vx = 0;
        }
        if (wall) {
            decoy->x += (int32_t)((uint32_t)wall << 16);
            decoy->vx = -(decoy->vx >> 1);
        }
        if (decoy->timer == 0) {
            decoy->blinking = true;
            decoy->timer = AOS_DECOY_BLINK;
            decoy->state = 2;
        }
    }
    if (decoy->state == 2 && decoy->timer == 0) return false;
    decoy->timer--;
    return true;
}
