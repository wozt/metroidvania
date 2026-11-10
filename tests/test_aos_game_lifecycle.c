/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/game/aos_game_lifecycle.h"

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

    /* Confirming a Reset language propagates the selected language to the
     * native main-menu assets, rather than the save-request flag itself. */
    aos_game_lifecycle_init(&state, false);
    state.game_mode.update_stage = 5;
    state.reset.language = 3;
    state.reset.brightness = 14;
    aos_game_lifecycle_update(&state);
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.mode == AOS_MODE_LICENSED_BY_NINTENDO &&
           state.save_language_requested == 1 && state.main_menu.language == 3);

    /* Return to the regular English boot route for the title sequence. */
    aos_game_lifecycle_init(&state, false);
    for (unsigned frame = 0; frame < 1000 && state.game_mode.mode != AOS_MODE_TITLE_SCREEN;
         ++frame) {
        state.input.new_input = state.game_mode.mode == AOS_MODE_KONAMI_LOGO &&
                                        state.game_mode.update_stage == 3
                                    ? AOS_KEY_START
                                    : 0;
        aos_game_lifecycle_update(&state);
    }
    assert(state.game_mode.mode == AOS_MODE_TITLE_SCREEN);

    /* The title handler runs GameModeTitleScreenUpdate (aos_title_screen):
     * stage 1 then the 32-frame fade-in of stage 2. */
    aos_game_lifecycle_update(&state);
    assert(state.title_frame_requested && state.game_mode.update_stage == 1);
    aos_game_lifecycle_update(&state);
    assert(state.title_frame_requested && state.game_mode.update_stage == 2 &&
           state.title.song == 27);
    for (unsigned frame = 0; frame < 40 && state.game_mode.update_stage == 2; ++frame)
        aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 3);

    /* Start during stages 2-7 performs the native quick reveal. Stage 8 then
     * selects MAIN_MENU and stage 9 performs the duplicated 0..15 fade. */
    state.input.new_input = AOS_KEY_START;
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 8 && state.title.counter == 600 &&
           state.title.menu_shown);
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 9 && state.title.chosen);
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
    state.input.new_input = 0;
    aos_game_lifecycle_update(&state);
    state.input.new_input = AOS_KEY_START;      /* stage 1 into 2, then the shortcut */
    aos_game_lifecycle_update(&state);
    state.input.new_input = 0;
    assert(state.game_mode.update_stage == 8);
    state.title.counter = 1;
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.update_stage == 9 && state.title.timed_out);
    for (unsigned frame = 0; frame < 33; ++frame) aos_game_lifecycle_update(&state);
    assert(state.game_mode.mode == AOS_MODE_INTRO_CUTSCENE && state.title_cleanup_requested);

    /* The idle introduction is marked as a preview and its early skip goes
     * straight back to the title. */
    state.input.new_input = AOS_KEY_START;
    aos_game_lifecycle_update(&state);
    assert(state.intro_frame_requested && state.game_mode.mode == AOS_MODE_TITLE_SCREEN &&
           state.intro_cutscene.preview_only &&
           !state.intro_cutscene.gameplay_setup_requested);

    /* The same native introduction entered by a new file instead prepares
     * the first room and reaches IN_GAME. */
    aos_intro_cutscene_prepare(&state.intro_cutscene, false);
    aos_game_mode_set(&state.game_mode, AOS_MODE_INTRO_CUTSCENE);
    state.input.new_input = AOS_KEY_A;
    aos_game_lifecycle_update(&state);
    assert(state.game_mode.mode == AOS_MODE_IN_GAME && state.intro_frame_requested &&
           state.intro_cutscene.gameplay_setup_requested &&
           state.intro_cutscene.start_player_x == 0x78 &&
           state.intro_cutscene.start_player_y == 0x8D);
    state.input.new_input = 0;
    aos_game_lifecycle_update(&state);
    assert(state.in_game_frame_requested && !state.intro_frame_requested);

    puts("Aria native game lifecycle tests passed");
    /* The runtime's game over: GAME_OVER (graphics frames requested) until
     * a press, then the white fade, then the Konami logo of the boot path. */
    AosGameLifecycle over;
    aos_game_lifecycle_init(&over, false);
    aos_game_lifecycle_start_game_over(&over);
    assert(over.game_mode.mode == AOS_MODE_GAME_OVER);
    int over_frames = 0;
    while (over.game_mode.mode == AOS_MODE_GAME_OVER && over_frames < 2000) {
        over.input = (AosInput){0};
        if (over.game_over.stage == 3) over.input.new_input = 1;
        aos_game_lifecycle_update(&over);
        if (over.game_mode.mode == AOS_MODE_GAME_OVER) assert(over.game_over_frame_requested);
        ++over_frames;
    }
    assert(over.game_mode.mode == AOS_MODE_KONAMI_LOGO && !over.game_over.sprite_shown);

    /* fusion_aria_title_preview --main-menu (the launcher's entry point):
     * MAIN_MENU set at once runs its first stage as after the title. */
    static AosGameLifecycle direct;
    aos_game_lifecycle_init(&direct, false);
    aos_game_mode_set(&direct.game_mode, AOS_MODE_MAIN_MENU);
    aos_game_lifecycle_update(&direct);
    assert(direct.game_mode.mode == AOS_MODE_MAIN_MENU && direct.game_mode.update_stage == 1 &&
           direct.main_menu.display_control == 0x1F00);
    return 0;
}
