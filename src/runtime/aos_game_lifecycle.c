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

static void sync_title_stage(AosGameMode *mode, AosTitleStages *title) {
    title->stage = (uint16_t)mode->update_stage;
    title->stage_timer = (uint16_t)mode->stage_timer;
    switch (mode->update_stage) {
        case 4: aos_title_stage4(title); break;
        case 5: aos_title_stage5(title); break;
        case 6: aos_title_stage6(title); break;
        case 7: aos_title_stage7(title); break;
        default: break;
    }
    mode->update_stage = title->stage;
    mode->stage_timer = title->stage_timer;
}

static int32_t title_handler(AosGameMode *mode, void *context) {
    AosGameLifecycle *state = context;
    state->title_frame_requested = true;
    switch (mode->update_stage) {
        case 0:
            aos_title_fade_init(&state->title_fade);
            mode->update_stage = state->title_fade.stage;
            mode->stage_timer = state->title_fade.stage_timer;
            break;
        case 1:
            aos_title_fade_begin(&state->title_fade);
            mode->update_stage = state->title_fade.stage;
            mode->stage_timer = state->title_fade.stage_timer;
            break;
        case 2:
            aos_title_fade_tick(&state->title_fade);
            mode->update_stage = state->title_fade.stage;
            mode->stage_timer = state->title_fade.stage_timer;
            break;
        case 4:
        case 5:
        case 6:
        case 7:
            sync_title_stage(mode, &state->title_stages);
            break;
        default:
            /* Native title stage 3 and stage 8 selection are still partial. */
            break;
    }
    return AOS_MODE_SAME;
}

static int32_t in_game_handler(AosGameMode *mode, void *context) {
    (void)mode;
    AosGameLifecycle *state = context;
    state->in_game_frame_requested = true;
    return AOS_MODE_SAME;
}

static const AosGameModeHandler handlers[AOS_MODE_COUNT] = {
    [AOS_MODE_KONAMI_LOGO] = konami_handler,
    [AOS_MODE_TITLE_SCREEN] = title_handler,
    [AOS_MODE_IN_GAME] = in_game_handler,
    [AOS_MODE_LICENSED_BY_NINTENDO] = licensed_handler,
    [AOS_MODE_RESET] = reset_handler,
};

void aos_game_lifecycle_init(AosGameLifecycle *state, bool direct_gameplay) {
    *state = (AosGameLifecycle){0};
    state->reset.language = 1;
    if (direct_gameplay)
        aos_game_mode_set(&state->game_mode, AOS_MODE_IN_GAME);
    else
        aos_game_mode_boot(&state->game_mode);
}

void aos_game_lifecycle_update(AosGameLifecycle *state) {
    state->title_frame_requested = false;
    state->in_game_frame_requested = false;
    state->save_language_requested = 0;
    aos_game_mode_update(&state->game_mode, handlers, state);
}
