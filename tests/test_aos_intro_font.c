/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_font.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    ROM_SIZE = 0x510000,
    METRICS_OFFSET = 0x0E2B4C,
    GLYPHS_OFFSET = 0x50A136,
    GLYPH_A = 'A' - 0x20,
    GLYPH_B = 'B' - 0x20,
};

int main(int argc, char **argv) {
    uint8_t *data = calloc(1, ROM_SIZE);
    assert(data);
    uint8_t *metric = data + METRICS_OFFSET + GLYPH_A * 4;
    metric[0] = 1;
    metric[1] = 3;
    uint8_t *bitmap = data + GLYPHS_OFFSET + GLYPH_A * 14;
    bitmap[0] = 0x70;
    bitmap[1] = 0x50;
    bitmap[8] = 0x40;
    uint8_t *metric_b = data + METRICS_OFFSET + GLYPH_B * 4;
    metric_b[1] = 8;
    uint8_t *bitmap_b = data + GLYPHS_OFFSET + GLYPH_B * 14;
    bitmap_b[0] = 0xFF;

    FusionRom rom = {
        .data = data,
        .size = ROM_SIZE,
        .game = FUSION_ASSET_GAME_ARIA,
    };
    AosIntroFont font;
    char error[128];
    assert(aos_intro_font_init(&font, &rom, error, sizeof error));

    AosIntroTextTiles tiles;
    aos_intro_text_tiles_clear(&tiles);
    const uint8_t line[] = {'A'};
    assert(aos_intro_font_render_line(&font, &tiles, 0, line, sizeof line));
    assert(aos_intro_text_tiles_pixel(&tiles, 24, 0) == 8);
    assert(aos_intro_text_tiles_pixel(&tiles, 25, 0) == 8);
    assert(aos_intro_text_tiles_pixel(&tiles, 26, 0) == 8);
    assert(aos_intro_text_tiles_pixel(&tiles, 24, 1) == 8);
    assert(aos_intro_text_tiles_pixel(&tiles, 25, 1) == 0);
    assert(aos_intro_text_tiles_pixel(&tiles, 26, 1) == 8);
    assert(aos_intro_text_tiles_pixel(&tiles, 24, 8) == 8);
    assert(tiles.data[96] == 0x88 && tiles.data[97] == 0x08);
    assert(tiles.data[100] == 0x08 && tiles.data[101] == 0x08);

    /* The native clear covers cells 4..40 (222 pixels) and leaves adjacent
     * pixels untouched. */
    tiles.data[2 * 32 + 3 * 4 + 3] = 0x80;
    tiles.data[30 * 32 + 3 * 4 + 3] = 0x08;
    assert(aos_intro_text_tiles_pixel(&tiles, 23, 3) == 8);
    assert(aos_intro_text_tiles_pixel(&tiles, 246, 3) == 8);
    assert(aos_intro_font_render_line(&font, &tiles, 0, NULL, 0));
    assert(aos_intro_text_tiles_pixel(&tiles, 23, 3) == 8);
    assert(aos_intro_text_tiles_pixel(&tiles, 246, 3) == 8);
    assert(aos_intro_text_tiles_pixel(&tiles, 24, 0) == 0);

    /* The second glyph crosses a horizontal tile boundary exactly as the
     * native split writer does. */
    const uint8_t split_line[] = {'A', 'B'};
    assert(aos_intro_font_render_line(&font, &tiles, 0, split_line,
                                      sizeof split_line));
    for (unsigned x = 27; x < 35; ++x)
        assert(aos_intro_text_tiles_pixel(&tiles, x, 0) == 8);

    rom.game = FUSION_ASSET_GAME_METROID;
    assert(!aos_intro_font_init(&font, &rom, error, sizeof error));
    free(data);

    /* Optional local smoke check. CTest remains synthetic and never exports
     * glyph data from the verified user-provided ROM. */
    if (argc == 2) {
        FusionRom local = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &local,
                               error, sizeof error));
        assert(aos_intro_font_init(&font, &local, error, sizeof error));
        aos_intro_text_tiles_clear(&tiles);
        assert(aos_intro_font_render_line(&font, &tiles, 0, line,
                                          sizeof line));
        bool found = false;
        for (unsigned y = 0; y < 12; ++y)
            for (unsigned x = 24; x < 32; ++x)
                found |= aos_intro_text_tiles_pixel(&tiles, x, y) != 0;
        assert(found);
        fusion_rom_close(&local);
    }
    puts("Aria introduction font tests passed");
    return 0;
}
