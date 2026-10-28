/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_throw.h"

enum { AOS_THROW_LIMIT = 0x80000 };

bool aos_throw_supported(const AosThrowRecord *record) {
    return !(record->flags & ~AOS_THROW_PORTED_FLAGS);
}

AosThrow aos_throw_create(int32_t soma_x, int32_t soma_y, bool facing_left) {
    return (AosThrow){.x = soma_x + (facing_left ? -8 : 8) * 65536, .y = soma_y - 0x1A * 65536,
                      .facing_left = facing_left};
}

/* sub_0806D430 / sub_0806D460 for one axis with no acceleration sign
 * flip: the speed gains the acceleration, is capped at the limit on the
 * acceleration's side, and moves the position. */
static void move_axis(int32_t *position, int32_t *speed, int32_t acceleration) {
    *speed += acceleration;
    if (acceleration >= 0) {
        if (*speed > AOS_THROW_LIMIT) *speed = AOS_THROW_LIMIT;
    } else if (*speed < -AOS_THROW_LIMIT) {
        *speed = -AOS_THROW_LIMIT;
    }
    *position += *speed;
}

bool aos_throw_update(AosThrow *entity, const AosThrowRecord *record, int cam_x, int cam_y,
                      int mx, int my) {
    switch (entity->state) {
    case 0:
        /* The 0x80 path takes an affine slot (sub_0803E654); the spin is
         * negated facing left. */
        if (record->flags & AOS_THROW_SPIN)
            entity->spin = (int16_t)(entity->facing_left ? -record->spin : record->spin);
        entity->vx = entity->facing_left ? -record->vx : record->vx;
        entity->vy = record->vy;
        entity->gravity = record->gravity;
        entity->state = 1;
        break;
    case 1:
        /* Neither terrain flag (2, 4): free flight; the x acceleration
         * (+ 0x50) stays 0. */
        move_axis(&entity->x, &entity->vx, 0);
        move_axis(&entity->y, &entity->vy, entity->gravity);
        break;
    case 3: return false;
    default: break;
    }
    /* + 0x59 bit 2 was set by sub_08019478, so only the margins count. */
    int sx = (entity->x >> 16) - cam_x, sy = (entity->y >> 16) - cam_y;
    if (sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0) entity->state = 3;
    if (record->flags & AOS_THROW_SPIN) entity->angle = (uint16_t)(entity->angle + entity->spin);
    return true;
}
