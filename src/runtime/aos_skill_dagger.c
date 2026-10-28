/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_dagger.h"

AosDagger aos_dagger_create(int index, int32_t x, int32_t y) {
    AosDagger dagger = {.x = x, .y = y, .facing_left = (index & 1) != 0};
    dagger.vx = dagger.facing_left ? -0x50000 : 0x50000;
    return dagger;
}

bool aos_dagger_update(AosDagger *dagger, const AosCollision *layer, int cam_x, int cam_y,
                       int mx, int my) {
    switch (dagger->state) {
    case 0:
        dagger->state = 1;
        /* fall through */
    case 1: {
        dagger->x += dagger->vx;
        if (dagger->struck) {
            dagger->state = 4;
            break;
        }
        int32_t x = dagger->x >> 16, y = dagger->y >> 16;
        int wall = dagger->facing_left ? aos_wall_push_right(layer, x - 6, y)
                                       : aos_wall_push_left(layer, x + 6, y);
        if ((int16_t)wall) dagger->state = 2;
        break;
    }
    case 2:
        dagger->timer = 0x10;
        dagger->blinking = true;
        dagger->state = 3;
        /* fall through */
    case 3:
        if (dagger->timer) dagger->timer--;
        else dagger->state = 4;
        break;
    case 4: return false;
    default: break;
    }
    int sx = (dagger->x >> 16) - cam_x, sy = (dagger->y >> 16) - cam_y;
    if (sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0) dagger->state = 4;
    return true;
}

AosDaggerTrail aos_dagger_trail_create(const AosDagger *dagger) {
    AosDaggerTrail trail = {0};
    for (int i = 1; i < AOS_DAGGER_HISTORY; ++i) {
        trail.history[i][0] = (int16_t)(dagger->x >> 16);
        trail.history[i][1] = (int16_t)(dagger->y >> 16);
    }
    return trail;
}

void aos_dagger_trail_update(AosDaggerTrail *trail, const AosDagger *dagger) {
    for (int i = AOS_DAGGER_HISTORY - 1; i > 0; --i) {
        trail->history[i][0] = trail->history[i - 1][0];
        trail->history[i][1] = trail->history[i - 1][1];
    }
    trail->history[0][0] = (int16_t)(dagger->x >> 16);
    trail->history[0][1] = (int16_t)(dagger->y >> 16);
}
