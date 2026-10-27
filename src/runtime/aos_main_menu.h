/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MAIN_MENU_H
#define AOS_MAIN_MENU_H

#include <stdbool.h>
#include <stddef.h>
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

enum { AOS_MAIN_MENU_NO_PROMPT = 0xFFFF };

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
/* Native localized prompt selected by sub_08004928, sub_08007204 and the
 * copy/delete controllers. */
uint16_t aos_main_menu_prompt_id(const AosMainMenu *menu);

/* One localized entry the controllers print, at its context-A text cell. */
typedef struct {
    uint16_t id;
    uint8_t column, row;
} AosMainMenuText;

enum { AOS_MAIN_MENU_TEXT_MAX = 3 };

/* The entries the menu's text window shows: the prompt at (0, 1) and, while
 * COPY or DELETE asks for confirmation, YES (0x34B) and NO (0x34D) on row 2
 * at columns 9/0x14 (copy, sub_08007914) or 0xA/0x15 (delete, sub_08007B8C).
 * Returns the number of entries. */
size_t aos_main_menu_text_layout(const AosMainMenu *menu,
                                 AosMainMenuText texts[AOS_MAIN_MENU_TEXT_MAX]);
/* GameModeMainMenuUpdate through direct load/intro and name-entry handoff. */
int32_t aos_game_mode_main_menu_update(AosGameMode *mode, AosMainMenu *menu,
                                       const AosInput *input);

#endif
