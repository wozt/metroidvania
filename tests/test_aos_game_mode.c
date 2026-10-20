/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_mode.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosGameMode state = {3, 9, 12};
    aos_game_mode_accept(&state, AOS_MODE_SAME);
    assert(state.mode == 3 && state.update_stage == 9 && state.stage_timer == 12);
    aos_game_mode_accept(&state, AOS_MODE_NEXT);
    assert(state.mode == 4 && state.update_stage == 0 && state.stage_timer == 0);
    state.update_stage = 7;
    state.stage_timer = 8;
    aos_game_mode_accept(&state, 13);
    assert(state.mode == 13 && state.update_stage == 0 && state.stage_timer == 0);
    state.update_stage = 2;
    state.stage_timer = 3;
    aos_game_mode_set(&state, 0);
    assert(state.mode == 0 && state.update_stage == 0 && state.stage_timer == 0);

    /* Sentinels are never confused with regular native mode numbers. */
    const int32_t destinations[5] = {2, 10, 0, 5, 11};
    for (unsigned i = 0; i < 5; ++i)
        assert(aos_title_destination(i, destinations) == destinations[i]);
    assert(aos_title_destination(5, destinations) == AOS_MODE_SAME);
    assert(aos_title_destination(999, destinations) == AOS_MODE_SAME);
    puts("Aria native game mode dispatch tests passed");
    return 0;
}
