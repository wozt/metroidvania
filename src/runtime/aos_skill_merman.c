/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_merman.h"

enum { AOS_MERMAN_LIMIT = 0x80000 };

AosMermanDrop aos_merman_drop_create(int index, int32_t x, int32_t y, bool facing_left) {
    int32_t vx = (index + 1) * 65536;
    if (facing_left) vx = -vx;
    int32_t ax = -vx;
    ax = (ax < 0 ? ax + 0x7F : ax) >> 7;
    return (AosMermanDrop){.x = x, .y = y, .vx = vx, .ax = ax, .gravity = 0x2000,
                           .facing_left = facing_left, .splash = (uint8_t)(index + 2)};
}

/* sub_0806D430 and the Y half of sub_0806D3D8: the speed gains the
 * acceleration, capped on the acceleration's side, then moves. */
static void move_axis(int32_t *position, int32_t *speed, int32_t acceleration) {
    *speed += acceleration;
    if (acceleration >= 0) {
        if (*speed > AOS_MERMAN_LIMIT) *speed = AOS_MERMAN_LIMIT;
    } else if (*speed < -AOS_MERMAN_LIMIT) {
        *speed = -AOS_MERMAN_LIMIT;
    }
    *position += *speed;
}

bool aos_merman_drop_update(AosMermanDrop *drop, int cam_x, int cam_y, int mx, int my,
                            bool *splash) {
    *splash = false;
    switch (drop->state) {
    case 0:
        drop->attacking = true;
        drop->timer = 0x10;
        drop->state = 1;
        /* fall through */
    case 1:
        move_axis(&drop->x, &drop->vx, drop->ax);
        if (drop->timer != 0) {
            if (drop->timer == drop->splash) *splash = true;
            drop->timer--;
            break;
        }
        drop->attacking = false;
        drop->state = 2;
        /* fall through */
    case 2:
        move_axis(&drop->x, &drop->vx, drop->ax);
        move_axis(&drop->y, &drop->vy, drop->gravity);
        break;
    case 3: return false;
    default: break;
    }
    /* + 0x59 bit 2 is set at the start of every update. */
    int sx = (drop->x >> 16) - cam_x, sy = (drop->y >> 16) - cam_y;
    if (sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0) drop->state = 3;
    return true;
}
