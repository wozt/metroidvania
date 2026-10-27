/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_graphics.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_SIZE = 0x510000,
    PALETTE_OFFSET = 0x0E6634,
    GRAPHICS_FIRST_OFFSET = 0x0E5BF0,
    GRAPHICS_SECOND_OFFSET = 0x0E5BF8,
    BG1_OFFSET = 0x0E9464,
    BG2_OFFSET = 0x0E9244,
    BG3_OFFSET = 0x0E9354,
    COMPRESSED_FIRST_OFFSET = 0x1000,
    COMPRESSED_SECOND_OFFSET = 0x4000,
    BLOCKS_OFFSET = 0x7000,
    MAP_OFFSET = 0x7100,
    GRAPHICS_SIZE = 0x2000,
};

static void write_u16(uint8_t *data, size_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

static size_t write_literal_lz77(uint8_t *destination,
                                 const uint8_t *source, size_t size) {
    destination[0] = 0x10;
    destination[1] = (uint8_t)size;
    destination[2] = (uint8_t)(size >> 8);
    destination[3] = (uint8_t)(size >> 16);
    size_t input = 0, output = 4;
    while (input < size) {
        destination[output++] = 0;
        size_t count = size - input < 8 ? size - input : 8;
        memcpy(destination + output, source + input, count);
        input += count;
        output += count;
    }
    return output;
}

static void graphics_descriptor(uint8_t *data, size_t offset,
                                size_t compressed_offset) {
    data[offset] = 1;
    data[offset + 1] = 4;
    data[offset + 2] = 0x10;
    data[offset + 3] = 0x10;
    write_u32(data, offset + 4, 0x08000000u + (uint32_t)compressed_offset);
}

static void background_metadata(uint8_t *data, size_t offset) {
    data[offset] = 1;
    data[offset + 1] = 1;
    write_u32(data, offset + 4, 0x08000000u + BLOCKS_OFFSET);
    write_u32(data, offset + 12, 0x08000000u + MAP_OFFSET);
}

int main(int argc, char **argv) {
    uint8_t *data = calloc(1, ROM_SIZE);
    uint8_t *decoded = calloc(1, GRAPHICS_SIZE + 4);
    assert(data && decoded);

    data[PALETTE_OFFSET + 2] = 16;
    write_u16(data, PALETTE_OFFSET + 4, 31u << 5);
    write_u16(data, PALETTE_OFFSET + 4 + 2, 31);
    write_u16(data, PALETTE_OFFSET + 4 + (15 * 16 + 8) * 2,
              31u << 10);
    decoded[1] = 4;
    decoded[2] = 0x10;
    decoded[3] = 0x10;
    memset(decoded + 4, 0x11, 32);
    write_literal_lz77(data + COMPRESSED_FIRST_OFFSET, decoded,
                       GRAPHICS_SIZE + 4);
    memset(decoded + 4, 0, GRAPHICS_SIZE);
    write_literal_lz77(data + COMPRESSED_SECOND_OFFSET, decoded,
                       GRAPHICS_SIZE + 4);
    graphics_descriptor(data, GRAPHICS_FIRST_OFFSET,
                        COMPRESSED_FIRST_OFFSET);
    graphics_descriptor(data, GRAPHICS_SECOND_OFFSET,
                        COMPRESSED_SECOND_OFFSET);
    background_metadata(data, BG1_OFFSET);
    background_metadata(data, BG2_OFFSET);
    background_metadata(data, BG3_OFFSET);
    for (unsigned index = 0; index < 64; ++index)
        write_u16(data, MAP_OFFSET + index * 2, 1);

    FusionRom rom = {
        .data = data,
        .size = ROM_SIZE,
        .game = FUSION_ASSET_GAME_ARIA,
    };
    AosIntroGraphics graphics;
    char error[160];
    assert(aos_intro_graphics_init(&graphics, &rom, error, sizeof error));
    assert(graphics.backdrop_rgb[0] == 0 &&
           graphics.backdrop_rgb[1] == 255 &&
           graphics.backdrop_rgb[2] == 0);
    assert(graphics.text_rgb[0] == 0 && graphics.text_rgb[1] == 0 &&
           graphics.text_rgb[2] == 255);
    for (unsigned index = 0; index < AOS_INTRO_BACKGROUND_COUNT; ++index) {
        const FusionAosRenderedBackground *background =
            &graphics.backgrounds[index];
        assert(background->width_pixels == 256 &&
               background->height_pixels == 256 &&
               background->unresolved_count == 0);
        assert(background->rgba[0] == 255 && background->rgba[1] == 0 &&
               background->rgba[2] == 0 && background->rgba[3] == 255);
    }
    aos_intro_graphics_free(&graphics);

    rom.game = FUSION_ASSET_GAME_METROID;
    assert(!aos_intro_graphics_init(&graphics, &rom, error, sizeof error));
    free(decoded);
    free(data);

    /* Optional local smoke check loads but never writes ROM-derived pixels. */
    if (argc == 2) {
        FusionRom local = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &local,
                               error, sizeof error));
        if (!aos_intro_graphics_init(&graphics, &local, error, sizeof error)) {
            fprintf(stderr, "Local Aria introduction graphics: %s\n", error);
            assert(!"local Aria introduction graphics smoke check failed");
        }
        assert(graphics.backgrounds[0].width_pixels == 256 &&
               graphics.backgrounds[0].height_pixels == 512);
        assert(graphics.backgrounds[1].width_pixels == 256 &&
               graphics.backgrounds[1].height_pixels == 768);
        assert(graphics.backgrounds[2].width_pixels == 256 &&
               graphics.backgrounds[2].height_pixels == 512);
        aos_intro_graphics_free(&graphics);
        fusion_rom_close(&local);
    }
    puts("Aria introduction graphics tests passed");
    return 0;
}
