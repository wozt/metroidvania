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

/* GameModeTitleScreenUpdate stage 6: fade secondary title object out.
 * The native sub_080009E4 trig polynomial, affine BG transform, and
 * TitleScreen_sub_08002FF8 remain outside this deliberately bounded port. */
void aos_title_stage6(AosTitleStages *state) {
    if (state->stage != 6) return;
    state->affine_angle = (uint16_t)(state->affine_angle + 0x40u);
    if (state->other_timer != 0) {
        --state->other_timer;
        state->alpha = (uint16_t)(state->other_timer >> 3);
    } else {
        state->other_flags |= 4u;
        state->alpha = 16;
        state->show_bg1 = true;
        state->stage = 7;
        state->stage_timer = 0;
    }
}

/* Stage 7: native title reveal deceleration and the handoff to stage 8.
 * The real display-register effects, glyph initialization and sprite
 * updates remain unimplemented; event flags expose the transition. */
void aos_title_stage7(AosTitleStages *state) {
    if (state->stage != 7) return;
    state->affine_angle = (uint16_t)(state->affine_angle + 0x40u);
    state->palette_event = false;
    if (state->reveal_motion != 0) {
        if (!(state->other_flags & 2u) && state->reveal_motion <= 0x37f)
            state->other_flags |= 2u;
        int32_t before = state->reveal_motion;
        state->reveal_motion = before - 2 - ((before / 256) * 256) / 80;
        if (state->reveal_motion < 0xc0)
            state->reveal_motion = 0;
        int32_t eva = state->reveal_motion / 1280;
        state->alpha = (uint16_t)(eva < 0 ? 0 : eva > 16 ? 16 : eva);
    } else {
        state->show_bg0 = true;
        state->palette_event = true;
        state->idle_timer = 0x258;
        state->stage = 8;
        state->stage_timer = 0;
    }
}

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
