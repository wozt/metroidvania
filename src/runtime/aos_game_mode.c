/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_mode.h"
#include "aos_title_fade.h"

enum {
    AOS_DCNT_BG0 = 1u << 8,
    AOS_BG0_HIGH_PRIORITY = 28u << 8,
    AOS_BG0_LANGUAGE_MENU = (28u << 8) | 2u,
    AOS_BLEND_SCREEN_BRIGHTNESS_INCREASE = 0x3fu | (1u << 7),
    AOS_BLEND_SCREEN_BRIGHTNESS_DECREASE = 0x3fu | (3u << 6),
    AOS_BRIGHTNESS_MAX = 16,
};

/* SetGameMode, native src/main.c. */
void aos_game_mode_set(AosGameMode *state, int32_t mode) {
    state->mode = mode;
    state->update_stage = 0;
    state->stage_timer = 0;
}

/* IncrementGameMode, native src/main.c (unused by original callers). */
void aos_game_mode_increment(AosGameMode *state) {
    aos_game_mode_set(state, state->mode + 1);
}

/* The result-handling tail of GameModeUpdate, src/main.c. This does not
 * misrepresent the individual mode handlers as having been ported. */
void aos_game_mode_accept(AosGameMode *state, int32_t update_result) {
    if (update_result == AOS_MODE_NEXT)
        aos_game_mode_increment(state);
    else if (update_result != AOS_MODE_SAME)
        aos_game_mode_set(state, update_result);
}

/* GameModeUpdate, native src/main.c. */
void aos_game_mode_update(AosGameMode *state, const AosGameModeHandler handlers[AOS_MODE_COUNT],
                          void *context) {
    int32_t result = AOS_MODE_SAME;
    if (state->mode >= 0 && state->mode < AOS_MODE_COUNT && handlers[state->mode])
        result = handlers[state->mode](state, context);
    aos_game_mode_accept(state, result);
}

/* The last two calls of sub_080006CC, native src/main.c. */
void aos_game_mode_boot(AosGameMode *state) {
    aos_game_mode_set(state, AOS_MODE_KONAMI_LOGO);
    aos_game_mode_set(state, AOS_MODE_RESET);
}

/* GameModeResetUpdate case 0, native src/code_08002454.c. */
int32_t aos_game_mode_reset_stage0(uint8_t *language, int *save_language) {
    *save_language = *language != 1;
    if (*save_language) *language = 1;
    return AOS_MODE_LICENSED_BY_NINTENDO;
}

static void reset_menu_draw(AosResetMode *reset, const AosResetMenuHooks *hooks) {
    static const uint8_t panel_rows[3] = {5, 7, 9};
    static const uint16_t panel_tiles[3] = {0xF020, 0xF040, 0xF060};
    static const uint16_t text_ids[3] = {0xB4C, 0xB4D, 0xB4E};
    if (hooks) {
        for (unsigned index = 0; index < 3; ++index)
            if (hooks->draw_panel)
                hooks->draw_panel(hooks->context, 10, panel_rows[index], panel_tiles[index], 10);
        if (hooks->prepare_menu) hooks->prepare_menu(hooks->context);
        for (unsigned index = 0; index < 3; ++index) {
            if (hooks->set_text_cursor)
                hooks->set_text_cursor(hooks->context, 2, (uint8_t)(index + 1));
            if (hooks->draw_text) hooks->draw_text(hooks->context, text_ids[index], 0);
        }
    }
    if (reset->language < 1 || reset->language > 3) reset->language = 1;
    if (hooks && hooks->set_text_cursor)
        hooks->set_text_cursor(hooks->context, 0, reset->language);
    if (hooks && hooks->write_cursor_tile) hooks->write_cursor_tile(hooks->context, 0x3E);
    reset->menu_prepared = true;
    reset->cursor_to = reset->language;
}

static void reset_menu_input(AosGameMode *mode, AosResetMode *reset, const AosInput *input,
                             const AosResetMenuHooks *hooks) {
    uint8_t previous = reset->language;
    uint16_t pressed = input ? input->new_input : 0;
    if (pressed & AOS_KEY_A) {
        reset->blend_control = AOS_BLEND_SCREEN_BRIGHTNESS_INCREASE;
        reset->selection_confirmed = true;
        mode->update_stage = 5;
        mode->stage_timer = 0;
    } else if (pressed & AOS_KEY_B) {
        reset->language = 1;
    } else if (pressed & AOS_KEY_UP) {
        reset->language = reset->language == 1 ? 3 : (uint8_t)(reset->language - 1);
    } else if (pressed & AOS_KEY_DOWN) {
        reset->language = reset->language == 3 ? 1 : (uint8_t)(reset->language + 1);
    }
    if (previous == reset->language) return;
    reset->cursor_changed = true;
    reset->cursor_from = previous;
    reset->cursor_to = reset->language;
    if (hooks && hooks->set_text_cursor) hooks->set_text_cursor(hooks->context, 0, previous);
    if (hooks && hooks->write_cursor_tile) {
        hooks->write_cursor_tile(hooks->context, 0x20);
        hooks->write_cursor_tile(hooks->context, 0x20);
    }
    if (hooks && hooks->set_text_cursor)
        hooks->set_text_cursor(hooks->context, 0, reset->language);
    if (hooks && hooks->write_cursor_tile) hooks->write_cursor_tile(hooks->context, 0x3E);
}

