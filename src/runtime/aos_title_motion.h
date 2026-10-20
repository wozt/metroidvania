/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_TITLE_MOTION_H
#define AOS_TITLE_MOTION_H
#include <stdbool.h>
#include <stdint.h>

/* Bound to title entity slot 0x12 native fixed-point fields. */
typedef struct {
    int32_t motion;
    int32_t x;
    int32_t y;
    uint16_t flags;
    int32_t affine_scale;
    bool palette_trigger;
    bool stage_four;
} AosTitleMotion;

/* One iteration of GameModeTitleScreenUpdate stage 3 only. */
void aos_title_motion_stage3(AosTitleMotion *state);
#endif
