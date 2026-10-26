/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_main_menu.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosMainMenu menu = {.language = 1, .slot_present_mask = 1};
    AosGameMode mode = {AOS_MODE_MAIN_MENU, 0, 7};
    AosInput input = {0};

    assert(aos_game_mode_main_menu_update(&mode, &menu, &input) == AOS_MODE_SAME);
    assert(mode.update_stage == 1 && mode.stage_timer == 0);
    assert(menu.display_control == 0x1F00 && menu.blend_control == 0x3FFF &&
           menu.brightness == 16);
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 2 && menu.assets_requested && menu.selected_slot == 0);
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 3 && menu.alpha_blend == 0x0A06 && menu.brightness == 0);

    input.repeated_input = AOS_KEY_UP;
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(menu.selected_slot == 2 && menu.selection_changed);
    input.repeated_input = AOS_KEY_DOWN;
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(menu.selected_slot == 0 && menu.selection_changed);

    input.repeated_input = AOS_KEY_A;
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 0x28 && menu.selection_confirmed);
    input.repeated_input = 0;
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 0x2A && menu.alpha_blend == 0x0010);
    for (unsigned frame = 0; frame < 8; ++frame) {
        aos_game_mode_main_menu_update(&mode, &menu, &input);
        assert(mode.update_stage == 0x2A && menu.fade_counter == (int32_t)(frame + 1) * 2);
    }
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 0x2B && menu.selected_slot_init_requested);
    for (unsigned frame = 0; frame < 8; ++frame)
        aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 0x2B && menu.fade_counter == 0);
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 6 && menu.selected_slot_active &&
           menu.alpha_blend == 0x0A06);

    /* Without any occupied native slot, directions do not move the cursor,
     * but confirm is still accepted as the new-file path. */
    menu = (AosMainMenu){0};
    input = (AosInput){.repeated_input = AOS_KEY_DOWN};
    assert(aos_main_menu_input(&menu, &input) == 0 && menu.selected_slot == 0);
    input.repeated_input = AOS_KEY_START;
    assert(aos_main_menu_input(&menu, &input) == 1);

    menu = (AosMainMenu){0};
    mode = (AosGameMode){AOS_MODE_MAIN_MENU, 3, 4};
    input = (AosInput){.repeated_input = AOS_KEY_B};
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 0x63 && menu.selection_cancelled &&
           menu.music_mute_requested);
    input.repeated_input = 0;
    assert(aos_game_mode_main_menu_update(&mode, &menu, &input) == AOS_MODE_SAME);
    menu.music_idle = true;
    assert(aos_game_mode_main_menu_update(&mode, &menu, &input) == AOS_MODE_TITLE_SCREEN);
    assert(menu.cleanup_requested);

    /* Native COPY uses the even entries as the three visible files and the
     * adjacent odd entries as their companions. */
    menu = (AosMainMenu){.slot_present_mask = (1u << 0) | (1u << 4)};
    aos_main_menu_copy_init(&menu);
    assert(menu.file_cursor == 0 && menu.source_slot == 0 &&
           menu.destination_slot == 2);
    input = (AosInput){.repeated_input = AOS_KEY_RIGHT};
    aos_main_menu_copy_update(&menu, &input);
    assert(menu.file_cursor == 4);
    input.repeated_input = AOS_KEY_A;
    aos_main_menu_copy_update(&menu, &input);
    assert(menu.file_phase == 1 && menu.source_slot == 4 && menu.file_cursor == 2);
    aos_main_menu_copy_update(&menu, &input);
    assert(menu.file_phase == 2 && menu.destination_slot == 2 && menu.file_cursor == 1);
    input.repeated_input = AOS_KEY_LEFT;
    aos_main_menu_copy_update(&menu, &input);
    assert(menu.file_cursor == 0);
    input.repeated_input = AOS_KEY_A;
    aos_main_menu_copy_update(&menu, &input);
    assert(menu.file_phase == 0 && menu.copy_slots_requested &&
           menu.source_slot == 4 && menu.destination_slot == 2 &&
           (menu.slot_present_mask & (1u << 2)));

    menu = (AosMainMenu){.slot_present_mask = (1u << 0) | (1u << 1) | (1u << 4)};
    aos_main_menu_delete_init(&menu);
    input = (AosInput){.repeated_input = AOS_KEY_A};
    aos_main_menu_delete_update(&menu, &input);
    assert(menu.file_phase == 1 && menu.source_slot == 0 && menu.file_cursor == 1);
    input.repeated_input = AOS_KEY_LEFT;
    aos_main_menu_delete_update(&menu, &input);
    assert(menu.file_cursor == 0);
    input.repeated_input = AOS_KEY_A;
    aos_main_menu_delete_update(&menu, &input);
    assert(menu.file_phase == 0 && menu.delete_slots_mask == 3 &&
           !(menu.slot_present_mask & 3));
    input.repeated_input = AOS_KEY_RIGHT;
    aos_main_menu_delete_update(&menu, &input);
    assert(menu.file_cursor == 2);
    input.repeated_input = AOS_KEY_A;
    aos_main_menu_delete_update(&menu, &input);
    assert(menu.delete_rejected && menu.file_phase == 0);

    /* Cancelling either file controller follows 0x46/0x48/0x49 back to the
     * operation selector. */
    menu = (AosMainMenu){.selected_slot = 2, .slot_present_mask = 1,
                         .selected_slot_active = true};
    aos_main_menu_delete_init(&menu);
    mode = (AosGameMode){AOS_MODE_MAIN_MENU, 6, 0};
    input = (AosInput){.repeated_input = AOS_KEY_B};
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 0x46);
    input.repeated_input = 0;
    aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 0x48 && menu.alpha_blend == 0x0010);
    for (unsigned frame = 0; frame < 9; ++frame)
        aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 0x49 && menu.fade_counter == 16);
    for (unsigned frame = 0; frame < 9; ++frame)
        aos_game_mode_main_menu_update(&mode, &menu, &input);
    assert(mode.update_stage == 3 && !menu.selected_slot_active &&
           menu.alpha_blend == 0x0A06);

    puts("Aria native main-menu controller tests passed");
    return 0;
}
