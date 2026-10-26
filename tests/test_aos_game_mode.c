/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_mode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int calls;

typedef struct {
    char calls[64];
    unsigned count;
    uint16_t a[64], b[64], c[64], d[64];
} MenuLog;

static void log_call(MenuLog *log, char kind, unsigned a, unsigned b, unsigned c, unsigned d) {
    assert(log->count < sizeof log->calls - 1);
    unsigned at = log->count++;
    log->calls[at] = kind;
    log->calls[log->count] = '\0';
    log->a[at] = (uint16_t)a;
    log->b[at] = (uint16_t)b;
    log->c[at] = (uint16_t)c;
    log->d[at] = (uint16_t)d;
}

static void draw_panel(void *context, uint8_t x, uint8_t y, uint16_t tile, uint8_t width) {
    log_call(context, 'P', x, y, tile, width);
}

static void prepare_menu(void *context) {
    log_call(context, 'M', 0, 0, 0, 0);
}

static void set_text_cursor(void *context, uint8_t x, uint8_t y) {
    log_call(context, 'C', x, y, 0, 0);
}

static void draw_text(void *context, uint16_t text_id, uint8_t style) {
    log_call(context, 'T', text_id, style, 0, 0);
}

static void write_cursor_tile(void *context, uint16_t tile) {
    log_call(context, 'W', tile, 0, 0, 0);
}

static void clear_log(MenuLog *log) {
    *log = (MenuLog){0};
}

static int32_t stay(AosGameMode *state, void *context) {
    (void)state;
    ++*(int *)context;
    return AOS_MODE_SAME;
}

static int32_t advance(AosGameMode *state, void *context) {
    (void)state;
    (void)context;
    ++calls;
    return AOS_MODE_NEXT;
}

static int32_t to_reset(AosGameMode *state, void *context) {
    (void)context;
    state->stage_timer = 5;
    return AOS_MODE_RESET;
}

