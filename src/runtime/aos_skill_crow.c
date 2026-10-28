/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_crow.h"

#include "aos_enemy.h"

AosCrow aos_crow_create(bool soma_facing_left) {
    AosCrow crow = {.angle = 0x1000, .step = (int16_t)0xFEB9, .timer = AOS_CROW_FRAMES,
                    .facing_left = !soma_facing_left};
    if (soma_facing_left) {
        crow.angle = 0x7000;
        crow.step = (int16_t)-crow.step;
    }
    return crow;
}

bool aos_crow_update(AosCrow *crow, int32_t soma_x, int32_t soma_y) {
    if (crow->state == 2) return false;
    crow->state = 1;
    int32_t scaled = aos_sine((uint32_t)(crow->timer * 0x28F)) * 60;
    crow->radius = scaled < 0 ? -((-scaled) >> 16) : scaled >> 16;
    crow->angle = (int16_t)(crow->angle + crow->step);
    crow->x = soma_x + crow->radius * aos_sine((uint32_t)(crow->angle + 0x4000));
    crow->y = soma_y + crow->radius * aos_sine((uint32_t)crow->angle) - 0x120000;
    if (crow->timer) crow->timer--;
    else crow->state = 2;
    return true;
}
