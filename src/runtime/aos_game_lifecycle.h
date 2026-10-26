/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_GAME_LIFECYCLE_H
#define AOS_GAME_LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_game_mode.h"
#include "aos_title_fade.h"
#include "aos_title_stages.h"

/* Host-neutral owner of Aria's native game-mode dispatcher. The SDL runtime
 * consumes the frame requests while the individual controllers retain their
 * source-derived state. */
typedef struct {
    AosGameMode game_mode;
    AosResetMode reset;
    AosStartupLogoMode startup_logo;
    AosTitleFade title_fade;
    AosTitleStages title_stages;
    AosInput input;
    uint32_t progress_flags;
    int save_language_requested;
    bool title_frame_requested;
    bool in_game_frame_requested;
} AosGameLifecycle;

/* Direct gameplay preserves the room, save and debug command-line entry. The
 * native boot entry starts at RESET, as sub_080006CC does. */
void aos_game_lifecycle_init(AosGameLifecycle *state, bool direct_gameplay);
void aos_game_lifecycle_update(AosGameLifecycle *state);

#endif
