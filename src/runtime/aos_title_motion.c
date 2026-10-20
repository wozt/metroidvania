/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_motion.h"

/* Native GameModeTitleScreenUpdate case 3, code_08002454.c.
 * TitleScreen_sub_08002FF8, palette writes and BgAffineSet remain external.
 * Signed fixed-point arithmetic reproduces the positive native trajectory;
 * the original negative-value rounding path is retained for the scale. */
void aos_title_motion_stage3(AosTitleMotion *state) {
    state->palette_trigger = false;
    state->stage_four = false;
    if (state->motion == 0) return;
    state->motion -= 0x2000;
    state->x -= 0x2000;
    state->y -= 0x2000;
    if (state->motion == 0x40000)
        state->palette_trigger = true;
    if (state->motion < 0x30000) {
        state->flags |= 1;
        state->stage_four = true;
    }
    int32_t rounded = state->motion;
    if (rounded < 0) rounded += 0x7fff;
    /* Arithmetic right shift of the native 32-bit signed GBA integer. */
    int32_t quotient = rounded >= 0 ? (rounded >> 15)
                                         : -(((-rounded) + 32767) / 32768);
    state->affine_scale = 0x108 - quotient;
}
