/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_ghost.h"

#include "aos_enemy.h"

#include <limits.h>

static int32_t whole(int32_t value) {
    return value < 0 ? -((-value) >> 16) : value >> 16;
}

static int16_t turn_toward(int16_t angle, int16_t wanted) {
    int32_t difference = (uint16_t)wanted - (uint16_t)angle;
    if (difference > 0x7FFF) difference -= 0x10000;
    if (difference < -0x8000) difference += 0x10000;
    return (int16_t)(angle + (difference > 0 ? 0x300 : -0x300));
}

static void steer(AosGhostSoul *ghost, int target_x, int target_y) {
    int16_t dx = (int16_t)(target_x - (int16_t)(ghost->x >> 16));
    int16_t dy = (int16_t)(target_y - (int16_t)(ghost->y >> 16));
    ghost->angle = turn_toward(ghost->angle, (int16_t)aos_arctan2(dx, dy));
}

static void fly(AosGhostSoul *ghost) {
    if (ghost->speed <= 0x3FFFF) ghost->speed += 0x2000;
    ghost->vx = (aos_sine((uint32_t)(ghost->angle + 0x4000)) >> 8) *
                (ghost->speed >> 8);
    ghost->vy = (aos_sine((uint32_t)ghost->angle) >> 8) * (ghost->speed >> 8);
    ghost->x += ghost->vx;
    ghost->y += ghost->vy;
}

AosGhostSoul aos_ghost_soul_create(int index, int32_t x, int32_t y, uint32_t random) {
    AosGhostSoul ghost = {.target = -1};
    ghost.angle = (int16_t)((random & 0xFFF) - 0x7800 + index * 0x6000);
    ghost.x = x + aos_sine((uint32_t)(ghost.angle + 0x4000)) * 16;
    ghost.y = y + aos_sine((uint32_t)ghost.angle) * 16;
    return ghost;
}

void aos_ghost_soul_launch(AosGhostSoul *ghost, const AosSkillTarget targets[4]) {
    int best = -1;
    uint32_t best_distance = UINT_MAX;
    for (int i = 0; i < 4; ++i) {
        if (!targets[i].present) continue;
        int32_t dx = whole(targets[i].x) - (int16_t)(ghost->x >> 16);
        int32_t dy = whole(targets[i].y) - (int16_t)(ghost->y >> 16);
        uint32_t distance = (uint32_t)(dx * dx + dy * dy);
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    ghost->target = best;
    ghost->state = best >= 0 ? 2 : 3;
    ghost->timer = AOS_GHOST_TIMER;
}

void aos_ghost_soul_update(AosGhostSoul *ghost, const AosSkillTarget *target,
                           uint32_t (*random)(void)) {
    if (ghost->state == 2) {
        if (target && target->present)
            steer(ghost, whole(target->x), whole(target->y));
        fly(ghost);
        ghost->timer--;
        if (!target || !target->present || target->hp <= 0 || ghost->struck || !ghost->timer)
            ghost->state = 4;
    } else if (ghost->state == 3) {
        int target_x = (int)(random() & 0x1FF);
        int target_y = (int)(random() & 0xFF);
        steer(ghost, target_x, target_y);
        fly(ghost);
        ghost->timer--;
        if (!ghost->timer) ghost->state = 4;
    } else if (ghost->state == 4) {
        fly(ghost);
    }
}

bool aos_ghost_soul_outside(const AosGhostSoul *ghost, int cam_x, int cam_y, int mx, int my) {
    int sx = (ghost->x >> 16) - cam_x, sy = (ghost->y >> 16) - cam_y;
    return sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0;
}
