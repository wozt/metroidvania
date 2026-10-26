/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_GAME_MODE_H
#define AOS_GAME_MODE_H
#include <stdbool.h>
#include <stdint.h>
#include "aos_input.h"

/* Pure mode dispatch state from main.c, without graphics or host lifecycle. */
typedef struct {
    int32_t mode;
    int32_t update_stage;
    int32_t stage_timer;
} AosGameMode;

/* Native sentinel results from GameModeUpdate. */
#define AOS_MODE_SAME (-1)
#define AOS_MODE_NEXT (-2)

/* enum GameMode, include/constants/main.h. */
enum {
    AOS_MODE_KONAMI_LOGO, AOS_MODE_TITLE_SCREEN, AOS_MODE_MAIN_MENU, AOS_MODE_UNK_3,
    AOS_MODE_IN_GAME, AOS_MODE_GAME_OVER, AOS_MODE_CREDITS, AOS_MODE_INTRO_CUTSCENE,
    AOS_MODE_BOSS_RUSH_MENU, AOS_MODE_UNK_9, AOS_MODE_SOUL_TRADE_MENU,
    AOS_MODE_SOUND_TEST_MENU, AOS_MODE_LICENSED_BY_NINTENDO, AOS_MODE_RESET,
    AOS_MODE_DEBUG, AOS_MODE_UNK_F, AOS_MODE_DEBUG_EXIT, AOS_MODE_DEBUG_1967,
    AOS_MODE_DEBUG_2229, AOS_MODE_DEBUG_2739, AOS_MODE_DEBUG_4531,
    AOS_MODE_COUNT
};

/* One mode's update handler; it returns a mode, AOS_MODE_SAME or
 * AOS_MODE_NEXT. A NULL entry behaves as GAME_MODE_UNK_F's empty case. */
typedef int32_t (*AosGameModeHandler)(AosGameMode *state, void *context);

/* Rendering calls made by GameModeResetUpdate's language menu. They mirror
 * sub_08046DD4, sub_08040898, sub_08041338 and sub_08040C60 without binding
 * the source-derived controller to a particular renderer. */
typedef struct {
    void *context;
    void (*draw_panel)(void *context, uint8_t x, uint8_t y, uint16_t tile, uint8_t width);
    void (*prepare_menu)(void *context);
    void (*set_text_cursor)(void *context, uint8_t x, uint8_t y);
    void (*draw_text)(void *context, uint16_t text_id, uint8_t style);
    void (*write_cursor_tile)(void *context, uint16_t tile);
} AosResetMenuHooks;

typedef struct {
    uint8_t language;
    uint16_t display_control;
    uint16_t bg0_control;
    uint16_t blend_control;
    uint16_t brightness;
    int32_t duration_timer;
    bool bg_map_clear_requested;
    bool bg_tiles_clear_requested;
    bool menu_prepared;
    bool cursor_changed;
    bool selection_confirmed;
    uint8_t cursor_from;
    uint8_t cursor_to;
} AosResetMode;

typedef enum {
    AOS_STARTUP_ASSETS_NONE,
    AOS_STARTUP_ASSETS_LICENSED_BY_NINTENDO,
    AOS_STARTUP_ASSETS_KONAMI_LOGO
} AosStartupAssetRequest;

/* Register, timer and request state shared by the two startup-logo modes.
 * Asset and cleanup operations stay explicit requests until a renderer owns
 * the corresponding native graphics resources. */
typedef struct {
    uint16_t display_control;
    uint16_t bg0_control;
    uint16_t blend_control;
    uint16_t brightness;
    int32_t duration_timer;
    AosStartupAssetRequest asset_request;
    bool cleanup_requested;
    uint8_t konami_code_counter;
    bool konami_code_sound_requested;
} AosStartupLogoMode;

void aos_game_mode_set(AosGameMode *state, int32_t mode);
void aos_game_mode_increment(AosGameMode *state);
void aos_game_mode_accept(AosGameMode *state, int32_t update_result);
/* GameModeUpdate: runs the current mode's handler (indexed by the native
 * mode number) and applies its result. A mode outside the table matches no
 * case of the native switch and keeps the current mode. */
void aos_game_mode_update(AosGameMode *state, const AosGameModeHandler handlers[AOS_MODE_COUNT],
                          void *context);
/* The mode tail of sub_080006CC (boot): SetGameMode(KONAMI_LOGO), then
 * SetGameMode(RESET), so the first frame runs the reset mode. */
void aos_game_mode_boot(AosGameMode *state);
/* Stage 0 of GameModeResetUpdate: the US build forces the saved language to
 * 1 (English), asks for SaveData_SaveLanguage when it changed and leaves for
 * the "Licensed by Nintendo" mode. Returns that mode; *save_language tells
 * whether the language must be written. */
int32_t aos_game_mode_reset_stage0(uint8_t *language, int *save_language);
/* GameModeResetUpdate stages 0-5. Stages 1-3 initialize BG0, issue the
 * language-menu draw sequence and fade it in; stage 4 handles A/B/Up/Down
 * through the native AosInput edge state; stage 5 fades out and requests the
 * native language save before returning LICENSED_BY_NINTENDO. */
int32_t aos_game_mode_reset_update(AosGameMode *mode, AosResetMode *reset,
                                   const AosInput *input, const AosResetMenuHooks *hooks,
                                   int *save_language);
/* GameModeLicensedByNintendoUpdate and GameModeKonamiLogoUpdate. Graphics
 * loads and the Konami-mode subsystem cleanup are reported as requests;
 * controller timing, input, fades and destinations match the native modes. */
int32_t aos_game_mode_licensed_by_nintendo_update(AosGameMode *mode,
                                                   AosStartupLogoMode *logo);
void aos_konami_code_check(AosStartupLogoMode *logo, const AosInput *input);
int32_t aos_game_mode_konami_logo_update(AosGameMode *mode, AosStartupLogoMode *logo,
                                         const AosInput *input, uint32_t progress_flags);

/* Original title destination table: index is selected by the title handler.
 * Modes are supplied by the caller to avoid guessing version-specific enum
 * numbers from the decompiled GBA sources. */
int32_t aos_title_destination(unsigned selection, const int32_t modes[5]);
#endif
