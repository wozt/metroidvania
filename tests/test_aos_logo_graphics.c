/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's startup logos: the brightness effects of aos_logo_render, and
 * (optional local check with the ROM path) the decoded layers. */
#include "aos/title/aos_logo_graphics.h"

#include <assert.h>
#include <stdio.h>

static const uint8_t *pixel(const uint8_t *rgba, int x, int y) {
    return rgba + ((size_t)y * AOS_LOGO_W + (size_t)x) * 4;
}

int main(int argc, char **argv) {
    static AosLogoGraphics g;
    static uint8_t rgba[AOS_LOGO_W * AOS_LOGO_H * 4];
    g.palette[0] = 0x0000;                       /* backdrop: black */
    g.palette[0x13] = 0x001F;                    /* row 1 colour 3: red */
    g.bg0[10 * AOS_LOGO_LAYER + 20] = 0x13;
    /* No effect: the colours as they are. */
    aos_logo_render(&g, 0x003F, 16, rgba);
    assert(pixel(rgba, 20, 10)[0] == 255 && pixel(rgba, 20, 10)[1] == 0);
    assert(pixel(rgba, 0, 0)[0] == 0 && pixel(rgba, 0, 0)[3] == 255);
    /* BLDY 16 of the increase (Nintendo, the Konami fade-in): all white. */
    aos_logo_render(&g, 0x00BF, 16, rgba);
    assert(pixel(rgba, 20, 10)[1] == 255 && pixel(rgba, 0, 0)[2] == 255);
    /* BLDY 8 of the decrease (the Konami fade-out): 31 - 31 * 8 / 16 = 16. */
    aos_logo_render(&g, 0x00FF, 8, rgba);
    assert(pixel(rgba, 20, 10)[0] == 16 * 255 / 31 && pixel(rgba, 0, 0)[0] == 0);
    /* BLDY 0: the logo itself. */
    aos_logo_render(&g, 0x00BF, 0, rgba);
    assert(pixel(rgba, 20, 10)[0] == 255 && pixel(rgba, 20, 10)[2] == 0);
    /* Only the targets of BLDCNT bits 0 (BG0) and 5 (backdrop). */
    aos_logo_render(&g, 0x00A0, 16, rgba);
    assert(pixel(rgba, 20, 10)[1] == 0 && pixel(rgba, 0, 0)[1] == 255);
    char error[128];
    assert(!aos_logo_graphics_init(&g, NULL, AOS_LOGO_NINTENDO, error, sizeof error));

    if (argc == 2) {
        FusionRom rom = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &rom, error, sizeof error));
        for (int logo = AOS_LOGO_NINTENDO; logo <= AOS_LOGO_KONAMI; ++logo) {
            assert(aos_logo_graphics_init(&g, &rom, (AosLogo)logo, error, sizeof error));
            size_t drawn = 0;
            for (int y = 0; y < AOS_LOGO_H; ++y)
                for (int x = 0; x < AOS_LOGO_W; ++x) drawn += g.bg0[y * AOS_LOGO_LAYER + x] != 0;
            printf("logo %d: %zu pixels, backdrop %04X\n", logo, drawn, g.palette[0]);
            assert(drawn > 500);
        }
        fusion_rom_close(&rom);
    }
    puts("Aria logo graphics tests passed");
    return 0;
}
