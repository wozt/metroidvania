/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_TITLE_FADE_H
#define AOS_TITLE_FADE_H
#include <stdint.h>

/* The title mode's brightness initialization and stage-2 fade-in only.
 * Other title sprite, VRAM and animation operations remain unported. */
typedef struct {
    uint16_t stage;
    uint16_t brightness;
    uint16_t fade_counter;
    uint16_t stage_timer;
    uint16_t alpha_blend;
} AosTitleFade;

void aos_title_fade_init(AosTitleFade *state);
void aos_title_fade_begin(AosTitleFade *state);
void aos_title_fade_tick(AosTitleFade *state);
/* Shared BLDY stepping used by the title-adjacent startup modes. The return
 * becomes true on the native frame after the endpoint was first written. */
int aos_title_brightness_lower(uint16_t *brightness, uint16_t step);
int aos_title_brightness_raise(uint16_t *brightness, uint16_t step);
#endif
