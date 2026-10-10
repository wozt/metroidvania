/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow BG1 collision primitives; comments name the cvaos routine
 * each function reproduces. */
#include "aos_collision.h"

/* sub_08001A00 + sub_08001800 on the exported per-cell table. */
uint8_t aos_collision_cell(const AosCollision *layer, int32_t x, int32_t y) {
    if (!layer || !layer->cells) return 0;
    int32_t cx = x >> 3, cy = y >> 3;
    int32_t max_x = layer->width_screens != 1 ? layer->width_screens << 5 : 0x1E;
    int32_t max_y = layer->height_screens != 1 ? layer->height_screens << 5 : 0x1A;
    if (cx < 0) cx = 0;
    else if (cx >= max_x) cx = max_x - 1;
    if (cy < 0) cy = 0;
    else if (cy >= max_y) cy = max_y - 1;
    if (cx >= layer->width_cells || cy >= layer->height_cells) return 0;
    return layer->cells[cy * layer->width_cells + cx];
}

/* sub_08001B40 */
int aos_slope_height(uint8_t value, int32_t x) {
    int height = (value & 0x30) >> 3;
    if (value & 0xC0) {
        int local = ((value & 4) ? 7 - x : x) & 7;
        height += local >> ((value >> 6) - 1);
    }
    return height;
}

/* sub_08001BA0: bit-3 cells under the walk modes 1 and 2. */
uint8_t aos_collision_mode(uint8_t value, int mode) {
    if (mode == 1) {
        if (value & 8) value = (value & 1) ? 9 : (uint8_t)((value & ~8) | 1);
    } else if (mode == 2) {
        if (!(value & 8)) value = 0;
        else if (value & 1) value ^= 10;
        else value = (uint8_t)((value & ~8) | 1);
    }
    return value;
}

/* sub_08001C1C (use_mode false) and sub_08001CCC (use_mode true). */
int aos_ceiling_depth(const AosCollision *layer, int32_t x, int32_t y, int mode,
                      bool use_mode) {
    int depth = 0;
    while (depth < 9) {
        uint8_t value = aos_collision_cell(layer, x, y);
        if (use_mode) value = aos_collision_mode(value, mode);
        if (!(value & 2)) break;
        if (value == 0xFF) {
            int step = 16 - (y & 15);
            depth += step;
            y += step;
            continue;
        }
        if (value & 0xC0) {
            depth += 1 - (y & 7) + aos_slope_height(value, x);
            if (depth < 0) depth = 0;
            break;
        }
        int step = 8 - (y & 7);
        depth += step;
        y += step;
    }
    return depth;
}

/* sub_08001D94 (use_mode false) and sub_08001E58 (use_mode true). */
int aos_floor_depth(const AosCollision *layer, int32_t x, int32_t y, int mode,
                    bool use_mode) {
    int depth = 0;
    while (depth >= -8) {
        uint8_t value = aos_collision_cell(layer, x, y);
        if (use_mode) value = aos_collision_mode(value, mode);
        if (!(value & 1)) break;
        if (value == 0xFF) {
            int step = -(y & 15) - 1;
            depth += step;
            y += step;
            continue;
        }
        if ((value & 0xC0) && !(value & 2)) {
            depth += -(y & 7) - 1 + aos_slope_height(value, x);
            if (depth > 0) depth = 0;
            break;
        }
        int step = -(y & 7) - 1;
        depth += step;
        y += step;
    }
    return depth;
}

/* sub_08001F3C */
bool aos_collision_special(const AosCollision *layer, int32_t x, int32_t y) {
    uint8_t value = aos_collision_cell(layer, x, y);
    int slope = (value & 0xC0) ? ((value & 2) ? 2 : 1) : 0;
    int special = (value & 8) ? ((value & 1) ? 2 : 1) : 0;
    if (slope && special) {
        int height = aos_slope_height(value, x);
        int local = y & 7;
        int distance = slope == 1 ? height - local : local - height;
        return special == 1 ? distance <= 0 : distance >= 0;
    }
    return special != 0;
}

/* sub_080022E8 via sub_08002058: wall on the right, push left. */
int aos_wall_push_left(const AosCollision *layer, int32_t x, int32_t y) {
    uint8_t value = aos_collision_cell(layer, x, y);
    if (!(value & 2)) return 0;
    if (value == 0xFF) return ~(x & 15);
    if (!(value & 0xC0)) return ~(x & 7);
    return 0;
}

/* sub_080022A8 via sub_0800207C: wall on the left, push right. */
int aos_wall_push_right(const AosCollision *layer, int32_t x, int32_t y) {
    uint8_t value = aos_collision_cell(layer, x, y);
    if (!(value & 2)) return 0;
    if (value == 0xFF) return 16 - (x & 15);
    if (!(value & 0xC0)) return 8 - (x & 7);
    return 0;
}
