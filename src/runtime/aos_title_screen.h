/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's title screen: GameModeTitleScreenUpdate (code_08002454.c) with its
 * display registers, its palette system (sub_0803BE04 and the fades of
 * sub_0803D2D0, sub_0803D18C, sub_0803CDF0), the palette cycle of
 * TitleScreen_sub_08002FF8, the BG1 H-blank wave of
 * TitleScreen_sub_08002F44, entity 0x12's OBJ palette sequencer
 * (sub_08004004, whose end lets stage 4 go on) and the menu entity 0x14
 * (sub_080043CC / sub_0800432C), the title's objects (the disc of entity
 * 0x12, its OBJ window, the 34 bats of sub_08003DBC, the swooping bat of
 * sub_08003CF8, the two sparkles of sub_08003F08, the menu of sub_0800427C,
 * the ring and copyright of sub_080044DC) as an OAM list, and a GBA-style
 * compositor for its three backgrounds (aos_title_graphics) and objects.
 *
 * Deliberate divergence: the menu has no SOUL TRADE entry (there is no
 * link cable on a PC); its entries are GAME START, then the extra entry
 * once unlocked. No SDL. */
#ifndef AOS_TITLE_SCREEN_H
#define AOS_TITLE_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "aos_title_layers.h"

enum { AOS_TITLE_W = 240, AOS_TITLE_H = 160, AOS_TITLE_WAVE_LINES = 120,
       AOS_TITLE_BATS = 34, AOS_TITLE_SPARKLES = 2, AOS_TITLE_MAX_SPRITES = 64 };

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

/* A bat (sub_08003A9C, updated by sub_080039D4): the entity's + 0x524 /
 * + 0x528 position, its drift (+ 0x4FC, + 0x504), the amplitudes (+ 0x534,
 * + 0x538), phases (+ 0x50C, + 0x50E) and speeds (+ 0x52E, + 0x532) of
 * its wobble, and animation 0 of the bat sheet. `record` is + 0x549. */
typedef struct {
    bool active, hidden, semi;  /* + 0x53C bit 5, + 0x53E bits 1-2 == 1 */
    uint8_t state;              /* + 0x4EE */
    int32_t x, y, vx, vy, ax, ay;
    uint16_t phase_x, phase_y, speed_x, speed_y;
    AosAnimState anim;
    uint8_t record;
} AosTitleBat;

/* The swooping bat (sub_08003CF8, sub_08003C18): animation 4 of the bat
 * sheet, affine, spiralling in to (0x7A, 0x1A). */
typedef struct {
    bool active, hidden;
    uint8_t state;
    uint16_t angle;             /* + 0x4FC */
    int32_t radius, scale;      /* + 0x500, + 0x504 */
    int32_t x, y;
    AosAnimState anim;
    uint8_t record;
} AosTitleSwoop;

/* A sparkle (sub_08003F08, sub_08003E64): animation 4 of the disc sheet,
 * affine and semi-transparent at (0x4E, 0x26); sparkle 0 drives BLDALPHA. */
typedef struct {
    bool active;
    uint8_t state, kind;        /* + 0x4EE, + 0x508 */
    int32_t size, scale, timer; /* + 0x500, + 0x504, + 0x4F8 */
    uint16_t angle, spin;       /* + 0x4FC, + 0x4FE */
    int32_t x, y;
    AosAnimState anim;
    uint8_t record;
} AosTitleSparkle;

/* One sprite of the OAM list (sub_0804311C / sub_0804342C): a frame record
 * of a sheet, its palette row, its priority (from its layer + 0x540 by the
 * thresholds 6, 7 and 0x16 of sub_0803DD14), its OBJ mode (0 normal, 1
 * semi-transparent, 2 OBJ window) and, when affine, the 8.8 matrix of
 * sub_0803E058 mapping screen offsets to frame offsets (double size). */
typedef AosObjSprite AosTitleSprite;

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
    /* The objects of sub_08003DBC and sub_08003F08. */
    uint32_t random;        /* gEwramData->randomNumber, 0x1967 at stage 1 */
    AosTitleBat bats[AOS_TITLE_BATS];
    AosTitleSwoop swoop;
    AosTitleSparkle sparkles[AOS_TITLE_SPARKLES];
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
/* The sprites drawn this frame in OAM order (the first on top among equal
 * priorities); returns their count. */
int aos_title_screen_sprites(const AosTitleScreen *title,
                             AosTitleSprite sprites[AOS_TITLE_MAX_SPRITES]);
/* The screen as 240 x 160 RGBA pixels. */
void aos_title_screen_render(const AosTitleScreen *title, uint8_t rgba[AOS_TITLE_W * AOS_TITLE_H * 4]);

#endif
