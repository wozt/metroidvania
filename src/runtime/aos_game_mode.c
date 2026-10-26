/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_mode.h"

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

/* sTitleScreenNextGameMode, src/code_08002454.c, order:
 * main menu, soul trade, Konami logo, boss rush, sound test.
 * The table's values are resolved by native enum constants at the caller. */
int32_t aos_title_destination(unsigned selection, const int32_t modes[5]) {
    if (selection >= 5) return AOS_MODE_SAME;
    return modes[selection];
}
