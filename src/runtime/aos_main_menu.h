/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MAIN_MENU_H
#define AOS_MAIN_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_game_mode.h"
#include "aos_input.h"

/* Source-derived controller state for the front half of
 * GameModeMainMenuUpdate. Native SRAM slot contents and rendering remain
 * owned by the host until their later stages are reconstructed. */
typedef struct {
    uint8_t language;
    uint8_t selected_slot;
    uint8_t slot_present_mask;
    uint16_t display_control;
    uint16_t blend_control;
    uint16_t brightness;
    uint16_t alpha_blend;
    int32_t fade_counter;
    bool assets_requested;
    bool selection_changed;
    bool selection_confirmed;
    bool selection_cancelled;
    bool music_mute_requested;
    bool music_idle;
    bool selected_slot_init_requested;
    bool selected_slot_active;
    bool cleanup_requested;
} AosMainMenu;

/* sub_08004928: 1 confirms, -1 cancels, 0 stays. */
int32_t aos_main_menu_input(AosMainMenu *menu, const AosInput *input);
/* GameModeMainMenuUpdate stages 0-3, 0x28, 0x2A, 0x2B, 6 and 0x63. */
int32_t aos_game_mode_main_menu_update(AosGameMode *mode, AosMainMenu *menu,
                                       const AosInput *input);

#endif
