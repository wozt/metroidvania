/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the main menu's objects (aos_main_menu_sprites), its
 * file panels (aos_main_menu_files) and the OBJ layer they draw into. */
#include "aos_main_menu_files.h"
#include "aos_main_menu_sprites.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosObjSprite sprites[AOS_MENU_MAX_SPRITES];

static int find(int count, uint8_t sheet, uint8_t record, int x, int y) {
    for (int k = 0; k < count; ++k)
        if (sprites[k].sheet == sheet && sprites[k].record == record && sprites[k].x == x &&
            sprites[k].y == y)
            return k;
    return -1;
}

int main(void) {
    /* The OBJ layer: palette row * 16 + colour, flips, the first sprite of
     * a priority on top, a lower priority value over it. */
    static AosObjSheet sheets[AOS_MENU_SHEETS];
    static AosObjLayer layer;
    AosObjSheet *icons = &sheets[AOS_MENU_SHEET_ICONS];
    icons->frame_count = 2;
    icons->frames[0].count = 1;
    icons->frames[0].pieces[0] = (AosObjPiece){0, 0, 8, 8, 0, 0, false, false};
    icons->frames[1].count = 1;
    icons->frames[1].pieces[0] = (AosObjPiece){0, 0, 8, 8, 0, 0, true, false};
    icons->pixels[0] = 3;                               /* sheet (0, 0) */
    icons->pixels[7] = 5;                               /* sheet (7, 0) */
    aos_obj_layer_clear(&layer);
    AosObjSprite a = {AOS_MENU_SHEET_ICONS, 0, 2, 1, 0, false, 0x100, 0, 0, 0x100, 10, 10};
    AosObjSprite b = {AOS_MENU_SHEET_ICONS, 1, 4, 1, 0, false, 0x100, 0, 0, 0x100, 10, 10};
    aos_obj_layer_draw(&layer, sheets, &a);
    aos_obj_layer_draw(&layer, sheets, &b);
    assert(layer.index[10 * AOS_OBJ_SCREEN_W + 10] == 0x23 &&
           layer.index[10 * AOS_OBJ_SCREEN_W + 17] == 0x25);   /* b's flipped (7, 0) */
    b.priority = 0;
    aos_obj_layer_draw(&layer, sheets, &b);
    assert(layer.index[10 * AOS_OBJ_SCREEN_W + 10] == 0x45 && layer.priority[10 * AOS_OBJ_SCREEN_W + 10] == 0);

    /* The ornament's animation 0 (frames 17, 18, six frames each). */
    icons->anim_count = 1;
    icons->anim_length[0] = 2;
    icons->anim_frame[0][0] = 17;
    icons->anim_frame[0][1] = 18;
    icons->anim_duration[0][0] = icons->anim_duration[0][1] = 6;
    static AosMenuFileSummary files[AOS_MENU_FILES];
    static const uint8_t soma[8] = {0x14, 0x10, 0x0E, 0x02, 0, 0, 0, 0};
    files[0] = (AosMenuFileSummary){60u * (3600 + 2 * 60 + 3), {0}, 0, 0, 4, 0, 0, 0, 123, 1234, 0};
    memcpy(files[0].name, soma, 8);
    AosMainMenu menu = {0};
    AosMainMenuSpriteState state = {0};

    /* The base menu (sub_080069C4 case 1): the labels, COPY lit, and the
     * ornament at 0x48 - 1 and 0x48 - 1 + 0x62 on its row. */
    menu.selected_slot = 1;
    int n = aos_main_menu_sprites(&menu, false, files, 0, icons, &state, sprites);
    assert(n == 5);
    assert(sprites[0].sheet == AOS_MENU_SHEET_LABELS && sprites[0].record == 0 &&
           sprites[0].palette_row == 15 && sprites[0].x == 0x20 && sprites[0].y == 0x10);
    assert(sprites[1].record == 1 && sprites[1].palette_row == 5 && sprites[1].x == 0x50 &&
           sprites[1].y == 0x30);
    assert(sprites[2].sheet == AOS_MENU_SHEET_ICONS && sprites[2].palette_row == 14 &&
           sprites[2].record == 17 && sprites[2].x == 0x47 && sprites[2].y == 0x38 &&
           sprites[3].x == 0x47 + 0x62);
    for (int k = 0; k < 4; ++k) aos_main_menu_sprites(&menu, false, files, 0, icons, &state, sprites);
    aos_main_menu_sprites(&menu, false, files, 0, icons, &state, sprites);
    assert(sprites[2].record == 18);

    /* PLAY (sub_08005928): the hand on file 2 (empty: NEW GAME, 0xA at
     * 0x90, 0x38), wobbling 3 pixels left at frameCounter 0. */
    menu.selected_slot = 0;
    menu.selected_slot_active = true;
    menu.file_cursor = 2;
    n = aos_main_menu_sprites(&menu, false, files, 0, icons, &state, sprites);
    assert(find(n, AOS_MENU_SHEET_ICONS, 0x10, 2 * 0x10 + 0x10 - 3, 0x48) >= 0);
    assert(find(n, AOS_MENU_SHEET_ICONS, 0xA, 0x90, 0x38) >= 0);
    for (int i = 0; i < 3; ++i) assert(find(n, AOS_MENU_SHEET_ICONS, (uint8_t)i, (i << 5) + 0x10, 0x40) >= 0);
    /* On file 1: its name in the font, one glyph every 12 pixels. */
    menu.file_cursor = 0;
    n = aos_main_menu_sprites(&menu, false, files, 4, icons, &state, sprites);
    for (int k = 0; k < 4; ++k) assert(find(n, AOS_MENU_SHEET_FONT, soma[k], 0x80 + 12 * k, 0x20) >= 0);
    assert(find(n, AOS_MENU_SHEET_ICONS, 0xA, 0x90, 0x38) < 0);
    /* Its confirmation: the hand under YES / NO at 0x24 + 0x60 * choice. */
    menu.confirm_active = true;
    menu.confirm_choice = 1;
    n = aos_main_menu_sprites(&menu, false, files, 0, icons, &state, sprites);
    assert(find(n, AOS_MENU_SHEET_ICONS, 0x10, 0x60 + 0x24 - 3, 0x8C) >= 0);
    menu.confirm_active = false;

    /* COPY's confirmation (sub_08005E28, phase 2). */
    menu.selected_slot = 1;
    menu.file_phase = 2;
    menu.file_cursor = 0;
    menu.source_slot = 0;
    menu.destination_slot = 2;
    n = aos_main_menu_sprites(&menu, false, files, 0, icons, &state, sprites);
    assert(find(n, AOS_MENU_SHEET_ICONS, 0x10, 0x46 - 3, 0x90) >= 0);
    assert(find(n, AOS_MENU_SHEET_ICONS, 9, 0x98, 0x38) >= 0);     /* empty target */

    /* Name entry (sub_08006318): the ornaments, the cursor, 35 symbols. */
    n = aos_main_menu_sprites(&menu, true, files, 0, icons, &state, sprites);
    assert(n == 2 + 1 + 35 && find(n, AOS_MENU_SHEET_FONT, 1, 0x18, 0x40) >= 0 &&
           find(n, AOS_MENU_SHEET_FONT, 2, 0x12, 0x38) >= 0 &&
           find(n, AOS_MENU_SHEET_FONT, 0x23, 0x48, 0x10) < 0);
    n = aos_main_menu_sprites(&menu, true, files, 0x10, icons, &state, sprites);
    assert(find(n, AOS_MENU_SHEET_FONT, 0x23, 0x48, 0x10) >= 0);   /* the blinking caret */

    /* The file panels (sub_08006D9C through sub_08007204): level, map
     * per mille, play time and gold as 0xD100 + digit, the area's line of
     * text row 6, and BG1 shown unshifted for a used file. */
    static AosMenuBg0 bg0;
    AosMainMenuFilePanel panel;
    memset(&menu, 0, sizeof menu);
    menu.selected_slot_active = true;
    aos_main_menu_file_panels(&bg0, &menu, false, files, &panel);
    const uint16_t *map = bg0.map;
    assert(map[8 * 32 + 0x13] == 0xD104 && map[8 * 32 + 0x19] == 0xD103 &&
           map[8 * 32 + 0x17] == 0xD102);                  /* Lv 4, 12.3 */
    assert(map[9 * 32 + 0x1B] == 0xD103 && map[9 * 32 + 0x1A] == 0xD100 &&
           map[9 * 32 + 0x18] == 0xD102 && map[9 * 32 + 0x15] == 0xD101);   /* 01:02:03 */
    assert(map[10 * 32 + 0x19] == 0xD104 && map[10 * 32 + 0x16] == 0xD101);  /* 1234 */
    assert(map[7 * 32 + 0x10] == 0xC0C0 && map[7 * 32 + 0x1B] == 0xC0CB);
    assert(panel.area_count == 1 && panel.area_text[0] == 0x3B7 && panel.area_row[0] == 6 &&
           panel.split_first == 0x18 && panel.split_count == 0x50 && panel.split_vofs == 0);
    /* An empty file: nothing written, BG1 shifted by 0x88. */
    menu.file_cursor = 2;
    aos_main_menu_file_panels(&bg0, &menu, false, files, &panel);
    assert(map[8 * 32 + 0x13] == 0 && panel.area_count == 0 && panel.split_vofs == 0x88);
    /* COPY past its first phase with a used target: both panels, the
     * second on text row 7, BG1 shifted on 0x40 lines. */
    files[2] = files[0];
    files[2].area = 3;
    menu.selected_slot = 1;
    menu.file_phase = 1;
    menu.source_slot = 0;
    menu.file_cursor = 2;
    aos_main_menu_file_panels(&bg0, &menu, false, files, &panel);
    assert(panel.area_count == 2 && panel.area_text[1] == 0x3B7 + 3 && panel.area_row[1] == 7 &&
           panel.split_count == 0x40 && panel.split_vofs == 0x88 && map[7 * 32 + 0x11] == 0xC0E0);
    printf("aos_main_menu_sprites: ok\n");
    return 0;
}
