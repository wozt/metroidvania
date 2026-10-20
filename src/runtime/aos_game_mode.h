/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_GAME_MODE_H
#define AOS_GAME_MODE_H
#include <stdint.h>

/* Pure mode dispatch state from main.c, without graphics or host lifecycle. */
typedef struct {
    int32_t mode;
    int32_t update_stage;
    int32_t stage_timer;
} AosGameMode;

/* Native sentinel results from GameModeUpdate. */
#define AOS_MODE_SAME (-1)
#define AOS_MODE_NEXT (-2)

void aos_game_mode_set(AosGameMode *state, int32_t mode);
void aos_game_mode_increment(AosGameMode *state);
void aos_game_mode_accept(AosGameMode *state, int32_t update_result);

/* Original title destination table: index is selected by the title handler.
 * Modes are supplied by the caller to avoid guessing version-specific enum
 * numbers from the decompiled GBA sources. */
int32_t aos_title_destination(unsigned selection, const int32_t modes[5]);
#endif
