/* SPDX-License-Identifier: GPL-3.0-only */
/* The launcher's split screen: see metroidvania_split.h. */
#include "metroidvania_split.h"

#include <math.h>
#include <string.h>

uint16_t mv_split_angle(uint32_t frame) {
    return (uint16_t)(((uint64_t)(frame % MV_FRAMES_PER_TURN) << 16) / MV_FRAMES_PER_TURN);
}

static void normal(uint16_t angle, double *nx, double *ny) {
    double a = angle * (6.283185307179586 / 65536.0);
    *nx = sin(a);
    *ny = -cos(a);
}

MvGame mv_split_pixel(uint16_t angle, int x, int y) {
    double nx, ny;
    normal(angle, &nx, &ny);
    double dx = x + 0.5 - MV_SCREEN_W / 2.0, dy = y + 0.5 - MV_SCREEN_H / 2.0;
    return dx * nx + dy * ny > 0 ? MV_GAME_ARIA : MV_GAME_MZM;
}

MvGame mv_split_top(uint16_t angle) {
    return angle < 0x4000 || angle >= 0xC000 ? MV_GAME_ARIA : MV_GAME_MZM;
}

void mv_split_compose(uint16_t angle, const uint8_t *aria, const uint8_t *mzm, uint8_t *out) {
    double nx, ny;
    normal(angle, &nx, &ny);
    for (int y = 0; y < MV_SCREEN_H; ++y) {
        double dy = y + 0.5 - MV_SCREEN_H / 2.0;
        for (int x = 0; x < MV_SCREEN_W; ++x) {
            double dx = x + 0.5 - MV_SCREEN_W / 2.0;
            size_t at = ((size_t)y * MV_SCREEN_W + (size_t)x) * 4;
            memcpy(out + at, (dx * nx + dy * ny > 0 ? aria : mzm) + at, 4);
        }
    }
}
