/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_cutscene.h"

enum {
    AOS_INTRO_DISPLAY_ALL_LAYERS = 0x1F00,
    AOS_INTRO_BLEND_BRIGHTNESS_FIRST = 0x00FF,
    AOS_INTRO_BLEND_BRIGHTNESS_SECOND = 0x3FC0,
    AOS_INTRO_BLEND_ALPHA_SECOND = 0x3F40,
    AOS_INTRO_BRIGHTNESS_MAX = 16,
    AOS_INTRO_PALETTE_FADE_FRAMES = 0x78,
};

static uint16_t alpha_pack(unsigned first, unsigned second) {
    return (uint16_t)((first & 0x1Fu) | ((second & 0x1Fu) << 8));
}

static void clear_requests(AosIntroCutscene *intro) {
    intro->hblank_stop_requested = false;
    intro->cleanup_requested = false;
    intro->assets_requested = false;
    intro->entities_init_requested = false;
    intro->entities_frame_requested = false;
    intro->music_start_requested = false;
    intro->music_mute_requested = false;
    intro->palette_fade_requested = false;
    intro->restore_backgrounds_requested = false;
    intro->gameplay_setup_requested = false;
}

void aos_intro_cutscene_prepare(AosIntroCutscene *intro, bool preview_only) {
    *intro = (AosIntroCutscene){
        .preview_only = preview_only,
        /* The native seed source is not traced. The controller still uses
         * the original RandomNumberGenerator recurrence for the flicker. */
        .random_state = 0,
    };
}

static void scroll_up(int32_t *position, int32_t *velocity, int32_t step) {
    if (*position + step < 0) step = -*position;
    if (step < -0x80000) step = -0x80000;
    *velocity = step;
    *position += step;
}

static int32_t finish(AosIntroCutscene *intro, bool immediate_skip) {
    intro->hblank_stop_requested = true;
    intro->cleanup_requested = true;
    intro->music_mute_requested = true;
    intro->restore_backgrounds_requested = true;
    intro->entities_frame_requested = true;
    if (intro->preview_only)
        return immediate_skip ? AOS_MODE_TITLE_SCREEN : AOS_MODE_KONAMI_LOGO;

    /* sub_08008ED0(0): area/room, camera and Soma's first-room position. */
    intro->gameplay_setup_requested = true;
    intro->start_area = 0;
    intro->start_room = 0;
    intro->start_camera_x = 0x20;
    intro->start_camera_y = 0x200;
    intro->start_player_x = 0x78;
    intro->start_player_y = 0x8D;
    return AOS_MODE_IN_GAME;
}

int32_t aos_game_mode_intro_cutscene_update(AosGameMode *mode,
                                             AosIntroCutscene *intro,
                                             const AosInput *input) {
    clear_requests(intro);
    uint16_t pressed = input ? input->new_input : 0;
    switch (mode->update_stage) {
        case 0:
            intro->hblank_stop_requested = true;
            intro->cleanup_requested = true;
            intro->assets_requested = true;
            intro->entities_init_requested = true;
            intro->display_control = AOS_INTRO_DISPLAY_ALL_LAYERS;
            intro->blend_control = AOS_INTRO_BLEND_BRIGHTNESS_FIRST;
            intro->brightness = AOS_INTRO_BRIGHTNESS_MAX;
            intro->alpha_blend = alpha_pack(16, 0);
            intro->bg_y[1] = 0x1600000;
            intro->bg_y[2] = 0x2600000;
            intro->bg_y[3] = 0x0C90000;
            intro->bg_y_velocity[1] = 0;
            intro->bg_y_velocity[2] = 0;
            intro->bg_y_velocity[3] = 0;
            intro->timer = 0;
            mode->update_stage = 1;
            mode->stage_timer = 0;
            break;

        case 1:
            intro->brightness = (uint16_t)(AOS_INTRO_BRIGHTNESS_MAX -
                                            (intro->timer >> 3));
            if (intro->brightness != 0) {
                if (intro->timer == 0) intro->music_start_requested = true;
                ++intro->timer;
            } else {
                intro->blend_control = AOS_INTRO_BLEND_BRIGHTNESS_SECOND;
                intro->timer = 0;
                mode->update_stage = 2;
                mode->stage_timer = 0;
            }
            break;

        case 2:
            mode->update_stage = 3;
            mode->stage_timer = 0;
            break;

        case 3:
            scroll_up(&intro->bg_y[1], &intro->bg_y_velocity[1], -0x1C00);
            scroll_up(&intro->bg_y[2], &intro->bg_y_velocity[2], -0x3200);
            scroll_up(&intro->bg_y[3], &intro->bg_y_velocity[3], -0x1000);
            if (intro->bg_y_velocity[1] == 0) {
                intro->blend_control = AOS_INTRO_BLEND_ALPHA_SECOND;
                intro->timer = 0;
                mode->update_stage = 4;
                mode->stage_timer = 0;
            }
            break;

        case 4:
            ++intro->timer;
            if (!(intro->timer & 7u)) {
                unsigned first = (intro->timer & 2u) +
                                 (16u - (intro->timer >> 4));
                intro->alpha_blend = alpha_pack(first, 16u - first);
                if ((intro->timer >> 3) > 10) {
                    intro->timer = 0;
                    mode->update_stage = 5;
                    mode->stage_timer = 0;
                }
            }
            break;

        case 5:
            ++intro->timer;
            intro->random_state = (intro->random_state >> 8) * 0x3243F6ADu +
                                  0x1B0CB175u;
            {
                unsigned first = (intro->timer & 1u) +
                                 ((intro->random_state & 1u) * 2u) + 13u;
                intro->alpha_blend = alpha_pack(first, 16u - first);
            }
            if (intro->timer > 0x77 || (pressed & (AOS_KEY_A | AOS_KEY_START))) {
                intro->palette_fade_requested = true;
                intro->music_mute_requested = true;
                intro->palette_fade_timer = AOS_INTRO_PALETTE_FADE_FRAMES;
                mode->update_stage = 6;
                mode->stage_timer = 0;
            }
            break;

        case 6:
            if (intro->palette_fade_timer != 0) --intro->palette_fade_timer;
            if (intro->palette_fade_timer == 0)
                return finish(intro, false);
            break;

        default:
            break;
    }

    if ((pressed & (AOS_KEY_A | AOS_KEY_START)) && mode->update_stage < 6)
        return finish(intro, true);

    /* sub_08000B64 runs after every native intro controller frame. */
    intro->entities_frame_requested = true;
    return AOS_MODE_SAME;
}
