/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's startup logos: see aos_logo_graphics.h. */
#include "aos_logo_graphics.h"

#include "aos_rom_graphics.h"
#include "aos_title_graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
    uint32_t graphics, palette, bg0;
    bool whole_palette;   /* sub_0803C8B0, else row 0 only (sub_0803C918) */
} logos[2] = {
    [AOS_LOGO_NINTENDO] = {0x080E5C00, 0x080E6838, 0x080E94F4, true},
    [AOS_LOGO_KONAMI] = {0x080E5C18, 0x080E6C40, 0x080E9614, false},
};

bool aos_logo_graphics_init(AosLogoGraphics *graphics, const FusionRom *rom, AosLogo logo,
                            char *error, size_t error_size) {
    if (!graphics || !rom || rom->game != FUSION_ASSET_GAME_ARIA ||
        (logo != AOS_LOGO_NINTENDO && logo != AOS_LOGO_KONAMI)) {
        if (error && error_size) snprintf(error, error_size, "invalid Aria logo request");
        return false;
    }
    memset(graphics, 0, sizeof *graphics);
    enum { VRAM_SIZE = 0x10000 };
    uint8_t *vram = calloc(1, VRAM_SIZE);
    uint8_t palette[0x200] = {0};
    bool ok = vram &&
              aos_rom_load_graphics_resource(rom, logos[logo].graphics, vram, VRAM_SIZE, error,
                                             error_size) &&
              (logos[logo].whole_palette
                   ? aos_rom_load_palette_resource(rom, logos[logo].palette, palette, error,
                                                   error_size)
                   : aos_rom_load_palette_range(rom, logos[logo].palette, 0, 1, 0, palette,
                                                error, error_size)) &&
              aos_title_decode_text_layer(graphics->bg0, rom, logos[logo].bg0, 0, vram, error,
                                          error_size);
    free(vram);
    if (!ok) {
        if (error && error_size && !error[0]) snprintf(error, error_size, "Aria logo graphics");
        return false;
    }
    for (int k = 0; k < 256; ++k)
        graphics->palette[k] = (uint16_t)(palette[k * 2] | palette[k * 2 + 1] << 8);
    if (error && error_size) error[0] = '\0';
    return true;
}

void aos_logo_render(const AosLogoGraphics *graphics, uint16_t blend_control, uint16_t brightness,
                     uint8_t rgba[AOS_LOGO_W * AOS_LOGO_H * 4]) {
    int effect = (blend_control >> 6) & 3;
    int evy = brightness > 16 ? 16 : brightness;
    for (int y = 0; y < AOS_LOGO_H; ++y)
        for (int x = 0; x < AOS_LOGO_W; ++x) {
            uint8_t index = graphics->bg0[y * AOS_LOGO_LAYER + x];
            uint16_t c = graphics->palette[index];
            /* BG0 is first target bit 0, the backdrop bit 5. */
            bool first = (blend_control >> (index ? 0 : 5)) & 1;
            int ch[3] = {c & 31, (c >> 5) & 31, (c >> 10) & 31};
            for (int k = 0; k < 3 && first; ++k) {
                if (effect == 2) ch[k] += ((31 - ch[k]) * evy) >> 4;
                else if (effect == 3) ch[k] -= (ch[k] * evy) >> 4;
            }
            uint8_t *out = rgba + ((size_t)y * AOS_LOGO_W + (size_t)x) * 4;
            for (int k = 0; k < 3; ++k) out[k] = (uint8_t)(ch[k] * 255 / 31);
            out[3] = 255;
        }
}
