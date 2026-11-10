/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_GAME_LIFECYCLE_H
#define AOS_GAME_LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_game_mode.h"
#include "../intro/aos_game_over.h"
#include "../intro/aos_intro_cutscene.h"
#include "../menus/aos_main_menu.h"
#include "../title/aos_title_screen.h"

/* Host-neutral owner of Aria's native game-mode dispatcher. The SDL runtime
 * consumes the frame requests while the individual controllers retain their
 * source-derived state. */
typedef struct {
    AosGameMode game_mode;
    AosResetMode reset;
    AosStartupLogoMode startup_logo;
    AosIntroCutscene intro_cutscene;
    AosMainMenu main_menu;
    AosTitleScreen title;       /* its graphics and palette rows are the host's */
    AosGameOver game_over;
    AosInput input;
    uint32_t progress_flags;
    int save_language_requested;
    bool title_frame_requested;
    bool intro_frame_requested;
    bool in_game_frame_requested;
    bool game_over_frame_requested;
    bool title_cleanup_requested;
} AosGameLifecycle;

/* Direct gameplay preserves the room, save and debug command-line entry. The
 * native boot entry starts at RESET, as sub_080006CC does. */
void aos_game_lifecycle_init(AosGameLifecycle *state, bool direct_gameplay);
void aos_game_lifecycle_update(AosGameLifecycle *state);
/* Enters GAME_OVER, as sub_0800C8C8 does once the death fade has ended. */
void aos_game_lifecycle_start_game_over(AosGameLifecycle *state);

#endif
