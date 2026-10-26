/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_fade.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    uint16_t brightness = 16;
    for (unsigned frame = 0; frame < 8; ++frame) {
        assert(!aos_title_brightness_lower(&brightness, 2));
        assert(brightness == 14 - frame * 2);
    }
    assert(aos_title_brightness_lower(&brightness, 2));
    assert(brightness == 0);
    for (unsigned frame = 0; frame < 8; ++frame) {
        assert(!aos_title_brightness_raise(&brightness, 2));
        assert(brightness == (frame + 1) * 2);
    }
    assert(aos_title_brightness_raise(&brightness, 2));
    assert(brightness == 16);

    AosTitleFade state = {0};
    aos_title_fade_init(&state);
    assert(state.stage == 1 && state.brightness == 16);
    assert(state.fade_counter == 0 && state.alpha_blend == 0x10);
    aos_title_fade_tick(&state);
    assert(state.stage == 1);
    aos_title_fade_begin(&state);
    assert(state.stage == 2 && state.fade_counter == 1 && state.brightness == 16);
    for (unsigned i = 1; i < 32; ++i) {
        aos_title_fade_tick(&state);
        assert(state.brightness == 16 - (i >> 1));
    }
    assert(state.stage == 2 && state.fade_counter == 32);
    aos_title_fade_tick(&state);
    assert(state.stage == 3 && state.brightness == 0);
    assert(state.alpha_blend == 1 && state.stage_timer == 0);
    aos_title_fade_tick(&state);
    assert(state.stage == 3 && state.fade_counter == 32);
    puts("Aria title fade tests passed");
    return 0;
}
