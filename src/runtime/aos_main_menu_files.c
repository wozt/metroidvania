/* SPDX-License-Identifier: GPL-3.0-only */
/* The main menu's file panels: see aos_main_menu_files.h. */
#include "aos_main_menu_files.h"

#include <string.h>

enum { DIGITS = 0xD100, AREA_TEXT = 0x3B7, NAME_ENTRY = 0xC0C0 };

/* sUnk_080E0D98: the rooms of area 7 named as area 0xC. */
static const uint8_t area7_rooms[13] = {0xD, 0x10, 0x22, 0x23, 0x29, 0x24, 0x25,
                                        0x34, 0x33, 0x35, 0x36, 0x37, 0x16};

/* sub_08006D9C_inline_1 */
static int area_name(const AosMenuFileSummary *file) {
    if (file->area == 7)
        for (int k = 0; k < 13; ++k)
            if (file->room == area7_rooms[k]) return 0xC;
    return file->area;
}

uint16_t aos_main_menu_file_stats(AosMenuBg0 *bg0, const AosMenuFileSummary *file, int x, int y,
                                  int panel) {
    aos_menu_bg0_put_number(bg0, x + 3, y, file->level < 100 ? file->level : 0, 2, DIGITS, false);
    int permille = file->map_permille;
    aos_menu_bg0_put_number(bg0, x + 9, y, permille % 10, 3, DIGITS, false);
    aos_menu_bg0_put_number(bg0, x + 7, y, permille / 10, 1, DIGITS, false);
    int32_t seconds = (int32_t)(file->play_time / 60), minutes = seconds / 60;
    aos_menu_bg0_put_number(bg0, x + 0xB, y + 1, seconds % 60, 2, DIGITS, true);
    aos_menu_bg0_put_number(bg0, x + 8, y + 1, minutes % 60, 2, DIGITS, true);
    aos_menu_bg0_put_number(bg0, x + 5, y + 1, minutes / 60, 2, DIGITS, true);
    aos_menu_bg0_put_number(bg0, x + 9, y + 2, (int32_t)file->gold, 6, DIGITS, false);
    /* The name line: text row 6 + panel, one line or (areas 7 and 12) two. */
    uint16_t entry = (uint16_t)((panel << 5) + NAME_ENTRY);
    if (file->area == 7 || file->area == 12) {
        aos_menu_bg0_put_tiles(bg0, x, y - 2, entry, 7);
        aos_menu_bg0_put_tiles(bg0, x + 4, y - 1, (uint16_t)(entry + 7), 0xC);
    } else {
        aos_menu_bg0_put_tiles(bg0, x, y - 1, entry, 0xC);
    }
    /* The completed file's second time (+ 0x3C) is not drawn: the portable
     * saves keep no completion flag. */
    return (uint16_t)(AREA_TEXT + area_name(file));
}

static void clear_rows(AosMenuBg0 *bg0) {
    /* DMA_FILL_32(0x0600E000, 0x340): map rows 0-12. */
    memset(bg0->map, 0, 13 * AOS_MENU_BG0_MAP_SIZE * sizeof bg0->map[0]);
}

static const AosMenuFileSummary *file_at(const AosMenuFileSummary *files, int slot) {
    static const AosMenuFileSummary none;
    return slot >= 0 && slot < AOS_MENU_FILES ? &files[slot] : &none;
}

static void add_area(AosMainMenuFilePanel *panel, uint16_t text, int row) {
    if (panel->area_count >= 2) return;
    panel->area_text[panel->area_count] = text;
    panel->area_row[panel->area_count++] = (uint8_t)(6 + row);
}

void aos_main_menu_file_panels(AosMenuBg0 *bg0, const AosMainMenu *menu, bool name_entry,
                               const AosMenuFileSummary files[AOS_MENU_FILES],
                               AosMainMenuFilePanel *panel) {
    memset(panel, 0, sizeof *panel);
    clear_rows(bg0);
    if (name_entry || !menu->selected_slot_active) return;
    int cursor = menu->file_cursor;
    if (menu->selected_slot == 1) {
        /* sub_0800737C: the source, then (past phase 0) the target. */
        int source = menu->file_phase == 0 ? cursor : menu->source_slot;
        int target = menu->file_phase == 1 ? cursor : menu->destination_slot;
        add_area(panel, aos_main_menu_file_stats(bg0, file_at(files, source), 1, 8, 0), 0);
        bool empty = true;
        if (menu->file_phase != 0 && file_at(files, target)->play_time) {
            add_area(panel, aos_main_menu_file_stats(bg0, file_at(files, target), 0x11, 8, 1), 1);
            empty = false;
        }
        panel->split_first = 0x18;
        panel->split_count = 0x40;
        panel->split_vofs = empty ? 0 : 0x88;
        return;
    }
    bool del = menu->selected_slot == 2;
    int slot = del && menu->file_phase ? menu->source_slot : cursor >= 6 ? cursor - 6 : cursor;
    const AosMenuFileSummary *file = file_at(files, slot);
    bool empty = true;
    if (file->play_time && (del || cursor < 6)) {
        add_area(panel, aos_main_menu_file_stats(bg0, file, 0x10, 8, 0), 0);
        empty = false;
    } else if (file->play_time) {
        empty = false;
    }
    if (!del && cursor > 5 && (file->completed & 0xF)) empty = true;
    if (!del) {
        /* sub_08007204 (4EF 0 only): 0x50 lines from 0x18. */
        panel->split_first = 0x18;
        panel->split_count = 0x50;
        panel->split_vofs = empty ? 0x88 : 0;
    }
}
