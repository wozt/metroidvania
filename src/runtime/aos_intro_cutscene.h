/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_INTRO_CUTSCENE_H
#define AOS_INTRO_CUTSCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_game_mode.h"
#include "aos_input.h"

/* Display-independent state for GameModeIntroCutsceneUpdate. Graphics,
 * entities, palette fades and songs are reported as host requests. */
typedef struct {
    uint16_t display_control;
    uint16_t blend_control;
    uint16_t brightness;
    uint16_t alpha_blend;
    uint32_t timer;
    uint16_t palette_fade_timer;
    int32_t bg_y[4];
    int32_t bg_y_velocity[4];
    uint32_t random_state;
    bool preview_only;
    bool hblank_stop_requested;
    bool cleanup_requested;
    bool assets_requested;
    bool entities_init_requested;
    bool entities_frame_requested;
    bool music_start_requested;
    bool music_mute_requested;
    bool palette_fade_requested;
    bool restore_backgrounds_requested;
    bool gameplay_setup_requested;
    uint8_t start_area;
    uint16_t start_room;
    int16_t start_camera_x;
    int16_t start_camera_y;
    int16_t start_player_x;
    int16_t start_player_y;
} AosIntroCutscene;

/* preview_only is the title's idle demonstration. A named new file passes
 * false and continues into gameplay when the introduction finishes. */
void aos_intro_cutscene_prepare(AosIntroCutscene *intro, bool preview_only);

/* GameModeIntroCutsceneUpdate and its sub_08008ED0 gameplay handoff. */
int32_t aos_game_mode_intro_cutscene_update(AosGameMode *mode,
                                             AosIntroCutscene *intro,
                                             const AosInput *input);

#endif
