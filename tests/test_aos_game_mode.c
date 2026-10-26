/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_mode.h"
#include <assert.h>
#include <stdio.h>

static int calls;

static int32_t stay(AosGameMode *state, void *context) {
    (void)state;
    ++*(int *)context;
    return AOS_MODE_SAME;
}

static int32_t advance(AosGameMode *state, void *context) {
    (void)state;
    (void)context;
    ++calls;
    return AOS_MODE_NEXT;
}

static int32_t to_reset(AosGameMode *state, void *context) {
    (void)context;
    state->stage_timer = 5;
    return AOS_MODE_RESET;
}

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
    /* The native enum: mode numbers the switch of GameModeUpdate tests. */
    assert(AOS_MODE_IN_GAME == 4 && AOS_MODE_RESET == 13 && AOS_MODE_UNK_F == 15 &&
           AOS_MODE_DEBUG_4531 == 20 && AOS_MODE_COUNT == 21);

    /* GameModeUpdate runs exactly the current mode's handler. */
    AosGameModeHandler handlers[AOS_MODE_COUNT] = {0};
    int stays = 0;
    handlers[AOS_MODE_TITLE_SCREEN] = stay;
    handlers[AOS_MODE_KONAMI_LOGO] = advance;
    handlers[AOS_MODE_GAME_OVER] = to_reset;
    AosGameMode game = {AOS_MODE_TITLE_SCREEN, 2, 7};
    aos_game_mode_update(&game, handlers, &stays);
    assert(stays == 1 && game.mode == AOS_MODE_TITLE_SCREEN && game.update_stage == 2 &&
           game.stage_timer == 7);
    game = (AosGameMode){AOS_MODE_KONAMI_LOGO, 4, 1};
    aos_game_mode_update(&game, handlers, &stays);
    assert(calls == 1 && game.mode == AOS_MODE_TITLE_SCREEN && !game.update_stage &&
           !game.stage_timer);
    /* A handler's own stage writes are reset by an explicit destination. */
    game = (AosGameMode){AOS_MODE_GAME_OVER, 3, 0};
    aos_game_mode_update(&game, handlers, &stays);
    assert(game.mode == AOS_MODE_RESET && !game.update_stage && !game.stage_timer);
    /* GAME_MODE_UNK_F (an empty case), an unported entry and an out-of-range
     * mode keep the state. */
    for (int32_t mode = 0; mode < 3; ++mode) {
        int32_t value = mode == 0 ? AOS_MODE_UNK_F : mode == 1 ? AOS_MODE_CREDITS : 99;
        game = (AosGameMode){value, 6, 9};
        aos_game_mode_update(&game, handlers, &stays);
        assert(game.mode == value && game.update_stage == 6 && game.stage_timer == 9);
    }

    /* sub_080006CC ends in the reset mode with a clean stage. */
    game = (AosGameMode){AOS_MODE_IN_GAME, 8, 8};
    aos_game_mode_boot(&game);
    assert(game.mode == AOS_MODE_RESET && !game.update_stage && !game.stage_timer);

    /* GameModeResetUpdate stage 0 forces English (1) and saves only a change. */
    for (unsigned value = 0; value < 5; ++value) {
        uint8_t language = (uint8_t)value;
        int save = -1;
        assert(aos_game_mode_reset_stage0(&language, &save) == AOS_MODE_LICENSED_BY_NINTENDO);
        assert(language == 1 && save == (value != 1));
    }
    puts("Aria native game mode dispatch tests passed");
    return 0;
}
