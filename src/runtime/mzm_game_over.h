/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission game over screen (menus/game_over.c, US version): the
 * fade from white, the GAME OVER letters and their breathing palette, the
 * continue / quit choice with Samus's head and the window darkening the
 * other option, the music and the fade out. The debug variant and the demo
 * ending are not ported. */
#ifndef MZM_GAME_OVER_H
#define MZM_GAME_OVER_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_screen.h"

enum { MZM_GAME_OVER_STAY = 0, MZM_GAME_OVER_CONTINUE, MZM_GAME_OVER_QUIT };

/* struct GameOverDynamicPalette */
typedef struct {
    uint16_t timer;
    uint8_t enable_flags, current_palette_row, unk_4;
    uint16_t palette[6];
    uint8_t timer_limit, unk_13;
} MzmGameOverPalette;

typedef struct {
    uint16_t timer, dispcnt, bldcnt, win1h, win1v;
    uint8_t option_selected, unk_35;
    MzmGameOverPalette dynamic_palette;
    MzmMenuOam oam[1];
    uint8_t suit;                   /* gEquipment.suitType */
    uint8_t sub_mode1;              /* gSubGameMode1 */
} MzmGameOver;

/* gSubGameMode1 = 0 with Samus's suit (her head's look). */
void mzm_game_over_enter(MzmGameOver *g, uint8_t suit);
/* GameOverHandler then GameOverVBlank for one frame; MZM_GAME_OVER_* once
 * the screen has faded out. */
int mzm_game_over_update(MzmGameOver *g, MzmScreen *s);

#endif
