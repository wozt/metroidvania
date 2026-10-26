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

    /* The title handler owns the existing fade stages. Stage 3 deliberately
     * remains the current native-parity boundary. */
    aos_game_lifecycle_update(&state);
    assert(state.title_frame_requested && state.game_mode.update_stage == 1);
    aos_game_lifecycle_update(&state);
    assert(state.title_frame_requested && state.game_mode.update_stage == 2);
    for (unsigned frame = 0; frame < 40 && state.game_mode.update_stage == 2; ++frame)
        aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 3);

    /* Already-ported stages 4-7 remain connected to the same title handler. */
    state.game_mode.update_stage = 4;
    state.title_stages = (AosTitleStages){.motion = 0, .flags = 2};
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 5 && state.title_stages.progress == 1);

    puts("Aria native game lifecycle tests passed");
    return 0;
}
