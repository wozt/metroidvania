/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_lifecycle.h"

#include <stddef.h>

static int32_t reset_handler(AosGameMode *mode, void *context) {
    AosGameLifecycle *state = context;
    return aos_game_mode_reset_update(mode, &state->reset, &state->input, NULL,
                                      &state->save_language_requested);
}

static int32_t licensed_handler(AosGameMode *mode, void *context) {
    AosGameLifecycle *state = context;
    return aos_game_mode_licensed_by_nintendo_update(mode, &state->startup_logo);
}

static int32_t konami_handler(AosGameMode *mode, void *context) {
    AosGameLifecycle *state = context;
    return aos_game_mode_konami_logo_update(mode, &state->startup_logo, &state->input,
                                            state->progress_flags);
}

/* GameModeTitleScreenUpdate (aos_title_screen); its stage is the mode's. */
static int32_t title_handler(AosGameMode *mode, void *context) {
    AosGameLifecycle *state = context;
    state->title_frame_requested = true;
    state->title.stage = (uint16_t)mode->update_stage;
    int32_t result = aos_title_screen_update(&state->title, state->input.new_input,
                                             state->input.repeated_input);
    mode->update_stage = state->title.stage;
    mode->stage_timer = 0;
    if (result == AOS_MODE_SAME) return result;
    state->title_cleanup_requested = true;
    if (result == AOS_MODE_INTRO_CUTSCENE) aos_intro_cutscene_prepare(&state->intro_cutscene, true);
    return result;
}

static int32_t in_game_handler(AosGameMode *mode, void *context) {
    (void)mode;
    AosGameLifecycle *state = context;
    state->in_game_frame_requested = true;
    return AOS_MODE_SAME;
}

static int32_t main_menu_handler(AosGameMode *mode, void *context) {
    AosGameLifecycle *state = context;
    int32_t result = aos_game_mode_main_menu_update(mode, &state->main_menu,
                                                     &state->input);
    if (result == AOS_MODE_INTRO_CUTSCENE)
        aos_intro_cutscene_prepare(&state->intro_cutscene, false);
    return result;
}

static int32_t intro_handler(AosGameMode *mode, void *context) {
    AosGameLifecycle *state = context;
    state->intro_frame_requested = true;
    return aos_game_mode_intro_cutscene_update(mode, &state->intro_cutscene,
                                                &state->input);
}

static int32_t game_over_handler(AosGameMode *mode, void *context) {
    (void)mode;
    AosGameLifecycle *state = context;
    state->game_over_frame_requested = true;
    return aos_game_over_update(&state->game_over, &state->input);
}

static const AosGameModeHandler handlers[AOS_MODE_COUNT] = {
    [AOS_MODE_GAME_OVER] = game_over_handler,
    [AOS_MODE_KONAMI_LOGO] = konami_handler,
    [AOS_MODE_TITLE_SCREEN] = title_handler,
    [AOS_MODE_MAIN_MENU] = main_menu_handler,
    [AOS_MODE_IN_GAME] = in_game_handler,
    [AOS_MODE_INTRO_CUTSCENE] = intro_handler,
    [AOS_MODE_LICENSED_BY_NINTENDO] = licensed_handler,
    [AOS_MODE_RESET] = reset_handler,
};

void aos_game_lifecycle_init(AosGameLifecycle *state, bool direct_gameplay) {
    *state = (AosGameLifecycle){0};
    state->reset.language = 1;
    state->main_menu.language = 1;
    aos_title_screen_init(&state->title, NULL, NULL, NULL);
    if (direct_gameplay)
        aos_game_mode_set(&state->game_mode, AOS_MODE_IN_GAME);
    else
        aos_game_mode_boot(&state->game_mode);
}

void aos_game_lifecycle_update(AosGameLifecycle *state) {
    state->title_frame_requested = false;
    state->intro_frame_requested = false;
    state->in_game_frame_requested = false;
    state->game_over_frame_requested = false;
    state->title_cleanup_requested = false;
    state->save_language_requested = 0;
    aos_game_mode_update(&state->game_mode, handlers, state);
    if (state->save_language_requested)
        state->main_menu.language = state->reset.language;
}

void aos_game_lifecycle_start_game_over(AosGameLifecycle *state) {
    state->game_over = (AosGameOver){0};
    aos_game_mode_set(&state->game_mode, AOS_MODE_GAME_OVER);
}
