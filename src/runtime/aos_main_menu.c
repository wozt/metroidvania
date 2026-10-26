/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_main_menu.h"

enum {
    AOS_MAIN_DISPLAY_ALL_LAYERS = 0x1F00,
    AOS_MAIN_BLEND_BRIGHTNESS_DECREASE_ALL = 0x3FFF,
    AOS_MAIN_BLEND_BG2_OVER_BG3 = 0x0844,
    AOS_MAIN_BLEND_MENU_OVER_BG3 = 0x0853,
    AOS_MAIN_ALPHA_DEFAULT = 0x0A06,
    AOS_MAIN_BRIGHTNESS_MAX = 16,
};

static uint16_t alpha_pack(unsigned first, unsigned second) {
    return (uint16_t)(((first & 0x1Fu) << 8) | (second & 0x1Fu));
}

int32_t aos_main_menu_input(AosMainMenu *menu, const AosInput *input) {
    uint16_t repeated = input ? input->repeated_input : 0;
    if (repeated & (AOS_KEY_START | AOS_KEY_A)) return 1;
    if (repeated & AOS_KEY_B) return -1;
    if (!menu->slot_present_mask) return 0;
    uint8_t previous = menu->selected_slot;
    if (repeated & AOS_KEY_DOWN)
        menu->selected_slot = menu->selected_slot == 2 ? 0 :
                                                       (uint8_t)(menu->selected_slot + 1);
    else if (repeated & AOS_KEY_UP)
        menu->selected_slot = menu->selected_slot == 0 ? 2 :
                                                       (uint8_t)(menu->selected_slot - 1);
    menu->selection_changed = previous != menu->selected_slot;
    return 0;
}

int32_t aos_game_mode_main_menu_update(AosGameMode *mode, AosMainMenu *menu,
                                       const AosInput *input) {
    menu->selection_changed = false;
    menu->selection_confirmed = false;
    menu->selection_cancelled = false;
    menu->music_mute_requested = false;
    menu->selected_slot_init_requested = false;
    menu->cleanup_requested = false;
    switch (mode->update_stage) {
        case 0:
            menu->display_control = AOS_MAIN_DISPLAY_ALL_LAYERS;
            menu->blend_control = AOS_MAIN_BLEND_BRIGHTNESS_DECREASE_ALL;
            menu->brightness = AOS_MAIN_BRIGHTNESS_MAX;
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 1:
            menu->assets_requested = true;
            menu->selected_slot = 0;
            menu->fade_counter = 0;
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 2:
            menu->display_control = AOS_MAIN_DISPLAY_ALL_LAYERS;
            menu->blend_control = AOS_MAIN_BLEND_BG2_OVER_BG3;
            menu->alpha_blend = AOS_MAIN_ALPHA_DEFAULT;
            menu->brightness = 0;
            menu->fade_counter = 0;
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 3: {
            int32_t action = aos_main_menu_input(menu, input);
            if (action < 0) {
                menu->selection_cancelled = true;
                menu->music_mute_requested = true;
                menu->brightness = AOS_MAIN_BRIGHTNESS_MAX;
                menu->fade_counter = 0;
                mode->update_stage = 0x63;
                mode->stage_timer = 0;
            } else if (action > 0) {
                menu->selection_confirmed = true;
                menu->brightness = 0;
                menu->fade_counter = 0;
                mode->update_stage = 0x28;
                mode->stage_timer = 0;
            }
            break;
        }
        case 0x28:
            /* The native case overwrites its initial 0x29 setup and exits at
             * 0x2A in the same call. */
            menu->blend_control = AOS_MAIN_BLEND_MENU_OVER_BG3;
            menu->alpha_blend = alpha_pack(0, 16);
            menu->fade_counter = 0;
            mode->update_stage = 0x2A;
            mode->stage_timer = 0;
            break;
        case 0x2A:
            if ((menu->alpha_blend >> 8) < 16) {
                menu->fade_counter += 2;
                menu->alpha_blend = alpha_pack((unsigned)menu->fade_counter,
                                               (unsigned)(16 - menu->fade_counter));
            } else {
                menu->selected_slot_init_requested = true;
                menu->fade_counter = 16;
                mode->update_stage = 0x2B;
                mode->stage_timer = 0;
            }
            break;
        case 0x2B:
            if (menu->fade_counter != 0) {
                menu->fade_counter -= 2;
                menu->alpha_blend = alpha_pack((unsigned)menu->fade_counter,
                                               (unsigned)(16 - menu->fade_counter));
            } else {
                menu->blend_control = AOS_MAIN_BLEND_BG2_OVER_BG3;
                menu->alpha_blend = AOS_MAIN_ALPHA_DEFAULT;
                menu->selected_slot_active = true;
                mode->update_stage = 6;
                mode->stage_timer = 0;
            }
            break;
        case 6:
            /* The selected slot's native sub-controller is the next slice. */
            menu->selected_slot_active = true;
            break;
        case 0x63:
            if (menu->music_idle) {
                menu->cleanup_requested = true;
                return AOS_MODE_TITLE_SCREEN;
            }
            break;
        default:
            break;
    }
    return AOS_MODE_SAME;
}
