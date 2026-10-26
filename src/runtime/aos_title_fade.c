/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_fade.h"

enum { AOS_BRIGHTNESS_MAX = 16 };

int aos_title_brightness_lower(uint16_t *brightness, uint16_t step) {
    if (!step) return *brightness == 0;
    if (*brightness >= step) {
        *brightness = (uint16_t)(*brightness - step);
        return 0;
    }
    *brightness = 0;
    return 1;
}

int aos_title_brightness_raise(uint16_t *brightness, uint16_t step) {
    if (!step) return *brightness >= AOS_BRIGHTNESS_MAX;
    if (*brightness < AOS_BRIGHTNESS_MAX) {
        unsigned next = (unsigned)*brightness + step;
        *brightness = (uint16_t)(next > AOS_BRIGHTNESS_MAX ? AOS_BRIGHTNESS_MAX : next);
        return 0;
    }
    return 1;
}

/* GameModeTitleScreenUpdate stage 0 in code_08002454.c.
 * Native BLDCNT selects brightness decrease and BLDY starts at 16. */
void aos_title_fade_init(AosTitleFade *state) {
    state->stage = 1;
    state->brightness = 16;
    state->fade_counter = 0;
    state->stage_timer = 0;
    state->alpha_blend = 0x0010;
}

/* Stage 1 performs asset and entity initialization (still unported).
 * The native tail enters stage 2 and executes its first tick immediately. */
void aos_title_fade_begin(AosTitleFade *state) {
    if (state->stage != 1) return;
    state->stage = 2;
    state->stage_timer = 0;
    aos_title_fade_tick(state);
}

/* Native stage 2: BLDY = 16 - (unk_4 >> 1), increment unk_4 while
 * positive; otherwise enable alpha blending and enter stage 3.
 * Do not synthesize the native sprite motion or graphical resources. */
void aos_title_fade_tick(AosTitleFade *state) {
    if (state->stage != 2) return;
    state->brightness = (uint16_t)(16u - (state->fade_counter >> 1));
    if (state->brightness > 0) {
        ++state->fade_counter;
    } else {
        state->alpha_blend = 1;
        state->stage = 3;
        state->stage_timer = 0;
    }
}
