/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_cutscene.h"

#include <string.h>

enum {
    AOS_INTRO_DISPLAY_ALL_LAYERS = 0x1F00,
    AOS_INTRO_BLEND_BRIGHTNESS_FIRST = 0x00FF,
    AOS_INTRO_BLEND_BRIGHTNESS_SECOND = 0x3FC0,
    AOS_INTRO_BLEND_ALPHA_SECOND = 0x3F40,
    AOS_INTRO_BRIGHTNESS_MAX = 16,
    AOS_INTRO_PALETTE_FADE_FRAMES = 0x78,
};

/* BLDALPHA layout: EVA (first-target coefficient) in bits 0-4, EVB
 * (second-target) in bits 8-12. C_16_2_8(high, low) passes the EVB value
 * first, so native call sites swap argument order here. */
static uint16_t alpha_pack(unsigned eva, unsigned evb) {
    return (uint16_t)((eva & 0x1Fu) | ((evb & 0x1Fu) << 8));
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
        /* sub_08004160 reseeds gEwramData->randomNumber to 0x1967 on the
         * new-game path; no intro entity draws from it before stage 5,
         * so the flicker sequence matches the native recurrence. */
        .random_state = 0x1967,
    };
}

size_t aos_intro_cutscene_audio_requests(
    const AosIntroCutscene *intro,
    uint16_t requests[AOS_INTRO_AUDIO_CAPACITY]) {
    if (!intro || !requests) return 0;
    size_t count = 0;
    if (intro->music_start_requested)
        requests[count++] = AOS_INTRO_SONG_PROLOGUE;
    if (intro->entities.affine_sound_requested)
        requests[count++] = AOS_INTRO_SOUND_AFFINE_RISE;
    if (intro->music_mute_requested)
        requests[count++] = AOS_INTRO_AUDIO_MUTE;
    return count;
}

/* sub_0803F8A8's pBgMetadata->unk_0/unk_1 scroll gates for the three
 * intro backgrounds, read from the ROM metadata records the native
 * stage 0 binds: BG1 0x080E9464, BG2 0x080E9244, BG3 0x080E9354.
 * unk_0 = 1 pins the x axis (native also clears bgOfs[1].hOfs in
 * stage 3); unk_1 = 2/3/2 leaves the y axis live. */
enum {
    AOS_INTRO_SCROLL_META_X = 1,
    AOS_INTRO_SCROLL_META_Y_BG1 = 2,
    AOS_INTRO_SCROLL_META_Y_BG2 = 3,
    AOS_INTRO_SCROLL_META_Y_BG3 = 2,
};

/* sub_0803F8A8(bg, pBgMetadata, 0, yInt): seeds bgInfo[bg] with the
 * intro background's scroll gates and 16.16 start position. */
static void intro_scroll_bind(AosIntroCutscene *intro) {
    memset(&intro->scroll, 0, sizeof intro->scroll);
    aos_scroll_bind(&intro->scroll, 1, AOS_INTRO_SCROLL_META_X,
                    AOS_INTRO_SCROLL_META_Y_BG1, 0, 0, 0, 0, 0x1600000);
    aos_scroll_bind(&intro->scroll, 2, AOS_INTRO_SCROLL_META_X,
                    AOS_INTRO_SCROLL_META_Y_BG2, 0, 0, 0, 0, 0x2600000);
    aos_scroll_bind(&intro->scroll, 3, AOS_INTRO_SCROLL_META_X,
                    AOS_INTRO_SCROLL_META_Y_BG3, 0, 0, 0, 0, 0x0C90000);
}

/* Mirror the scroll layer's y halves into the cutscene's public
 * bg_y[]/bg_y_velocity[] view after the stage-3 requests. */
static void intro_scroll_sync(AosIntroCutscene *intro) {
    for (int bg = 1; bg < 4; ++bg) {
        intro->bg_y[bg] = intro->scroll.bg[bg].y_pos;
        intro->bg_y_velocity[bg] = intro->scroll.bg[bg].y_vel;
    }
}

static int32_t finish(AosIntroCutscene *intro, bool immediate_skip) {
    intro->hblank_stop_requested = true;
    intro->cleanup_requested = true;
    intro->music_mute_requested = true;
    intro->restore_backgrounds_requested = true;
    intro->entities_frame_requested = true;
    aos_intro_entities_clear(&intro->entities);
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
            intro_scroll_bind(intro);
            intro->timer = 0;
            aos_intro_entities_init(&intro->entities);
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
            /* GameModeIntroCutsceneUpdate stage 3: sub_0803FC6C on
             * BG1-3 with the fixed negative-y steps, then wait on
             * bgInfo[1].yVel == 0. The tile-row rebuilds the native
             * requests queue stay unported — the host draws the
             * pre-rendered intro bitmaps, not BG tilemap memory. */
            aos_scroll_transition_request(&intro->scroll, 1, 0, -0x1C00, NULL);
            aos_scroll_transition_request(&intro->scroll, 2, 0, -0x3200, NULL);
            aos_scroll_transition_request(&intro->scroll, 3, 0, -0x1000, NULL);
            intro_scroll_sync(intro);
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
                unsigned evb = (intro->timer & 2u) +
                               (16u - (intro->timer >> 4));
                intro->alpha_blend = alpha_pack(16u - evb, evb);
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
                unsigned evb = (intro->timer & 1u) +
                               ((intro->random_state & 1u) * 2u) + 13u;
                intro->alpha_blend = alpha_pack(16u - evb, evb);
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
            /* sub_0803D18C/sub_0803CDF0 interpolate the palette to black
             * over 0x78 frames; brightness models the same fade-out. */
            intro->brightness = (uint16_t)(
                (AOS_INTRO_PALETTE_FADE_FRAMES - intro->palette_fade_timer) *
                AOS_INTRO_BRIGHTNESS_MAX / AOS_INTRO_PALETTE_FADE_FRAMES);
            if (intro->palette_fade_timer == 0)
                return finish(intro, false);
            break;

        default:
            break;
    }

    if ((pressed & (AOS_KEY_A | AOS_KEY_START)) && mode->update_stage < 6)
        return finish(intro, true);

    /* sub_08000B64 runs after every native intro controller frame. */
    aos_intro_entities_update(&intro->entities, mode->update_stage,
                              intro->bg_y[1], intro->bg_y_velocity[1],
                              intro->bg_y[3]);
    if (intro->entities.alpha_override_valid)
        intro->alpha_blend = intro->entities.alpha_override;
    intro->entities_frame_requested = true;
    return AOS_MODE_SAME;
}
