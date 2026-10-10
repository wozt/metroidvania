/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow room exits; comments name the cvaos routine. */
#include "aos_room.h"

/* sub_08011A44: unsigned comparisons, so negative positions are outside. */
bool aos_room_outside(const AosCollision *layer, int width_screens, int height_screens,
                      int32_t x, int32_t y) {
    uint32_t right = width_screens > 1 ? (uint32_t)width_screens << 8 : 0xF0u;
    uint32_t bottom = height_screens > 1 ? ((uint32_t)height_screens << 8) - 0x30u : 0xD0u;
    if ((uint32_t)x > right) return true;
    if ((uint32_t)y < 0x30u || (uint32_t)y > bottom) return true;
    return layer && aos_collision_cell(layer, x, y) == AOS_EXIT_CELL;
}

/* sub_08010244 */
void aos_room_exit_velocity(AosSoma *soma, int screen_x, int screen_y) {
    if (screen_y < 0x31 || screen_y > 0xCF) return;
    if (screen_x < 0x78 ? soma->vx > 0 : soma->vx < 0) {
        soma->vx = 0;
        soma->friction = 0;
    }
    if (soma->vy < -0x50000) soma->vy = -0x10000;
}

/* sub_08010350: the screen column and row Soma leaves through. */
const AosTransition *aos_room_find_exit(const AosTransition *list, size_t count,
                                        int width_screens, int height_screens,
                                        int32_t x, int32_t y) {
    int column = x >> 8;
    if (width_screens == 1 && x > 0xF0) column = 1;
    int row;
    if (height_screens > 1) {
        if (y < 0x30) row = (y - 0x30) >> 8;
        else if (y > (height_screens << 8) - 0x30) row = (y + 0x30) >> 8;
        else row = y >> 8;
    } else {
        /* The camera of a one-screen room sits at y = 0x30. */
        row = (y - 0x30) >> 8;
        if (y - 0x30 > 0xA0) row += 1;
    }
    for (size_t i = 0; i < count; ++i)
        if (list[i].screen_x == column && list[i].screen_y == row) return &list[i];
    return NULL;
}

/* sub_08010350 places Soma on screen at the position below; sub_0800F9EC
 * then loads the target with BG1 at (load_x, load_y + 0x30) through
 * sub_0800ED5C / sub_0800EE54 (BG1 scrolls 1:1), and sub_0803FBBC pins
 * one-screen axes to x = 0 / y = 0x30. */
void aos_room_arrival(const AosTransition *exit, int target_width_screens,
                      int target_height_screens, int32_t x, int32_t y,
                      int32_t *out_x, int32_t *out_y) {
    int32_t local_x, local_y;
    if (x < 0) {
        local_x = x + 0xF0;
    } else {
        local_x = x & 0xFF;
        if (local_x > 0xF0) local_x -= 0xF0;
    }
    local_x = (int16_t)(local_x + exit->adjust_x);
    if (y < 0x30) {
        local_y = y + 0x70;
    } else {
        local_y = (y - 0x30) & 0xFF;
        if (local_y > 0xA0) local_y -= 0xA0;
    }
    *out_x = (target_width_screens > 1 ? exit->load_x : 0) + local_x;
    *out_y = (target_height_screens > 1 ? exit->load_y + 0x30 : 0x30) + local_y;
}
