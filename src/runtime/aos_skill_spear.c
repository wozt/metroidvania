/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_spear.h"

#include "aos_enemy.h"

enum { AOS_SPEAR_LIMIT = 0x80000 };

AosSpear aos_spear_create(int32_t soma_x, int32_t soma_y, bool facing_left) {
    /* sub_08031EA4 writes only the integer halves of a fresh entity. */
    AosSpear spear = {.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (facing_left ? -8 : 8))
                                     << 16),
                      .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16),
                      .vx = 0x40000, .vy = -0x12000, .gravity = 0x2000,
                      .angle = (int16_t)0xF800, .step = 0x100, .facing_left = facing_left};
    if (facing_left) {
        spear.angle = (int16_t)-spear.angle;
        spear.step = (int16_t)-spear.step;
        spear.vx = -spear.vx;
    }
    return spear;
}

/* The tip's whole pixels, rounded toward zero. */
static int32_t whole(int32_t value) {
    return value < 0 ? -((-value) >> 16) : value >> 16;
}

bool aos_spear_update(AosSpear *spear, const AosCollision *layer, int cam_x, int cam_y, int mx,
                      int my) {
    switch (spear->state) {
    case 0:
        spear->state = 1;
        /* fall through */
    case 1: {
        /* + 0x59 bit 2 was set by sub_08019478, so only the margins count. */
        int sx = (spear->x >> 16) - cam_x, sy = (spear->y >> 16) - cam_y;
        if (sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0) {
            spear->state = 3;
            break;
        }
        if (spear->step != 0 ? spear->angle <= 0x3FFF : spear->angle <= 0x4000)
            spear->angle = (int16_t)(spear->angle + spear->step);
        int32_t angle = spear->angle + (spear->facing_left ? -0x8000 : 0);
        spear->tip_x = aos_sine((uint32_t)(angle + 0x4000)) * 16;
        spear->tip_y = aos_sine((uint32_t)angle) * 16;
        /* sub_0806D430 (no acceleration) and sub_0806D460. */
        spear->x += spear->vx;
        spear->vy += spear->gravity;
        if (spear->vy > AOS_SPEAR_LIMIT) spear->vy = AOS_SPEAR_LIMIT;
        spear->y += spear->vy;
        int32_t px = (int16_t)((spear->x >> 16) + whole(spear->tip_x));
        int32_t py = (int16_t)((spear->y >> 16) + whole(spear->tip_y));
        if (aos_collision_cell(layer, px, py) && !aos_collision_special(layer, px, py)) {
            spear->state = 2;
            spear->timer = 0x20;
        }
        break;
    }
    case 2:
        spear->blinking = true;
        if (spear->timer) spear->timer--;
        else spear->state = 3;
        break;
    case 3: return false;
    default: break;
    }
    return true;
}
