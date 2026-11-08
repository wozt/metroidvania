/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_hud.h"

#include <assert.h>
#include <stdio.h>

static AosHud hud;

static uint16_t at(int x, int y) { return hud.map[y * AOS_HUD_MAP_SIZE + x]; }

int main(void) {
    /* sub_0800DA50 with full HP and MP: the frame of sub_0800DB78 and two
     * full bars (tiles 0x208 and 0x218) from cell (4, 1). */
    aos_hud_init(&hud, 320, 320, 80, 80);
    assert(at(0, 0) == (0xD000 | 0x209) && at(3, 0) == (0xD000 | 0x20C) &&
           at(0, 4) == (0xD000 | 0x249) && at(12, 0) == (0xD000 | 0x20D) &&
           at(13, 3) == (0xD000 | 0x23E) && at(14, 0) == 0);
    for (int x = 4; x < 12; ++x) {
        assert(at(x, 1) == (0xD000 | 0x208));
        assert(at(x, 2) == (0xD000 | 0x218));
    }
    /* sub_0800F138: 100 of 320 fills (100 * 100 / 320 << 6) / 100 = 19
     * pixels: two full tiles, a 3-pixel tile, then empty ones. */
    aos_hud_bar(&hud, 4, 1, 100, 320, false);
    assert(at(4, 1) == (0xD000 | 0x208) && at(5, 1) == (0xD000 | 0x208) &&
           at(6, 1) == (0xD000 | 0x203) && at(7, 1) == (0xD000 | 0x200) &&
           at(11, 1) == (0xD000 | 0x200));
    /* A tiny positive value still shows one pixel; zero shows none. */
    aos_hud_bar(&hud, 4, 2, 1, 999, true);
    assert(at(4, 2) == (0xD000 | 0x211) && at(5, 2) == (0xD000 | 0x210));
    aos_hud_bar(&hud, 4, 2, 0, 80, true);
    assert(at(4, 2) == (0xD000 | 0x210));

    /* sub_0800DC70: two steps a frame toward the real values. */
    aos_hud_init(&hud, 320, 320, 80, 80);
    aos_hud_update(&hud, 300, 320, 75, 80);
    assert(hud.hp == 318 && hud.mp == 78);
    for (int i = 0; i < 9; ++i) aos_hud_update(&hud, 300, 320, 75, 80);
    assert(hud.hp == 300 && hud.mp == 75);
    aos_hud_update(&hud, 300, 332, 75, 80);
    assert(hud.max_hp == 322);

    /* Pixels: BG0 scrolled 6 pixels up, palette 13, index 0 transparent. */
    static uint8_t tiles[AOS_HUD_TILE_BYTES];
    uint8_t palette[AOS_HUD_PALETTE_BYTES] = {0};
    palette[2 * 3] = 0x1F;                      /* colour 3: red */
    tiles[(0x209 - 0x200) * 32 + 6 * 4] = 0x03; /* tile 0x209, row 6, pixel 0 */
    aos_hud_init(&hud, 1, 1, 1, 1);
    uint16_t color;
    assert(aos_hud_pixel(&hud, tiles, palette, 0, 0, &color) && color == 0x001F);
    assert(!aos_hud_pixel(&hud, tiles, palette, 1, 0, &color));
    assert(!aos_hud_pixel(&hud, tiles, palette, 200, 100, &color));

    /* sub_0800EA98/sub_0800DE4C: a defeated enemy uses context B's
     * eight-pixel font, alternates the two dynamic tile pages and keeps the
     * right-aligned three-row notice for 0xF0 frames. */
    static uint8_t metrics[AOS_HUD_FONT_METRIC_BYTES];
    static uint8_t glyphs[AOS_HUD_FONT_BITMAP_BYTES];
    unsigned a = ('A' - 0x20) * 4, b = ('B' - 0x20) * 4;
    metrics[a] = metrics[b] = 0;
    metrics[a + 1] = 4;
    metrics[b + 1] = 3;
    glyphs[('A' - 0x20) * 10] = 0xF0;
    glyphs[('B' - 0x20) * 10] = 0xE0;
    AosHudFont font = {metrics, glyphs};
    assert(aos_hud_show_enemy(&hud, &font, "AB"));
    assert(hud.notice_active && hud.notice_timer == 0xF0 && hud.notice_page == 1);
    assert(at(24, 18) == 0xE220 && at(29, 18) == 0xE620);
    assert(at(24, 19) == 0xE230 && at(26, 19) == 0xE280 && at(29, 19) == 0xE630);
    assert(at(24, 20) == 0xE240 && at(29, 20) == 0xE640);
    palette[96 + 0xF * 2] = 0xE0;
    palette[96 + 0xF * 2 + 1] = 0x03;
    assert(aos_hud_pixel(&hud, tiles, palette, 26 * 8, 19 * 8 - AOS_HUD_SCROLL_Y,
                         &color) && color == 0x03E0);
    for (int i = 0; i < 0xEF; ++i) aos_hud_tick(&hud);
    assert(hud.notice_active && hud.notice_timer == 1);
    aos_hud_tick(&hud);
    assert(!hud.notice_active && !hud.notice_timer && at(26, 19) == 0);
    /* sub_0800EF98/sub_0800E0E8 uses the other palette and places the
     * equivalent frame at the left edge. */
    assert(aos_hud_show_item(&hud, &font, "AB"));
    assert(hud.notice_kind == AOS_HUD_NOTICE_ITEM && at(0, 18) == 0xE220 &&
           at(5, 18) == 0xE620 && at(0, 19) == 0xE230 && at(2, 19) == 0xE2A0 &&
           at(5, 19) == 0xE630 && at(0, 20) == 0xE240 && at(5, 20) == 0xE640);
    palette[64 + 0xF * 2] = 0x1F;
    assert(aos_hud_pixel(&hud, tiles, palette, 2 * 8, 19 * 8 - AOS_HUD_SCROLL_Y,
                         &color) && color == 0x001F);
    /* sub_0800EB04: a scripted clear drops the notice before its timer. */
    aos_hud_clear_notice(&hud);
    assert(!hud.notice_active && !hud.notice_timer && at(0, 19) == 0);
    puts("Aria HUD tests passed");
    return 0;
}
