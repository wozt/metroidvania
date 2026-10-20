/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_input.h"

enum { KEY_A = 1, KEY_B = 2, KEY_START = 8, KEY_RIGHT = 16,
       KEY_LEFT = 32, KEY_UP = 64, KEY_DOWN = 128 };

void aos_title_input_enter(AosTitleInput *state) {
    state->idle_timer = 0x258;
    state->timed_out = false;
    state->leaving = false;
}

AosTitleAction aos_title_input_tick(AosTitleInput *state, uint16_t newly,
                                    uint16_t repeated, bool logo_selected,
                                    bool submenu_open, bool exit_blocked) {
    /* GameModeTitleScreenUpdate case 8: repeat up/down resets idle timer. */
    if (repeated & (KEY_UP | KEY_DOWN))
        state->idle_timer = 10 * 60;
    if (--state->idle_timer == 0)
        state->timed_out = true;

    if ((newly & (KEY_START | KEY_A)) && logo_selected)
        return AOS_TITLE_SUBMENU;
    if (!(newly & (KEY_START | KEY_A)) && (newly & KEY_B) && submenu_open)
        return AOS_TITLE_BACK;
    if (!exit_blocked && (state->timed_out || (newly & (KEY_START | KEY_A)))) {
        state->leaving = true;
        return AOS_TITLE_EXIT;
    }
    return AOS_TITLE_STAY;
}
