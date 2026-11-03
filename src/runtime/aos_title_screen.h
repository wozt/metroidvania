/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's title screen: GameModeTitleScreenUpdate (code_08002454.c) with its
 * display registers, its palette system (sub_0803BE04 and the fades of
 * sub_0803D2D0, sub_0803D18C, sub_0803CDF0), the palette cycle of
 * TitleScreen_sub_08002FF8, the BG1 H-blank wave of
 * TitleScreen_sub_08002F44, entity 0x12's OBJ palette sequencer
 * (sub_08004004, whose end lets stage 4 go on) and the menu entity 0x14
 * (sub_080043CC / sub_0800432C), and a GBA-style compositor for its three
 * backgrounds (aos_title_graphics). The title objects (the window disc,
 * the bats, the sparkles, the menu and copyright sprites) are not drawn yet.
 *
 * Deliberate divergence: the menu has no SOUL TRADE entry (there is no
 * link cable on a PC); its entries are GAME START, then the extra entry
 * once unlocked. No SDL. */
#ifndef AOS_TITLE_SCREEN_H
#define AOS_TITLE_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_title_layers.h"

enum { AOS_TITLE_W = 240, AOS_TITLE_H = 160, AOS_TITLE_WAVE_LINES = 120 };

/* The palette effects of one palette (gEwramData + 0x123E4 of 0x11DDC for
 * the objects, 0x1261C for the backgrounds): per row (0-15) a blend toward
 * a target row, and (index 16) a blend toward one colour; bits of
 * `enabled` (+ 0x44), `on` (+ 0x48) and `running` (+ 0x4C). */
typedef struct {
    uint32_t enabled, on, running;
    uint16_t target[16][16];
    int32_t weight[17], end[17], step[17];
    uint8_t color[3];
} AosTitlePaletteFx;

typedef struct {
    uint16_t source[256];   /* the palette rows as the game writes them */
    uint16_t shown[256];    /* after the effects (sub_0803BE04) */
    AosTitlePaletteFx fx;
} AosTitlePalette;

/* The menu's entries (frame numbers of sUnk_084F0D2C) and the game modes
 * they lead to (sTitleScreenNextGameMode). */
enum { AOS_TITLE_ENTRY_GAME_START = 0, AOS_TITLE_ENTRY_SOUL_TRADE = 1,
       AOS_TITLE_ENTRY_EXTRA = 2, AOS_TITLE_ENTRY_BOSS_RUSH = 3,
       AOS_TITLE_ENTRY_SOUND_TEST = 4 };

typedef struct {
    const AosTitleGraphics *graphics;
    const uint8_t *obj_palette_rows;   /* 0x08209308 */
    const uint8_t *cycle_rows;         /* 0x080E622C */
    uint16_t stage;         /* gameModeUpdateStage */
    uint16_t counter;       /* gEwramData + 0x04 */
    bool timed_out;         /* gEwramData + 0x06 */
    uint32_t frame;         /* frameCounter */
    bool unlocked;          /* gEwramData + 0x60 bit 1: the extra entry */
    /* Display registers. */
    uint16_t dispcnt, bldcnt, bldalpha;
    uint8_t bldy;
    AosTitleAffine affine;  /* BG2PA... */
    int16_t wave[AOS_TITLE_WAVE_LINES]; /* BG1HOFS per scanline */
    AosTitlePalette bg, obj;
    /* Entity 0x12 (the title controller's disc). */
    int32_t e12_x, e12_y, motion, reveal;  /* + 0x524, + 0x528, + 0x500, + 0x504 */
    uint16_t e12_flags;     /* + 0x4FC */
    uint16_t progress;      /* + 0x4FE */
    uint16_t phase;         /* + 0x50A */
    uint8_t cycle_index;    /* + 0x508 */
    int32_t cycle_timer;    /* + 0x4F8 */
    uint8_t e12_state, e12_row, e12_wait; /* + 0x4EE, + 0x4EF, + 0x4F1 */
    bool e12_hidden;        /* + 0x53C bit 5 */
    uint8_t e12_blend;      /* + 0x53E bits 1-2 */
    /* Entity 0x13 (the bats' controller). */
    uint16_t e13_flags;     /* + 0x4FC */
    int32_t e13_timer;      /* + 0x4F8 */
    /* Entity 0x14 (the menu). */
    bool menu_shown;
    int8_t cursor;          /* + 0x4FC: among the shown entries */
    uint8_t first;          /* + 0x500: the first entry (0, or 3 in the submenu) */
    uint8_t slide_stage;    /* + 0x503 */
    bool sliding;           /* + 0x502 */
    int32_t slide;          /* + 0x504 */
    bool chosen;            /* + 0x4FE */
    bool copyright_shown;   /* entity 0x15 */
    /* Requests for the host. */
    int song;               /* PlaySong this frame, -1 for none */
} AosTitleScreen;

/* Stage 0 is entered at the next update. */
void aos_title_screen_init(AosTitleScreen *title, const AosTitleGraphics *graphics,
                           const uint8_t *obj_palette_rows, const uint8_t *cycle_rows);
/* One frame with the newly pressed and repeated keys (AOS_KEY_*): the game
 * mode it leaves for (AOS_MODE_*), or AOS_MODE_SAME. */
int32_t aos_title_screen_update(AosTitleScreen *title, uint16_t new_input,
                                uint16_t repeated_input);
/* The entries shown and the entry under the cursor. */
int aos_title_screen_entries(const AosTitleScreen *title, uint8_t entries[3]);
/* The screen as 240 x 160 RGBA pixels. */
void aos_title_screen_render(const AosTitleScreen *title, uint8_t rgba[AOS_TITLE_W * AOS_TITLE_H * 4]);

#endif
