/* SPDX-License-Identifier: GPL-3.0-only */
/* The main menu's file panels on BG0 and BG1: sub_08006D9C (a file's
 * statistics: level, map completion, play time, gold, and the line of its
 * area's name) and the screens that call it, sub_08007204 (PLAY),
 * sub_08007D84 (DELETE) and sub_0800737C (COPY), with the BG1 vertical
 * offset of their H-blank effect that hides the statistics labels of an
 * empty file. English layout (the other languages' name lines are not
 * ported). No SDL. */
#ifndef AOS_MAIN_MENU_FILES_H
#define AOS_MAIN_MENU_FILES_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_main_menu.h"
#include "aos_main_menu_sprites.h"
#include "aos_menu_bg0.h"

typedef struct {
    /* The area names to print (text id, text row): sub_08041338(0x3B7 +
     * area) after sub_08040970(0, 6 + panel, 0x1E, 1). */
    uint16_t area_text[2];
    uint8_t area_row[2];
    int area_count;
    /* BG1VOFS of scanlines first .. first + count - 1 (0 elsewhere). */
    int split_first, split_count;
    uint16_t split_vofs;
} AosMainMenuFilePanel;

/* sub_08006D9C's BG0 writes for one file at text cell (x, y), panel 0 or
 * 1; returns the area name's text id. */
uint16_t aos_main_menu_file_stats(AosMenuBg0 *bg0, const AosMenuFileSummary *file, int x, int y,
                                  int panel);
/* The current screen's panels: BG0's rows 0-12 cleared, then its files'
 * statistics; `panel` receives the area names and the BG1 split. */
void aos_main_menu_file_panels(AosMenuBg0 *bg0, const AosMainMenu *menu, bool name_entry,
                               const AosMenuFileSummary files[AOS_MENU_FILES],
                               AosMainMenuFilePanel *panel);

#endif