int main(void) {
    AosGameMode state = {3, 9, 12};
    aos_game_mode_accept(&state, AOS_MODE_SAME);
    assert(state.mode == 3 && state.update_stage == 9 && state.stage_timer == 12);
    aos_game_mode_accept(&state, AOS_MODE_NEXT);
    assert(state.mode == 4 && state.update_stage == 0 && state.stage_timer == 0);
    state.update_stage = 7;
    state.stage_timer = 8;
    aos_game_mode_accept(&state, 13);
    assert(state.mode == 13 && state.update_stage == 0 && state.stage_timer == 0);
    state.update_stage = 2;
    state.stage_timer = 3;
    aos_game_mode_set(&state, 0);
    assert(state.mode == 0 && state.update_stage == 0 && state.stage_timer == 0);

    /* Sentinels are never confused with regular native mode numbers. */
    const int32_t destinations[5] = {2, 10, 0, 5, 11};
    for (unsigned i = 0; i < 5; ++i)
        assert(aos_title_destination(i, destinations) == destinations[i]);
    assert(aos_title_destination(5, destinations) == AOS_MODE_SAME);
    assert(aos_title_destination(999, destinations) == AOS_MODE_SAME);
    /* The native enum: mode numbers the switch of GameModeUpdate tests. */
    assert(AOS_MODE_IN_GAME == 4 && AOS_MODE_RESET == 13 && AOS_MODE_UNK_F == 15 &&
           AOS_MODE_DEBUG_4531 == 20 && AOS_MODE_COUNT == 21);

    /* GameModeUpdate runs exactly the current mode's handler. */
    AosGameModeHandler handlers[AOS_MODE_COUNT] = {0};
    int stays = 0;
    handlers[AOS_MODE_TITLE_SCREEN] = stay;
    handlers[AOS_MODE_KONAMI_LOGO] = advance;
    handlers[AOS_MODE_GAME_OVER] = to_reset;
    AosGameMode game = {AOS_MODE_TITLE_SCREEN, 2, 7};
    aos_game_mode_update(&game, handlers, &stays);
    assert(stays == 1 && game.mode == AOS_MODE_TITLE_SCREEN && game.update_stage == 2 &&
           game.stage_timer == 7);
    game = (AosGameMode){AOS_MODE_KONAMI_LOGO, 4, 1};
    aos_game_mode_update(&game, handlers, &stays);
    assert(calls == 1 && game.mode == AOS_MODE_TITLE_SCREEN && !game.update_stage &&
           !game.stage_timer);
    /* A handler's own stage writes are reset by an explicit destination. */
    game = (AosGameMode){AOS_MODE_GAME_OVER, 3, 0};
    aos_game_mode_update(&game, handlers, &stays);
    assert(game.mode == AOS_MODE_RESET && !game.update_stage && !game.stage_timer);
    /* GAME_MODE_UNK_F (an empty case), an unported entry and an out-of-range
     * mode keep the state. */
    for (int32_t mode = 0; mode < 3; ++mode) {
        int32_t value = mode == 0 ? AOS_MODE_UNK_F : mode == 1 ? AOS_MODE_CREDITS : 99;
        game = (AosGameMode){value, 6, 9};
        aos_game_mode_update(&game, handlers, &stays);
        assert(game.mode == value && game.update_stage == 6 && game.stage_timer == 9);
    }

    /* sub_080006CC ends in the reset mode with a clean stage. */
    game = (AosGameMode){AOS_MODE_IN_GAME, 8, 8};
    aos_game_mode_boot(&game);
    assert(game.mode == AOS_MODE_RESET && !game.update_stage && !game.stage_timer);

    /* GameModeResetUpdate stage 0 forces English (1) and saves only a change. */
    for (unsigned value = 0; value < 5; ++value) {
        uint8_t language = (uint8_t)value;
        int save = -1;
        assert(aos_game_mode_reset_stage0(&language, &save) == AOS_MODE_LICENSED_BY_NINTENDO);
        assert(language == 1 && save == (value != 1));
    }

    /* Reset stages 1-4: BG0, the three native language strings, the
     * two-level-per-frame fade and edge-triggered wrapped selection. */
    MenuLog log = {0};
    AosResetMenuHooks hooks = {
        .context = &log,
        .draw_panel = draw_panel,
        .prepare_menu = prepare_menu,
        .set_text_cursor = set_text_cursor,
        .draw_text = draw_text,
        .write_cursor_tile = write_cursor_tile,
    };
    AosResetMode reset = {.language = 0};
    AosInput input = {0};
    game = (AosGameMode){AOS_MODE_RESET, 1, 77};
    int save = -1;
    assert(aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save) ==
           AOS_MODE_SAME);
    assert(game.update_stage == 2 && game.stage_timer == 0 && save == 0);
    assert(reset.display_control == 0x0100 && reset.bg0_control == 0x1C02);
    assert(reset.blend_control == 0x00BF && reset.brightness == 16);
    assert(reset.bg_map_clear_requested && reset.bg_tiles_clear_requested);

    assert(aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save) ==
           AOS_MODE_SAME);
    assert(game.update_stage == 3 && reset.language == 1 && reset.menu_prepared);
    assert(!strcmp(log.calls, "PPPMCTCTCTCW"));
    assert(log.a[0] == 10 && log.b[0] == 5 && log.c[0] == 0xF020 && log.d[0] == 10);
    assert(log.b[1] == 7 && log.c[1] == 0xF040);
    assert(log.b[2] == 9 && log.c[2] == 0xF060);
    assert(log.a[5] == 0xB4C && log.a[7] == 0xB4D && log.a[9] == 0xB4E);
    assert(log.a[10] == 0 && log.b[10] == 1 && log.a[11] == 0x3E);

    for (unsigned frame = 0; frame < 8; ++frame) {
        assert(aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save) ==
               AOS_MODE_SAME);
        assert(game.update_stage == 3);
        assert(reset.brightness == 14 - frame * 2);
    }
    assert(aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save) ==
           AOS_MODE_SAME);
    assert(game.update_stage == 4 && reset.brightness == 0 && reset.duration_timer == 120);

    clear_log(&log);
    aos_input_update(&input, AOS_KEY_DOWN);
    aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save);
    assert(reset.language == 2 && reset.cursor_changed && reset.cursor_from == 1 &&
           reset.cursor_to == 2 && !reset.selection_confirmed);
    assert(!strcmp(log.calls, "CWWCW"));
    assert(log.a[0] == 0 && log.b[0] == 1 && log.a[1] == 0x20 && log.a[2] == 0x20);
    assert(log.a[3] == 0 && log.b[3] == 2 && log.a[4] == 0x3E);

    aos_input_update(&input, 0);
    aos_input_update(&input, AOS_KEY_DOWN);
    aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save);
    assert(reset.language == 3);
    aos_input_update(&input, 0);
    aos_input_update(&input, AOS_KEY_DOWN);
    aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save);
    assert(reset.language == 1);
    aos_input_update(&input, 0);
    aos_input_update(&input, AOS_KEY_UP);
    aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save);
    assert(reset.language == 3);
    aos_input_update(&input, 0);
    aos_input_update(&input, AOS_KEY_B);
    aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save);
    assert(reset.language == 1);
    aos_input_update(&input, 0);
    aos_input_update(&input, AOS_KEY_A);
    aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save);
    assert(game.update_stage == 5 && game.stage_timer == 0 && reset.selection_confirmed &&
           !reset.cursor_changed);
    for (unsigned frame = 0; frame < 8; ++frame) {
        assert(aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save) ==
               AOS_MODE_SAME);
        assert(reset.brightness == (frame + 1) * 2 && save == 0);
    }
    assert(aos_game_mode_reset_update(&game, &reset, &input, &hooks, &save) ==
           AOS_MODE_LICENSED_BY_NINTENDO);
    assert(reset.brightness == 16 && save == 1);

    reset = (AosResetMode){.language = 3};
    game = (AosGameMode){AOS_MODE_RESET, 0, 0};
    save = 0;
    assert(aos_game_mode_reset_update(&game, &reset, &input, NULL, &save) ==
           AOS_MODE_LICENSED_BY_NINTENDO);
    assert(reset.language == 1 && save == 1);

    /* Licensed by Nintendo: register setup, asset request, 16-to-0 fade,
     * exact two-second hold, 0-to-16 fade and Konami destination. */
    AosStartupLogoMode logo = {0};
    game = (AosGameMode){AOS_MODE_LICENSED_BY_NINTENDO, 0, 9};
    assert(aos_game_mode_licensed_by_nintendo_update(&game, &logo) == AOS_MODE_SAME);
    assert(game.update_stage == 1 && game.stage_timer == 0);
    assert(logo.display_control == 0x0100 && logo.bg0_control == 0x1C02);
    assert(logo.blend_control == 0x00BF && logo.brightness == 16);
    assert(logo.asset_request == AOS_STARTUP_ASSETS_NONE);
    assert(aos_game_mode_licensed_by_nintendo_update(&game, &logo) == AOS_MODE_SAME);
    assert(game.update_stage == 2 &&
           logo.asset_request == AOS_STARTUP_ASSETS_LICENSED_BY_NINTENDO);
    for (unsigned frame = 0; frame < 8; ++frame) {
        assert(aos_game_mode_licensed_by_nintendo_update(&game, &logo) == AOS_MODE_SAME);
        assert(logo.brightness == 14 - frame * 2 && game.update_stage == 2);
    }
    assert(aos_game_mode_licensed_by_nintendo_update(&game, &logo) == AOS_MODE_SAME);
    assert(game.update_stage == 3 && logo.duration_timer == 120);
    for (unsigned frame = 0; frame < 119; ++frame)
        assert(aos_game_mode_licensed_by_nintendo_update(&game, &logo) == AOS_MODE_SAME);
    assert(game.update_stage == 3 && logo.duration_timer == 1);
    assert(aos_game_mode_licensed_by_nintendo_update(&game, &logo) == AOS_MODE_SAME);
    assert(game.update_stage == 4 && logo.duration_timer == 0);
    for (unsigned frame = 0; frame < 8; ++frame) {
        assert(aos_game_mode_licensed_by_nintendo_update(&game, &logo) == AOS_MODE_SAME);
        assert(logo.brightness == (frame + 1) * 2);
    }
    assert(aos_game_mode_licensed_by_nintendo_update(&game, &logo) ==
           AOS_MODE_KONAMI_LOGO);

    /* Konami logo: native cleanup/assets, one-level fade, exact input code,
     * Start/A exit and the title-screen transition after the black fade. */
    logo = (AosStartupLogoMode){0};
    input = (AosInput){0};
    game = (AosGameMode){AOS_MODE_KONAMI_LOGO, 0, 8};
    assert(aos_game_mode_konami_logo_update(&game, &logo, &input, 1) == AOS_MODE_SAME);
    assert(game.update_stage == 1 && game.stage_timer == 0);
    assert(logo.display_control == 0x0100 && logo.bg0_control == 0x1C00);
    assert(logo.blend_control == 0x00BF && logo.brightness == 16);
    assert(logo.cleanup_requested && logo.konami_code_counter == 1);
    assert(aos_game_mode_konami_logo_update(&game, &logo, &input, 1) == AOS_MODE_SAME);
    assert(game.update_stage == 2 && logo.asset_request == AOS_STARTUP_ASSETS_KONAMI_LOGO);
    for (unsigned frame = 0; frame < 16; ++frame) {
        assert(aos_game_mode_konami_logo_update(&game, &logo, &input, 1) == AOS_MODE_SAME);
        assert(logo.brightness == 15 - frame && game.update_stage == 2);
    }
    assert(aos_game_mode_konami_logo_update(&game, &logo, &input, 1) == AOS_MODE_SAME);
    assert(game.update_stage == 3 && logo.duration_timer == 240);

    static const uint16_t konami_code[] = {
        AOS_KEY_UP, AOS_KEY_UP, AOS_KEY_DOWN, AOS_KEY_DOWN, AOS_KEY_LEFT,
        AOS_KEY_RIGHT, AOS_KEY_LEFT, AOS_KEY_RIGHT, AOS_KEY_B, AOS_KEY_A,
    };
    for (unsigned index = 0; index < sizeof konami_code / sizeof konami_code[0]; ++index) {
        input.new_input = konami_code[index];
        assert(aos_game_mode_konami_logo_update(&game, &logo, &input, 1) ==
               AOS_MODE_SAME);
        assert(logo.konami_code_counter == index + 2);
    }
    assert(game.update_stage == 4 && logo.blend_control == 0x00FF);
    for (unsigned frame = 0; frame < 16; ++frame) {
        assert(aos_game_mode_konami_logo_update(&game, &logo, &input, 1) ==
               AOS_MODE_SAME);
        assert(logo.brightness == frame + 1);
    }
    assert(aos_game_mode_konami_logo_update(&game, &logo, &input, 1) ==
           AOS_MODE_TITLE_SCREEN);
    assert(logo.konami_code_counter == UINT8_MAX && logo.konami_code_sound_requested);

    logo = (AosStartupLogoMode){.konami_code_counter = 1};
    input = (AosInput){.new_input = AOS_KEY_DOWN};
    aos_konami_code_check(&logo, &input);
    assert(logo.konami_code_counter == 0);

    logo = (AosStartupLogoMode){.duration_timer = 240, .konami_code_counter = 1};
    input = (AosInput){.new_input = AOS_KEY_START};
    game = (AosGameMode){AOS_MODE_KONAMI_LOGO, 3, 4};
    aos_game_mode_konami_logo_update(&game, &logo, &input, 1);
    assert(game.update_stage == 4 && game.stage_timer == 0 &&
           logo.konami_code_counter == 0 && logo.blend_control == 0x00FF);

    logo.konami_code_counter = UINT8_MAX;
    game = (AosGameMode){AOS_MODE_KONAMI_LOGO, 0, 0};
    aos_game_mode_konami_logo_update(&game, &logo, &input, 0x3F);
    assert(logo.konami_code_counter == 0 && !logo.konami_code_sound_requested);

    logo = (AosStartupLogoMode){.duration_timer = 1};
    input = (AosInput){0};
    game = (AosGameMode){AOS_MODE_KONAMI_LOGO, 3, 4};
    aos_game_mode_konami_logo_update(&game, &logo, &input, 0);
    assert(game.update_stage == 4 && game.stage_timer == 0 &&
           logo.blend_control == 0x00FF);
    puts("Aria native game mode dispatch tests passed");
    return 0;
}
