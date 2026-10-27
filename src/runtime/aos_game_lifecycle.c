/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_lifecycle.h"

#include <stddef.h>

static const int32_t title_destinations[5] = {
    AOS_MODE_MAIN_MENU, AOS_MODE_SOUL_TRADE_MENU, AOS_MODE_KONAMI_LOGO,
    AOS_MODE_BOSS_RUSH_MENU, AOS_MODE_SOUND_TEST_MENU};

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
            state->title_motion = (AosTitleMotion){
                .motion = 0x200000, .x = 0x980000, .y = 0x700000};
            mode->update_stage = state->title_fade.stage;
            mode->stage_timer = state->title_fade.stage_timer;
            break;
        case 2:
            aos_title_fade_tick(&state->title_fade);
            mode->update_stage = state->title_fade.stage;
            mode->stage_timer = state->title_fade.stage_timer;
            break;
        case 3:
            aos_title_motion_stage3(&state->title_motion);
            if (state->title_motion.stage_four) {
                state->title_stages.motion = state->title_motion.motion;
                state->title_stages.x = state->title_motion.x;
                state->title_stages.y = state->title_motion.y;
                state->title_stages.flags = state->title_motion.flags;
                state->title_stages.affine_scale = state->title_motion.affine_scale;
                mode->update_stage = 4;
                mode->stage_timer = 0;
            }
            break;
        case 4:
        case 5:
        case 6:
        case 7:
            sync_title_stage(mode, &state->title_stages);
            if (mode->update_stage == 8) aos_title_input_enter(&state->title_input);
            break;
        case 8: {
            bool logo_selected = aos_title_destination(state->title_destination_index,
                                                       title_destinations) == AOS_MODE_KONAMI_LOGO;
            AosTitleAction action = aos_title_input_tick(
                &state->title_input, state->input.new_input, state->input.repeated_input,
                logo_selected, state->title_submenu_open, state->title_exit_blocked);
            state->title_submenu_requested = action == AOS_TITLE_SUBMENU;
            state->title_back_requested = action == AOS_TITLE_BACK;
            if (action == AOS_TITLE_EXIT) {
                mode->update_stage = 9;
                mode->stage_timer = 0;
            }
            break;
        }
        case 9:
            if (aos_title_exit_tick(&state->title_input)) {
                state->title_cleanup_requested = true;
                if (state->title_input.timed_out) {
                    aos_intro_cutscene_prepare(&state->intro_cutscene, true);
                    return AOS_MODE_INTRO_CUTSCENE;
                }
                return aos_title_destination(state->title_destination_index,
                                             title_destinations);
            }
            break;
        default:
            break;
    }
    /* Native Start/A shortcut from animation stages 2-7. */
    if (mode->update_stage >= 2 && mode->update_stage <= 7 &&
        (state->input.new_input & (AOS_KEY_START | AOS_KEY_A))) {
        state->title_motion.motion = 0;
        state->title_stages.motion = 0;
        state->title_stages.progress = 0x40;
        state->title_stages.alpha = 0;
        state->title_stages.show_bg0 = true;
        aos_title_input_enter(&state->title_input);
        mode->update_stage = 8;
        mode->stage_timer = 0;
    }
    return AOS_MODE_SAME;
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

static const AosGameModeHandler handlers[AOS_MODE_COUNT] = {
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
    if (direct_gameplay)
        aos_game_mode_set(&state->game_mode, AOS_MODE_IN_GAME);
    else
        aos_game_mode_boot(&state->game_mode);
}

void aos_game_lifecycle_update(AosGameLifecycle *state) {
    state->title_frame_requested = false;
    state->intro_frame_requested = false;
    state->in_game_frame_requested = false;
    state->title_submenu_requested = false;
    state->title_back_requested = false;
    state->title_cleanup_requested = false;
    state->save_language_requested = 0;
    aos_game_mode_update(&state->game_mode, handlers, state);
}
