/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_axe.h"

AosAxe aos_axe_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    /* Only the integer halves are copied (strh to + 0x42, + 0x46). */
    AosAxe axe = {.x = (int32_t)((uint32_t)(int16_t)(soma_x >> 16) << 16),
                  .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16)};
    axe.vx = soma_facing_left ? -0x34000 : 0x34000;
    axe.ax = soma_facing_left ? 0x1000 : -0x1000;
    axe.spin = soma_facing_left ? -1 : 1;
    return axe;
}

bool aos_axe_update(AosAxe *axe, int cam_x, int cam_y, int mx, int my) {
    if (axe->state == 2) return false;
    axe->state = 1;
    axe->x += axe->vx;
    if ((axe->ax < 0 && axe->vx > -0x80000) || (axe->ax > 0 && axe->vx <= 0x7FFFF))
        axe->vx += axe->ax;
    axe->angle = (uint16_t)(axe->turns << 11);
    axe->turns = (int16_t)(axe->turns + axe->spin);
    int sx = (axe->x >> 16) - cam_x, sy = (axe->y >> 16) - cam_y;
    if (sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0) axe->state = 2;
    return true;
}

AosAxeTrail aos_axe_trail_create(int index, bool soma_facing_left) {
    AosAxeTrail trail = {.lag = (uint8_t)(3 + 3 * index)};
    trail.step = soma_facing_left ? -0x800 : 0x800;
    trail.angle = -trail.step * trail.lag;
    return trail;
}

bool aos_axe_trail_update(AosAxeTrail *trail, const AosAxe *axe) {
    int16_t axe_x = (int16_t)(axe->x >> 16);
    if (trail->state == 0) {
        for (int i = 0; i < AOS_AXE_HISTORY; ++i) trail->history[i] = axe_x;
        trail->state = 1;
    }
    if (axe->state == 2) return false;
    for (int i = AOS_AXE_HISTORY - 1; i > 0; --i) trail->history[i] = trail->history[i - 1];
    trail->history[0] = axe_x;
    trail->x = trail->history[trail->lag] * 65536;
    trail->y = (int32_t)((uint32_t)(int16_t)(axe->y >> 16) << 16);
    trail->angle += trail->step;
    return true;
}
