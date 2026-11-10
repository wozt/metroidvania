/* SPDX-License-Identifier: GPL-3.0-only */
/* The main menu's objects: entity 0's draw routine sub_080069C4 and its
 * screens (the base menu, PLAY and DELETE through sub_08005928, COPY
 * through sub_08005E28, name entry through sub_08006318, the ornament of
 * sub_08006910), as the OAM list they build from the native controller
 * (aos_main_menu) and the files' summaries. The sprite styles are
 * sUnk_080E0CC0's (frame descriptor, sheet, OBJ palette row). SOUL TRADE's
 * screen (sub_0800656C) is not drawn: the port has no SOUL TRADE. No SDL,
 * no ROM. */
#ifndef AOS_MAIN_MENU_SPRITES_H
#define AOS_MAIN_MENU_SPRITES_H

#include <stdbool.h>
#include <stdint.h>

#include "aos/core/aos_anim.h"
#include "aos_main_menu.h"
#include "aos_obj.h"

enum { AOS_MENU_SHEET_ICONS = 0, AOS_MENU_SHEET_FONT = 1, AOS_MENU_SHEET_LABELS = 2,
       AOS_MENU_SHEETS = 3, AOS_MENU_MAX_SPRITES = 96, AOS_MENU_FILES = 6 };

/* What the menu reads of a file's 0x20-byte summary (SRAM slot + 0x190,
 * gEwramData + 0x94, read by sub_08013700). */
typedef struct {
    uint32_t play_time;       /* + 0x38 (inGameTimer): 0 for an unused file */
    uint8_t name[8];          /* + 0x20 */
    uint8_t area, room;       /* + 0x2A, + 0x2B */
    uint8_t level;            /* + 0x2C */
    uint8_t completed;        /* + 0x2D, low nibble */
    uint8_t icon;             /* + 0x2E (sub_080137F8): the second icon set */
    uint8_t character;        /* + 0x30 */
    uint16_t map_permille;    /* + 0x32 (sub_08011E24) */
    uint32_t gold;            /* + 0x34 */
    uint32_t extra_time;      /* + 0x3C */
} AosMenuFileSummary;

/* Entity 0's animation 0 (the ornament), started by sub_08006BC0. */
typedef struct {
    bool started;
    AosAnimState ornament;
} AosMainMenuSpriteState;

/* The sprites of this frame in OAM order; `files` are SRAM slots 0-5
 * (even: the file, odd: its quicksave), `frame_counter` is frameCounter
 * (the cursor's wobble and blinking), `icons` the sheet whose animation 0
 * is the ornament (NULL: no ornament animation). */
int aos_main_menu_sprites(const AosMainMenu *menu, bool name_entry,
                          const AosMenuFileSummary files[AOS_MENU_FILES],
                          uint32_t frame_counter, const AosObjSheet *icons,
                          AosMainMenuSpriteState *state,
                          AosObjSprite sprites[AOS_MENU_MAX_SPRITES]);

#endif
