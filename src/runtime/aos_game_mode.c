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

/* sTitleScreenNextGameMode, src/code_08002454.c, order:
 * main menu, soul trade, Konami logo, boss rush, sound test.
 * The table's values are resolved by native enum constants at the caller. */
int32_t aos_title_destination(unsigned selection, const int32_t modes[5]) {
    if (selection >= 5) return AOS_MODE_SAME;
    return modes[selection];
}
