/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_lifecycle.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosGameLifecycle state;

    /* Existing room, save and debug launches enter the old gameplay loop
     * through the native IN_GAME handler. */
    aos_game_lifecycle_init(&state, true);
    assert(state.game_mode.mode == AOS_MODE_IN_GAME);
    aos_game_lifecycle_update(&state);
    assert(state.in_game_frame_requested && !state.title_frame_requested);

    /* Native boot crosses Reset, Licensed and Konami before reaching title.
     * Start shortens only the Konami hold, leaving both fade chains intact. */
    aos_game_lifecycle_init(&state, false);
    assert(state.game_mode.mode == AOS_MODE_RESET);
    bool saw_licensed = false, saw_konami = false;
    for (unsigned frame = 0; frame < 1000 && state.game_mode.mode != AOS_MODE_TITLE_SCREEN;
         ++frame) {
        saw_licensed |= state.game_mode.mode == AOS_MODE_LICENSED_BY_NINTENDO;
        saw_konami |= state.game_mode.mode == AOS_MODE_KONAMI_LOGO;
        state.input.new_input = state.game_mode.mode == AOS_MODE_KONAMI_LOGO &&
                                        state.game_mode.update_stage == 3
                                    ? AOS_KEY_START
                                    : 0;
        aos_game_lifecycle_update(&state);
    }
    assert(saw_licensed && saw_konami && state.game_mode.mode == AOS_MODE_TITLE_SCREEN);

    /* The title handler owns the existing fade and motion stages. */
    aos_game_lifecycle_update(&state);
    assert(state.title_frame_requested && state.game_mode.update_stage == 1);
    aos_game_lifecycle_update(&state);
    assert(state.title_frame_requested && state.game_mode.update_stage == 2);
    for (unsigned frame = 0; frame < 40 && state.game_mode.update_stage == 2; ++frame)
        aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 3);

    state.title_motion = (AosTitleMotion){.motion = 0x42000, .x = 0x980000, .y = 0x700000};
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 3 && state.title_motion.motion == 0x40000 &&
           state.title_motion.palette_trigger);
    while (state.game_mode.update_stage == 3) aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 4 && state.title_stages.flags & 1);

    /* Already-ported stages 4-7 remain connected to the same title handler. */
    state.title_stages.motion = 0;
    state.title_stages.flags |= 2;
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 5 && state.title_stages.progress == 1);

    /* Start during stages 2-7 performs the native quick reveal. Stage 8 then
     * selects MAIN_MENU and stage 9 performs the duplicated 0..15 fade. */
    state.game_mode.update_stage = 5;
    state.input.new_input = AOS_KEY_START;
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 8 && state.title_input.idle_timer == 600);
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 9 && state.title_input.leaving);
    state.input.new_input = 0;
    for (unsigned frame = 0; frame < 32; ++frame) {
        aos_game_lifecycle_update(&state);
        assert(state.game_mode.mode == AOS_MODE_TITLE_SCREEN &&
               state.game_mode.update_stage == 9 && !state.title_cleanup_requested);
    }
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.mode == AOS_MODE_MAIN_MENU && state.title_cleanup_requested);
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.mode == AOS_MODE_MAIN_MENU && state.game_mode.update_stage == 1 &&
           state.main_menu.display_control == 0x1F00);

    /* The idle timeout has the native special destination: INTRO_CUTSCENE. */
    aos_game_mode_set(&state.game_mode, AOS_MODE_TITLE_SCREEN);
    state.game_mode.update_stage = 8;
    aos_title_input_enter(&state.title_input);
    state.title_input.idle_timer = 1;
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 9 && state.title_input.timed_out);
    for (unsigned frame = 0; frame < 33; ++frame) aos_game_lifecycle_update(&state);
    assert(state.game_mode.mode == AOS_MODE_INTRO_CUTSCENE && state.title_cleanup_requested);

    puts("Aria native game lifecycle tests passed");
    return 0;
}
