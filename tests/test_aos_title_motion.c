/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_motion.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosTitleMotion state = {.motion=0x42000, .x=0x980000, .y=0x700000};
    aos_title_motion_stage3(&state);
    assert(state.motion == 0x40000);
    assert(state.x == 0x97e000 && state.y == 0x6fe000);
    assert(state.palette_trigger && !state.stage_four);
    assert(state.affine_scale == 0x100);
    aos_title_motion_stage3(&state);
    assert(!state.palette_trigger && state.motion == 0x3e000);
    while (state.motion >= 0x30000)
        aos_title_motion_stage3(&state);
    assert(state.motion == 0x2e000 && state.stage_four);
    assert(state.flags & 1);
    int32_t x = state.x, y = state.y;
    state.motion = 0;
    aos_title_motion_stage3(&state);
    assert(state.x == x && state.y == y);
    assert(!state.palette_trigger && !state.stage_four);
    puts("Aria title motion tests passed");
    return 0;
}
