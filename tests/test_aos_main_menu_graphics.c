/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_main_menu_graphics.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_SIZE = 0x510000,
    PALETTE_OFFSET = 0x0E5E24,
    GRAPHICS_FIRST_OFFSET = 0x0E5BB0,
    GRAPHICS_SECOND_OFFSET = 0x0E5BB8,
    GRAPHICS_LANGUAGE_OFFSET = 0x0E5BC0,
    COMPRESSED_FIRST_OFFSET = 0x1000,
    COMPRESSED_SECOND_OFFSET = 0x4000,
    COMPRESSED_LANGUAGE_OFFSET = 0x7000,
    BLOCKS_OFFSET = 0xA000,
    MAP_OFFSET = 0xA100,
    GRAPHICS_SIZE = 0x2000,
};

static const size_t metadata_offsets[] = {
    0x0E73A4, 0x0E7434, 0x0E74C4,
    0x0E7314, 0x0E7044,
    0x0E7284, 0x0E6FB4,
    0x0E71F4, 0x0E6E04,
    0x0E70D4, 0x0E6E94,
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

static void write_literal_lz77(uint8_t *destination,
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
    write_u16(data, PALETTE_OFFSET + 6, 31);

    decoded[1] = 4;
    decoded[2] = 0x20;
    memset(decoded + 4, 0x11, 32);
    write_literal_lz77(data + COMPRESSED_FIRST_OFFSET, decoded,
                       GRAPHICS_SIZE + 4);
    memset(decoded + 4, 0, GRAPHICS_SIZE);
    write_literal_lz77(data + COMPRESSED_SECOND_OFFSET, decoded,
                       GRAPHICS_SIZE + 4);
    write_literal_lz77(data + COMPRESSED_LANGUAGE_OFFSET, decoded,
                       GRAPHICS_SIZE + 4);
    graphics_descriptor(data, GRAPHICS_FIRST_OFFSET,
                        COMPRESSED_FIRST_OFFSET);
    graphics_descriptor(data, GRAPHICS_SECOND_OFFSET,
                        COMPRESSED_SECOND_OFFSET);
    graphics_descriptor(data, GRAPHICS_LANGUAGE_OFFSET,
                        COMPRESSED_LANGUAGE_OFFSET);
    for (size_t index = 0;
         index < sizeof metadata_offsets / sizeof metadata_offsets[0];
         ++index)
        background_metadata(data, metadata_offsets[index]);
    for (unsigned index = 0; index < 64; ++index)
        write_u16(data, MAP_OFFSET + index * 2, 1);

    FusionRom rom = {
        .data = data,
        .size = ROM_SIZE,
        .game = FUSION_ASSET_GAME_ARIA,
    };
    AosMainMenuGraphics graphics;
    char error[160];
    assert(aos_main_menu_graphics_init(&graphics, &rom, 1,
                                       error, sizeof error));
    assert(graphics.language == 1 && graphics.backdrop_rgb[1] == 255);
    for (unsigned screen = 0; screen < AOS_MAIN_GRAPHICS_SCREEN_COUNT;
         ++screen) {
        for (unsigned layer = 0; layer < AOS_MAIN_GRAPHICS_LAYER_COUNT;
             ++layer) {
            const FusionAosRenderedBackground *background =
                &graphics.backgrounds[screen][layer];
            assert(background->width_pixels == 256 &&
                   background->height_pixels == 256 &&
                   background->unresolved_count == 0);
            assert(background->rgba[0] == 255 &&
                   background->rgba[3] == 255);
        }
    }
    aos_main_menu_graphics_free(&graphics);
    assert(!aos_main_menu_graphics_init(&graphics, &rom, 0,
                                        error, sizeof error));
    free(decoded);
    free(data);

    /* Optional local smoke check validates each language in memory only. */
    if (argc == 2) {
        FusionRom local = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &local,
                               error, sizeof error));
        for (uint8_t language = 1; language <= 3; ++language) {
            assert(aos_main_menu_graphics_init(
                &graphics, &local, language, error, sizeof error));
            for (unsigned screen = 0;
                 screen < AOS_MAIN_GRAPHICS_SCREEN_COUNT; ++screen)
                for (unsigned layer = 0;
                     layer < AOS_MAIN_GRAPHICS_LAYER_COUNT; ++layer)
                    assert(graphics.backgrounds[screen][layer]
                               .unresolved_count == 0);
            aos_main_menu_graphics_free(&graphics);
        }
        fusion_rom_close(&local);
    }
    puts("Aria main-menu graphics tests passed");
    return 0;
}