int32_t aos_game_mode_reset_update(AosGameMode *mode, AosResetMode *reset,
                                   const AosInput *input, const AosResetMenuHooks *hooks,
                                   int *save_language) {
    reset->cursor_changed = false;
    reset->selection_confirmed = false;
    if (save_language) *save_language = 0;
    switch (mode->update_stage) {
        case 0:
            return aos_game_mode_reset_stage0(&reset->language, save_language);
        case 1:
            reset->display_control = AOS_DCNT_BG0;
            reset->bg0_control = AOS_BG0_LANGUAGE_MENU;
            reset->blend_control = AOS_BLEND_SCREEN_BRIGHTNESS_INCREASE;
            reset->brightness = AOS_BRIGHTNESS_MAX;
            reset->bg_map_clear_requested = true;
            reset->bg_tiles_clear_requested = true;
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 2:
            reset_menu_draw(reset, hooks);
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 3:
            if (aos_title_brightness_lower(&reset->brightness, 2)) {
                reset->duration_timer = 2 * 60;
                ++mode->update_stage;
                mode->stage_timer = 0;
            }
            break;
        case 4:
            reset_menu_input(mode, reset, input, hooks);
            break;
        case 5:
            if (aos_title_brightness_raise(&reset->brightness, 2)) {
                if (save_language) *save_language = 1;
                return AOS_MODE_LICENSED_BY_NINTENDO;
            }
            break;
        default:
            break;
    }
    return AOS_MODE_SAME;
}

int32_t aos_game_mode_licensed_by_nintendo_update(AosGameMode *mode,
                                                   AosStartupLogoMode *logo) {
    switch (mode->update_stage) {
        case 0:
            logo->display_control = AOS_DCNT_BG0;
            logo->bg0_control = AOS_BG0_LANGUAGE_MENU;
            logo->blend_control = AOS_BLEND_SCREEN_BRIGHTNESS_INCREASE;
            logo->brightness = AOS_BRIGHTNESS_MAX;
            logo->asset_request = AOS_STARTUP_ASSETS_NONE;
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 1:
            logo->asset_request = AOS_STARTUP_ASSETS_LICENSED_BY_NINTENDO;
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 2:
            if (aos_title_brightness_lower(&logo->brightness, 2)) {
                logo->duration_timer = 2 * 60;
                ++mode->update_stage;
                mode->stage_timer = 0;
            }
            break;
        case 3:
            if (--logo->duration_timer == 0) {
                logo->blend_control = AOS_BLEND_SCREEN_BRIGHTNESS_INCREASE;
                ++mode->update_stage;
                mode->stage_timer = 0;
            }
            break;
        case 4:
            if (aos_title_brightness_raise(&logo->brightness, 2))
                return AOS_MODE_KONAMI_LOGO;
            break;
        default:
            break;
    }
    return AOS_MODE_SAME;
}

void aos_konami_code_check(AosStartupLogoMode *logo, const AosInput *input) {
    static const uint16_t sequence[] = {
        AOS_KEY_UP, AOS_KEY_UP, AOS_KEY_DOWN, AOS_KEY_DOWN, AOS_KEY_LEFT,
        AOS_KEY_RIGHT, AOS_KEY_LEFT, AOS_KEY_RIGHT, AOS_KEY_B, AOS_KEY_A,
    };
    uint16_t pressed = input ? input->new_input : 0;
    if (logo->konami_code_counter < 1 ||
        logo->konami_code_counter > sizeof sequence / sizeof sequence[0])
        return;
    if (pressed == sequence[logo->konami_code_counter - 1])
        ++logo->konami_code_counter;
    else if (pressed)
        logo->konami_code_counter = 0;
}

int32_t aos_game_mode_konami_logo_update(AosGameMode *mode, AosStartupLogoMode *logo,
                                         const AosInput *input, uint32_t progress_flags) {
    switch (mode->update_stage) {
        case 0:
            logo->display_control = AOS_DCNT_BG0;
            logo->bg0_control = AOS_BG0_HIGH_PRIORITY;
            logo->blend_control = AOS_BLEND_SCREEN_BRIGHTNESS_INCREASE;
            logo->brightness = AOS_BRIGHTNESS_MAX;
            logo->asset_request = AOS_STARTUP_ASSETS_NONE;
            logo->cleanup_requested = true;
            logo->konami_code_sound_requested = false;
            if ((progress_flags & 0x3fu) && logo->konami_code_counter != UINT8_MAX)
                logo->konami_code_counter = 1;
            else
                logo->konami_code_counter = 0;
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 1:
            logo->asset_request = AOS_STARTUP_ASSETS_KONAMI_LOGO;
            ++mode->update_stage;
            mode->stage_timer = 0;
            break;
        case 2:
            if (aos_title_brightness_lower(&logo->brightness, 1)) {
                logo->duration_timer = 4 * 60;
                ++mode->update_stage;
                mode->stage_timer = 0;
            }
            break;
        case 3:
            aos_konami_code_check(logo, input);
            if (--logo->duration_timer == 0 ||
                (input && (input->new_input & (AOS_KEY_START | AOS_KEY_A)))) {
                logo->blend_control = AOS_BLEND_SCREEN_BRIGHTNESS_DECREASE;
                ++mode->update_stage;
                mode->stage_timer = 0;
            }
            break;
        case 4:
            if (aos_title_brightness_raise(&logo->brightness, 1)) {
                if (logo->konami_code_counter == 11) {
                    logo->konami_code_sound_requested = true;
                    logo->konami_code_counter = UINT8_MAX;
                }
                return AOS_MODE_TITLE_SCREEN;
            }
            break;
        default:
            break;
    }
    return AOS_MODE_SAME;
}

/* sTitleScreenNextGameMode, src/code_08002454.c, order:
 * main menu, soul trade, Konami logo, boss rush, sound test.
 * The table's values are resolved by native enum constants at the caller. */
int32_t aos_title_destination(unsigned selection, const int32_t modes[5]) {
    if (selection >= 5) return AOS_MODE_SAME;
    return modes[selection];
}
