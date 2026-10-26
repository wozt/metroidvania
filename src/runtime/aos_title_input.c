/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_input.h"
#include "aos_input.h"

void aos_title_input_enter(AosTitleInput *state) {
    state->idle_timer = 0x258;
    state->exit_counter = 0;
    state->brightness = 16;
    state->timed_out = false;
    state->leaving = false;
}

AosTitleAction aos_title_input_tick(AosTitleInput *state, uint16_t newly,
                                    uint16_t repeated, bool logo_selected,
                                    bool submenu_open, bool exit_blocked) {
    /* GameModeTitleScreenUpdate case 8: repeat up/down resets idle timer. */
    if (repeated & (AOS_KEY_UP | AOS_KEY_DOWN))
        state->idle_timer = 10 * 60;
    if (--state->idle_timer == 0)
        state->timed_out = true;

    if ((newly & (AOS_KEY_START | AOS_KEY_A)) && logo_selected)
        return AOS_TITLE_SUBMENU;
    if (!(newly & (AOS_KEY_START | AOS_KEY_A)) && (newly & AOS_KEY_B) && submenu_open)
        return AOS_TITLE_BACK;
    if (!exit_blocked && (state->timed_out || (newly & (AOS_KEY_START | AOS_KEY_A)))) {
        state->leaving = true;
        state->exit_counter = 0;
        state->brightness = 0;
        return AOS_TITLE_EXIT;
    }
    return AOS_TITLE_STAY;
}

bool aos_title_exit_tick(AosTitleInput *state) {
    if (!state->leaving) return false;
    state->brightness = (uint16_t)(state->exit_counter >> 1);
    if (state->brightness == 16) return true;
    ++state->exit_counter;
    return false;
}
