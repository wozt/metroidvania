/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_window_frame.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosWindowFrame frame;

static uint16_t at(int x, int y) {
    return frame.map[y * AOS_WINDOW_FRAME_MAP_SIZE + x];
}

int main(int argc, char **argv) {
    /* sub_0805AFC0's strip loop: four rows of 0x1D entries at x = 2,
     * y = 16..19, bases 0xF060 + r * 0x20. */
    aos_window_frame_build(&frame);
    for (unsigned r = 0; r < 4; ++r) {
        assert(at(2, 16 + r) == 0xF060 + r * 0x20);
        assert(at(2 + 0x1C, 16 + r) == 0xF060 + r * 0x20 + 0x1C);
        assert(at(2 + 0x1D, 16 + r) == 0);
        assert(at(1, 16 + r) == 0);
    }
    assert(at(2, 15) == 0 && at(2, 20) == 0);

    /* Pixels: 4-bpp tiles, flips and the entry's palette row; index 0 is
     * transparent. */
    memset(frame.tiles, 0, sizeof frame.tiles);
    frame.tiles[0x21 * 32] = 0x31;             /* tile 0x21, row 0 */
    frame.palette[(15 * 16 + 1) * 2] = 0x1F;   /* palette 15 colour 1 */
    frame.palette[(15 * 16 + 3) * 2 + 1] = 0x7C;
    frame.map[0] = 0xF021;
    uint16_t color = 0;
    assert(aos_window_frame_pixel(&frame, 0, 0, &color) && color == 0x001F);
    assert(aos_window_frame_pixel(&frame, 1, 0, &color) && color == 0x7C00);
    assert(!aos_window_frame_pixel(&frame, 2, 0, &color));
    frame.map[0] = 0xF421; /* horizontal flip */
    assert(aos_window_frame_pixel(&frame, 7, 0, &color) && color == 0x001F);
    assert(aos_window_frame_entry_pixel(&frame, 0xF021, 0, 0, &color) &&
           color == 0x001F);
    assert(!aos_window_frame_entry_pixel(&frame, 0xF021, 8, 0, &color));

    /* Optional local check with the verified ROM: every language loads a
     * non-empty window sheet and the dialogue palette row. */
    if (argc == 2) {
        FusionRom rom = {0};
        char error[256];
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &rom,
                               error, sizeof error));
        for (uint8_t language = 1; language <= 3; ++language) {
            assert(aos_window_frame_init(&frame, &rom, language,
                                         error, sizeof error));
            bool opaque = false;
            for (size_t byte = 0x60 * 32; byte < 0x61 * 32; ++byte)
                opaque |= frame.tiles[byte] != 0;
            assert(opaque);
            assert(frame.palette[15 * 32] != 0 ||
                   frame.palette[15 * 32 + 1] != 0);
            /* sub_08067A04 alternates these native 8x16 marker pairs while
             * waiting for A. Each pair must contain visible sheet pixels. */
            for (unsigned variant = 0; variant < 2; ++variant) {
                bool marker = false;
                for (unsigned y = 0; y < 16; ++y)
                    for (unsigned x = 0; x < 8; ++x)
                        if (aos_window_frame_entry_pixel(
                            &frame, (uint16_t)(0xF0CE + variant +
                                              (y >= 8 ? 0x10 : 0)),
                            x, y & 7, &color)) {
                            marker = true;
                        }
                assert(marker);
            }
        }
        assert(!aos_window_frame_init(&frame, &rom, 4, error, sizeof error));
        fusion_rom_close(&rom);
    }
    puts("Aria window frame tests passed");
    return 0;
}
