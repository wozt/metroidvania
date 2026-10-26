/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MAIN_MENU_H
#define AOS_MAIN_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_game_mode.h"
#include "aos_input.h"

/* Source-derived GameModeMainMenuUpdate controller state. Slot masks retain
 * the native file/quicksave topology; SRAM contents and rendering stay owned
 * by the host until their adapters are reconstructed. */
typedef struct {
    uint8_t language;
    uint8_t selected_slot;
    uint8_t slot_present_mask;
    uint8_t special_slot_mask;
    uint8_t slot_mode_choice_mask;
    uint8_t slot_intro_mask;
    uint16_t display_control;
    uint16_t blend_control;
    uint16_t brightness;
    uint16_t alpha_blend;
    int32_t fade_counter;
    uint8_t file_cursor;
    uint8_t file_phase;
    uint8_t source_slot;
    uint8_t destination_slot;
    uint8_t requested_slot;
    uint8_t confirm_choice;
    uint8_t prompt_timer;
    uint8_t name_cursor;
    uint8_t name_position;
    uint8_t name_buffer[8];
    uint8_t committed_name[8];
    uint8_t current_character;
    uint8_t name_code;
    uint32_t name_unlock_flags;
    bool assets_requested;
    bool selection_changed;
    bool selection_confirmed;
    bool selection_cancelled;
    bool music_mute_requested;
    bool music_idle;
    bool selected_slot_init_requested;
    bool selected_slot_active;
    bool confirm_active;
    bool alternate_mode;
    bool prompt_alternate;
    bool prompt_changed;
    bool load_slot_requested;
    bool new_game_flow_requested;
    bool load_game_requested;
    bool intro_requested;
    bool room_setup_requested;
    bool name_entry_init_requested;
    bool name_entry_active;
    bool name_codes_enabled;
    bool name_changed;
    bool name_defaulted;
    bool name_committed;
    bool save_slot_requested;
    bool copy_slots_requested;
    bool delete_rejected;
    uint8_t delete_slots_mask;
    bool cleanup_requested;
} AosMainMenu;

/* sub_08004928: 1 confirms, -1 cancels, 0 stays. */
int32_t aos_main_menu_input(AosMainMenu *menu, const AosInput *input);
/* sub_0800744C/sub_080074C0: initialize and update PLAY/CREATE/LOAD. */
void aos_main_menu_play_init(AosMainMenu *menu);
int32_t aos_main_menu_play_update(AosMainMenu *menu, const AosInput *input);
/* sub_08008324/sub_080083C8: initialize and update native name entry. */
void aos_main_menu_name_init(AosMainMenu *menu);
int32_t aos_main_menu_name_update(AosMainMenu *menu, const AosInput *input);
/* sub_08007820/sub_08007914: initialize and update the copy controller. */
void aos_main_menu_copy_init(AosMainMenu *menu);
int32_t aos_main_menu_copy_update(AosMainMenu *menu, const AosInput *input);
/* sub_08008290/sub_08007B8C: initialize and update the delete controller. */
void aos_main_menu_delete_init(AosMainMenu *menu);
int32_t aos_main_menu_delete_update(AosMainMenu *menu, const AosInput *input);
/* GameModeMainMenuUpdate through direct load/intro and name-entry handoff. */
int32_t aos_game_mode_main_menu_update(AosGameMode *mode, AosMainMenu *menu,
                                       const AosInput *input);

#endif
