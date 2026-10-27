/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_cutscene.h"

#include <assert.h>
#include <stdio.h>

static int32_t tick(AosGameMode *mode, AosIntroCutscene *intro, uint16_t pressed) {
    AosInput input = {.new_input = pressed};
    return aos_game_mode_intro_cutscene_update(mode, intro, &input);
}

int main(void) {
    AosGameMode mode = {.mode = AOS_MODE_INTRO_CUTSCENE};
    AosIntroCutscene intro;
    aos_intro_cutscene_prepare(&intro, false);

    assert(tick(&mode, &intro, 0) == AOS_MODE_SAME);
    assert(mode.update_stage == 1 && intro.assets_requested &&
           intro.entities_init_requested && intro.entities_frame_requested);
    assert(intro.brightness == 16 && intro.bg_y[1] == 0x1600000 &&
           intro.bg_y[2] == 0x2600000 && intro.bg_y[3] == 0x0C90000);
    assert(intro.entities.entities[AOS_INTRO_ENTITY_MAIN].alive &&
           intro.entities.entities[AOS_INTRO_ENTITY_MAIN].x == 0x780000);
    assert(intro.entities.entities[AOS_INTRO_ENTITY_PANEL_LEFT].x ==
           0xF7F000);
    assert(intro.alpha_blend == 0x060A);

    unsigned frames = 0;
    while (mode.update_stage == 1) {
        assert(tick(&mode, &intro, 0) == AOS_MODE_SAME);
        ++frames;
        assert(frames < 140);
    }
    assert(frames == 129 && mode.update_stage == 2 && intro.brightness == 0);
    assert(tick(&mode, &intro, 0) == AOS_MODE_SAME && mode.update_stage == 3);

    frames = 0;
    while (mode.update_stage == 3) {
        assert(tick(&mode, &intro, 0) == AOS_MODE_SAME);
        ++frames;
        assert(frames < 3300);
    }
    assert(frames == 3220 && mode.update_stage == 4 && intro.bg_y[1] == 0 &&
           intro.bg_y[2] == 0 && intro.bg_y[3] == 0);

    frames = 0;
    while (mode.update_stage == 4) {
        assert(tick(&mode, &intro, 0) == AOS_MODE_SAME);
        ++frames;
    }
    assert(frames == 88 && mode.update_stage == 5);

    frames = 0;
    while (mode.update_stage == 5) {
        assert(tick(&mode, &intro, 0) == AOS_MODE_SAME);
        ++frames;
    }
    assert(frames == 120 && mode.update_stage == 6 &&
           intro.palette_fade_requested && intro.palette_fade_timer == 120);

    frames = 0;
    int32_t result = AOS_MODE_SAME;
    while (result == AOS_MODE_SAME) {
        result = tick(&mode, &intro, 0);
        ++frames;
    }
    assert(frames == 120 && result == AOS_MODE_IN_GAME);
    assert(intro.cleanup_requested && intro.restore_backgrounds_requested &&
           intro.gameplay_setup_requested);
    assert(intro.start_area == 0 && intro.start_room == 0 &&
           intro.start_camera_x == 0x20 && intro.start_camera_y == 0x200 &&
           intro.start_player_x == 0x78 && intro.start_player_y == 0x8D);

    /* A/START before stage 5 skips immediately. The title's idle preview
     * returns to TITLE_SCREEN; a new game enters gameplay. */
    mode = (AosGameMode){.mode = AOS_MODE_INTRO_CUTSCENE};
    aos_intro_cutscene_prepare(&intro, false);
    assert(tick(&mode, &intro, AOS_KEY_START) == AOS_MODE_IN_GAME);
    assert(intro.gameplay_setup_requested);
    assert(intro.entities.graphics_release_requested);
    for (unsigned index = 0; index < AOS_INTRO_ENTITY_COUNT; ++index)
        assert(!intro.entities.entities[index].alive);

    mode = (AosGameMode){.mode = AOS_MODE_INTRO_CUTSCENE, .update_stage = 3};
    aos_intro_cutscene_prepare(&intro, true);
    intro.bg_y[1] = intro.bg_y[2] = intro.bg_y[3] = 0x10000;
    assert(tick(&mode, &intro, AOS_KEY_A) == AOS_MODE_TITLE_SCREEN);
    assert(!intro.gameplay_setup_requested);

    /* Input in stage 5 starts the native 120-frame fade instead of using
     * the immediate skip tail. A preview then returns through Konami. */
    mode = (AosGameMode){.mode = AOS_MODE_INTRO_CUTSCENE, .update_stage = 5};
    aos_intro_cutscene_prepare(&intro, true);
    assert(tick(&mode, &intro, AOS_KEY_A) == AOS_MODE_SAME);
    assert(mode.update_stage == 6 && intro.palette_fade_timer == 120);
    for (unsigned i = 0; i < 119; ++i)
        assert(tick(&mode, &intro, 0) == AOS_MODE_SAME);
    assert(tick(&mode, &intro, 0) == AOS_MODE_KONAMI_LOGO);

    puts("Aria introduction controller tests passed");
    return 0;
}
