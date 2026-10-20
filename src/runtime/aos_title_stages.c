/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_stages.h"

static int32_t scale_for_motion(int32_t motion) {
    int32_t rounded = motion;
    if (rounded < 0) rounded += 0x7fff;
    /* The native arithmetic right shift is a floor division by 32768. */
    int32_t q = rounded >= 0 ? rounded / 32768 : -(((-rounded) + 32767) / 32768);
    return 0x108 - q;
}

/* Stage 4 of GameModeTitleScreenUpdate. Native stage 4 falls into stage 5
 * in the very same call when entity flag 2 becomes set. */
void aos_title_stage4(AosTitleStages *state) {
    if (state->stage != 4) return;
    if (state->motion != 0) {
        state->motion -= 0x2000;
        state->x -= 0x2000;
        state->y -= 0x2000;
        state->affine_scale = scale_for_motion(state->motion);
    }
    if (state->flags & 2u) {
        state->stage = 5;
        state->stage_timer = 0;
        aos_title_stage5(state);
    }
}

/* Stage 5's affine contraction and 64-frame progress counter.
 * Palette/graphics mutations represented here by the original state values;
 * the actual GBA BG and object rendering is not yet ported. */
void aos_title_stage5(AosTitleStages *state) {
    if (state->stage != 5) return;
    state->affine_scale = 0x118 - state->progress;
    if (state->flags & 2u)
        state->entity_53e_1 = 2;
    if (state->progress < 0x40u) {
        ++state->progress;
    } else {
        state->other_timer = 0x88;
        state->alpha = 16;
        state->other_flags |= 1u;
        state->stage = 6;
        state->stage_timer = 0;
    }
}
