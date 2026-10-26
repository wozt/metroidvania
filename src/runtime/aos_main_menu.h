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
    uint8_t file_cursor;
    uint8_t file_phase;
    uint8_t source_slot;
    uint8_t destination_slot;
    bool assets_requested;
    bool selection_changed;
    bool selection_confirmed;
    bool selection_cancelled;
    bool music_mute_requested;
    bool music_idle;
    bool selected_slot_init_requested;
    bool selected_slot_active;
    bool copy_slots_requested;
    bool delete_rejected;
    uint8_t delete_slots_mask;
    bool cleanup_requested;
} AosMainMenu;

/* sub_08004928: 1 confirms, -1 cancels, 0 stays. */
int32_t aos_main_menu_input(AosMainMenu *menu, const AosInput *input);
/* sub_08007820/sub_08007914: initialize and update the copy controller. */
void aos_main_menu_copy_init(AosMainMenu *menu);
int32_t aos_main_menu_copy_update(AosMainMenu *menu, const AosInput *input);
/* sub_08008290/sub_08007B8C: initialize and update the delete controller. */
void aos_main_menu_delete_init(AosMainMenu *menu);
int32_t aos_main_menu_delete_update(AosMainMenu *menu, const AosInput *input);
/* GameModeMainMenuUpdate stages through the copy/delete return path. */
int32_t aos_game_mode_main_menu_update(AosGameMode *mode, AosMainMenu *menu,
                                       const AosInput *input);

#endif
