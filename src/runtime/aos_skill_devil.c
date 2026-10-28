/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_devil.h"

#include "aos_enemy.h"

AosDevilFire aos_devil_fire_create(int index, int32_t x, int32_t y, bool facing_left) {
    int32_t angle = index * 0x1800 - 0x1800;
    if (facing_left) angle = -0x8000 - angle;
    return (AosDevilFire){.x = x, .y = y, .angle = (int16_t)angle, .target = -1};
}

void aos_devil_fire_launch(AosDevilFire *fire, uint32_t random, const bool slots[4]) {
    unsigned slot = random & 3;
    fire->target = -1;
    for (int tries = 0; tries < 4; ++tries) {
        if (slots[slot]) {
            fire->target = (int)slot;
            break;
        }
        slot = (slot + 1) & 3;
    }
    fire->state = fire->target >= 0 ? 2 : 3;
    fire->timer = AOS_DEVIL_TIMER;
}

static int32_t whole(int32_t value) {
    return value < 0 ? -((-value) >> 16) : value >> 16;
}

/* The velocity of the angle and speed, then sub_0806D3D8(e, 8.0) with no
 * acceleration (only the upper clamp applies). */
static void fly(AosDevilFire *fire) {
    if (fire->speed <= 0x4FFFF) fire->speed += 0x4000;
    fire->vx = (aos_sine((uint32_t)(fire->angle + 0x4000)) >> 8) * (fire->speed >> 8);
    fire->vy = (aos_sine((uint32_t)fire->angle) >> 8) * (fire->speed >> 8);
    if (fire->vx > 0x80000) fire->vx = 0x80000;
    if (fire->vy > 0x80000) fire->vy = 0x80000;
    fire->x += fire->vx;
    fire->y += fire->vy;
}

void aos_devil_fire_update(AosDevilFire *fire, const AosSkillTarget *target) {
    if (fire->state == 3) {
        fly(fire);
        return;
    }
    if (fire->state != 2) return;
    if (fire->speed <= 0x4FFFF) fire->speed += 0x4000;
    int32_t angle = fire->angle;
    if (target && target->present) {
        int16_t dx = (int16_t)(whole(target->x) - (int16_t)(fire->x >> 16));
        int16_t dy = (int16_t)(whole(target->y) - (int16_t)(fire->y >> 16));
        int32_t difference = (int32_t)aos_arctan2(dx, dy) - angle;
        if (difference > 0) angle += difference <= 0x7FFF ? 0x100 : -0x100;
        else angle += difference < -0x8000 ? 0x100 : -0x100;
        fire->angle = (int16_t)angle;
    }
    fire->vx = (aos_sine((uint32_t)(fire->angle + 0x4000)) >> 8) * (fire->speed >> 8);
    fire->vy = (aos_sine((uint32_t)fire->angle) >> 8) * (fire->speed >> 8);
    if (fire->vx > 0x80000) fire->vx = 0x80000;
    if (fire->vy > 0x80000) fire->vy = 0x80000;
    fire->x += fire->vx;
    fire->y += fire->vy;
    fire->timer--;
    if (!target || !target->present || target->hp <= 0 || fire->struck || fire->timer == 0)
        fire->state = 3;
}

bool aos_devil_fire_outside(const AosDevilFire *fire, int cam_x, int cam_y, int mx, int my) {
    int sx = (fire->x >> 16) - cam_x, sy = (fire->y >> 16) - cam_y;
    return sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0;
}

uint8_t aos_devil_fire_frame(const AosDevilFire *fire) {
    int32_t sum = fire->angle + 0x4000;
    if (sum < 0) sum += 0xFFF;
    return (uint8_t)((sum >> 12) & 0xF);
}
