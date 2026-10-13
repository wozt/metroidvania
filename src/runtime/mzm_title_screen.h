/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission title screen (menus/title_screen.c, US version): the
 * comets view and its scrolling, the title fading in, the animated
 * palettes, the sparkles, the prompt, START, and the fades in and out. The
 * idle demos are not ported: when one would start, the title screen is
 * entered again past its comets. */
#ifndef MZM_TITLE_SCREEN_H
#define MZM_TITLE_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_screen.h"

typedef struct { uint8_t palette_row, max_timer; uint16_t timer; uint8_t unk_4; } MzmTitleAnimatedPalette;
typedef struct { uint8_t stage; uint16_t timer; } MzmTitleOamTiming;

/* struct TitleScreenData and the game mode counters it runs under. */
typedef struct {
    uint16_t timer, demo_timer, effects_timer;
    uint8_t comets_stage;
    uint16_t comets_timer;
    uint8_t active_animated_palettes;
    uint16_t type;
    uint8_t unk_e, unk_f;
    uint8_t fading_stage, color_to_apply;
    bool palette_updated;
    uint16_t fading_timer;
    uint16_t dispcnt, bldcnt, unk_24;
    MzmTitleAnimatedPalette animated_palettes[3];
    MzmTitleOamTiming oam_timings[3];
    MzmMenuOam oam[7];
    uint8_t sub_mode1;              /* gSubGameMode1 */
    uint8_t sub_mode2;              /* gSubGameMode2: 1 START, 2 demo */
} MzmTitleScreen;

/* Enters the title screen (gSubGameMode1 = 0); `after_demo` skips the
 * comets as when a demo returns to it. */
void mzm_title_enter(MzmTitleScreen *t, bool after_demo);
/* TitleScreenHandler then TitleScreenVBlank for one frame; true when the
 * screen has faded out (`sub_mode2` says why). */
bool mzm_title_update(MzmTitleScreen *t, MzmScreen *s);

#endif
