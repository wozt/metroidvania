/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_portrait.h"

#include "aos_rom_graphics.h"

#include <stdio.h>
#include <string.h>

enum { AOS_DIALOGUE_PALETTE = 0x0826664C };

static const uint32_t portrait_graphics[AOS_PORTRAIT_COUNT] = {
    0x0815E03C, 0x0815E04C, 0x0815E054, 0x0815E05C,
    0x0815E074, 0x0815E07C, 0x0815E06C, 0x0815E044,
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

uint32_t aos_portrait_graphics_pointer(uint8_t portrait) {
    return portrait < AOS_PORTRAIT_COUNT ? portrait_graphics[portrait] : 0;
}

static uint8_t expand_channel(unsigned value) {
    return (uint8_t)((value * 255u) / 31u);
}

bool aos_portrait_decode(const FusionRom *rom, uint8_t portrait,
                         AosPortraitImage *image, char *error, size_t error_size) {
    if (!image || portrait >= AOS_PORTRAIT_COUNT) {
        set_error(error, error_size, "invalid Aria dialogue portrait");
        return false;
    }
    uint8_t tiles[0x2000];
    uint8_t palette[0x200];
    if (!aos_rom_load_graphics_resource(rom, portrait_graphics[portrait],
                                        tiles, sizeof tiles, error, error_size) ||
        !aos_rom_load_palette_resource(rom, AOS_DIALOGUE_PALETTE, palette,
                                       error, error_size))
        return false;
    memset(image, 0, sizeof *image);
    for (unsigned y = 0; y < AOS_PORTRAIT_HEIGHT; ++y) {
        for (unsigned x = 0; x < AOS_PORTRAIT_WIDTH; ++x) {
            size_t tile = ((size_t)(y / 8) * 16 + x / 8) * 32;
            uint8_t packed = tiles[tile + (y % 8) * 4 + (x % 8) / 2];
            unsigned index = (packed >> ((x & 1u) * 4u)) & 0x0Fu;
            uint16_t color = (uint16_t)palette[index * 2] |
                             ((uint16_t)palette[index * 2 + 1] << 8);
            uint8_t *pixel = &image->rgba[(y * AOS_PORTRAIT_WIDTH + x) * 4];
            pixel[0] = expand_channel(color & 31u);
            pixel[1] = expand_channel((color >> 5) & 31u);
            pixel[2] = expand_channel((color >> 10) & 31u);
            pixel[3] = index == 0 ? 0 : 255;
        }
    }
    return true;
}
