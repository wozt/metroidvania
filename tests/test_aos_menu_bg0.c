/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_bg0.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosMenuBg0 bg0;

static uint16_t at(int x, int y) { return bg0.map[y * AOS_MENU_BG0_MAP_SIZE + x]; }

int main(int argc, char **argv) {
    /* sub_08046DD4: consecutive entries from one tile. */
    aos_menu_bg0_put_tiles(&bg0, 4, 7, 0xD110, 3);
    assert(at(4, 7) == 0xD110 && at(5, 7) == 0xD111 && at(6, 7) == 0xD112 && at(7, 7) == 0);
    aos_menu_bg0_put_tiles(&bg0, 4, 8, 0xD110, 0);
    assert(at(4, 8) == 0);

    /* sub_08046E5C: digits leftwards from x; blanks (base + 0xD) replace the
     * leading zeros unless padded; a longer value overflows the width. */
    memset(bg0.map, 0, sizeof bg0.map);
    assert(aos_menu_bg0_put_number(&bg0, 10, 1, 7, 3, 0xD100, false) == 3);
    assert(at(10, 1) == 0xD107 && at(9, 1) == 0xD10D && at(8, 1) == 0xD10D);
    assert(aos_menu_bg0_put_number(&bg0, 10, 2, 7, 3, 0xD100, true) == 3);
    assert(at(9, 2) == 0xD100 && at(8, 2) == 0xD100);
    assert(aos_menu_bg0_put_number(&bg0, 10, 3, 0, 2, 0xD100, false) == 2);
    assert(at(10, 3) == 0xD100 && at(9, 3) == 0xD10D);
    assert(aos_menu_bg0_put_number(&bg0, 10, 4, 12345, 2, 0xD100, false) == 5);
    assert(at(10, 4) == 0xD105 && at(6, 4) == 0xD101);
    assert(aos_menu_bg0_put_number(&bg0, 10, 5, -4, 2, 0xD100, false) == 1);
    assert(at(10, 5) == 0xD100 && at(9, 5) == 0);

    /* The setup loop: text tile 0x20 at map cell (2, 15) with palette 12,
     * running linearly through the next rows. */
    memset(bg0.map, 0, sizeof bg0.map);
    aos_menu_bg0_map_text(&bg0);
    assert(at(2, 15) == 0xC020 && at(31, 15) == 0xC03D && at(0, 16) == 0xC03E &&
           at(1, 19) == 0xC09F && at(2, 19) == 0 && at(1, 15) == 0);

    /* Pixels: 4-bpp tiles, flips and the entry's palette row; index 0 is
     * transparent. */
    memset(bg0.chars, 0, sizeof bg0.chars);
    bg0.chars[0x21 * 32] = 0x31;            /* tile 0x21, row 0: pixels 1 and 3 */
    bg0.palette[(12 * 16 + 1) * 2] = 0x1F;  /* palette 12 colour 1: red */
    bg0.palette[(12 * 16 + 3) * 2 + 1] = 0x7C; /* colour 3: blue */
    bg0.map[0] = 0xC021;
    uint16_t color = 0;
    assert(aos_menu_bg0_pixel(&bg0, 0, 0, &color) && color == 0x001F);
    assert(aos_menu_bg0_pixel(&bg0, 1, 0, &color) && color == 0x7C00);
    assert(!aos_menu_bg0_pixel(&bg0, 2, 0, &color));
    bg0.map[0] = 0xC421; /* horizontal flip */
    assert(aos_menu_bg0_pixel(&bg0, 7, 0, &color) && color == 0x001F);
    bg0.map[0] = 0xC821; /* vertical flip */
    assert(aos_menu_bg0_pixel(&bg0, 0, 7, &color) && color == 0x001F);
    bg0.map[0] = 0x0021; /* palette 0 is black here: opaque, colour 0 */
    assert(aos_menu_bg0_pixel(&bg0, 0, 0, &color) && color == 0);

    /* Text tiles come from the canvas' first 0x2000 bytes. */
    AosIntroTextTiles text;
    memset(&text, 0x11, sizeof text);
    aos_menu_bg0_set_text(&bg0, &text);
    assert(bg0.chars[0x1FFF] == 0x11 && bg0.chars[0x2000] == 0);

    /* Optional local check with the verified ROM: each language loads a
     * non-empty bank and opaque digits (tiles 0x100-0x109, palette 13). */
    if (argc == 2) {
        FusionRom rom = {0};
        char error[256];
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &rom, error, sizeof error));
        for (uint8_t language = 1; language <= 3; ++language) {
            assert(aos_menu_bg0_init(&bg0, &rom, language, error, sizeof error));
            for (unsigned digit = 0; digit < 10; ++digit) {
                bool opaque = false;
                for (size_t byte = 0; byte < 32; ++byte)
                    opaque |= bg0.chars[(0x100 + digit) * 32 + byte] != 0;
                assert(opaque);
            }
            uint16_t text_color = (uint16_t)(bg0.palette[(12 * 16 + 8) * 2] |
                                             bg0.palette[(12 * 16 + 8) * 2 + 1] << 8);
            assert(text_color != 0);
        }
        assert(!aos_menu_bg0_init(&bg0, &rom, 4, error, sizeof error));
        fusion_rom_close(&rom);
    }
    puts("Aria main-menu BG0 tests passed");
    return 0;
}
