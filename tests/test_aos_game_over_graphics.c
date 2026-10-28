/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_over_graphics.h"

#include <assert.h>
#include <stdio.h>

int main(int argc, char **argv) {
    AosGameOverGraphics graphics;
    char error[256];
    /* Without a ROM the request is refused. */
    FusionRom empty = {0};
    assert(!aos_game_over_graphics_init(&graphics, &empty, 1, error, sizeof error));
    assert(!aos_game_over_graphics_init(&graphics, NULL, 1, error, sizeof error));
    /* Optional local check with the verified ROM: the 256-colour screen and
     * each language's sprite render. */
    if (argc == 2) {
        FusionRom rom = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &rom, error, sizeof error));
        for (uint8_t language = 1; language <= 3; ++language) {
            if (!aos_game_over_graphics_init(&graphics, &rom, language, error, sizeof error)) {
                fprintf(stderr, "language %u: %s\n", language, error);
                assert(0);
            }
            assert(graphics.background.width_pixels >= 240 &&
                   graphics.background.height_pixels >= 160 &&
                   graphics.background.unresolved_count == 0 && graphics.sprite_bmp_size > 54);
            size_t opaque = 0;
            for (size_t i = 0; i < graphics.background.pixel_count; ++i)
                opaque += graphics.background.rgba[i * 4 + 3] != 0;
            assert(opaque > graphics.background.pixel_count / 4);
            aos_game_over_graphics_free(&graphics);
        }
        fusion_rom_close(&rom);
    }
    puts("Aria game-over graphics tests passed");
    return 0;
}
