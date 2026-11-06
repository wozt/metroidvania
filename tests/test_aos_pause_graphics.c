/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_pause_graphics.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_SIZE = 0x27C000,
    PALETTE_OFFSET = 0x0E5C20,
    GRAPHICS_OFFSET = 0x15E02C,
    METADATA_OFFSET = 0x15E940,
    LANGUAGE_OFFSET = 0x27B200,
    COMPRESSED_GRAPHICS = 0x1000,
    COMPRESSED_LANGUAGE = 0x4000,
    BLOCKS_OFFSET = 0x8000,
    MAP_OFFSET = 0x8100,
    GRAPHICS_SIZE = 0x2000,
};

static void write_u16(uint8_t *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
    bytes[offset + 2] = (uint8_t)(value >> 16);
    bytes[offset + 3] = (uint8_t)(value >> 24);
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

static void graphics_descriptor(uint8_t *bytes, size_t offset,
                                size_t compressed) {
    bytes[offset] = 1;
    bytes[offset + 1] = 4;
    bytes[offset + 2] = 0x10;
    bytes[offset + 3] = 0x10;
    write_u32(bytes, offset + 4, 0x08000000u + (uint32_t)compressed);
}

static void write_ppm(const char *path, const uint8_t *rgba) {
    FILE *out = fopen(path, "wb");
    assert(out);
    fputs("P6\n240 160\n255\n", out);
    for (size_t pixel = 0; pixel < 240 * 160; ++pixel)
        assert(fwrite(rgba + pixel * 4, 1, 3, out) == 3);
    assert(fclose(out) == 0);
}

int main(int argc, char **argv) {
    uint8_t *bytes = calloc(1, ROM_SIZE);
    uint8_t *decoded = calloc(1, GRAPHICS_SIZE + 4);
    uint8_t *rgba = malloc(240 * 160 * 4);
    assert(bytes && decoded && rgba);
    bytes[PALETTE_OFFSET] = 0;
    bytes[PALETTE_OFFSET + 2] = 16;
    bytes[PALETTE_OFFSET + 3] = 0;
    write_u16(bytes, PALETTE_OFFSET + 4 + 1 * 2, 0x03E0);
    write_u16(bytes, PALETTE_OFFSET + 4 + (3 * 16 + 1) * 2, 0x001F);
    write_u16(bytes, PALETTE_OFFSET + 4 + (7 * 16 + 1) * 2, 0x7C00);

    decoded[1] = 4;
    decoded[2] = 0x20;
    memset(decoded + 4 + 32, 0x11, 32);
    write_literal_lz77(bytes + COMPRESSED_GRAPHICS, decoded,
                       GRAPHICS_SIZE + 4);
    memset(decoded + 4, 0, GRAPHICS_SIZE);
    memset(decoded + 4 + 0x40 * 32, 0x11, 7 * 0x10 * 32);
    write_literal_lz77(bytes + COMPRESSED_LANGUAGE, decoded,
                       GRAPHICS_SIZE + 4);
    graphics_descriptor(bytes, GRAPHICS_OFFSET, COMPRESSED_GRAPHICS);
    graphics_descriptor(bytes, LANGUAGE_OFFSET, COMPRESSED_LANGUAGE);

    bytes[METADATA_OFFSET] = 1;
    bytes[METADATA_OFFSET + 1] = 1;
    write_u32(bytes, METADATA_OFFSET + 4, 0x08000000u + BLOCKS_OFFSET);
    write_u32(bytes, METADATA_OFFSET + 12, 0x08000000u + MAP_OFFSET);
    for (unsigned cell = 0; cell < 16; ++cell)
        write_u16(bytes, BLOCKS_OFFSET + cell * 2, 1);
    for (unsigned block = 0; block < 64; ++block)
        write_u16(bytes, MAP_OFFSET + block * 2, 1);

    FusionRom rom = {bytes, ROM_SIZE, FUSION_ASSET_GAME_ARIA};
    AosPauseGraphics graphics;
    char error[160];
    assert(aos_pause_graphics_init(&graphics, &rom, 1, error, sizeof error));
    assert(graphics.background.width_pixels == 256 &&
           graphics.background.height_pixels == 256 &&
           graphics.background.unresolved_count == 0);
    AosPauseHubValues values = {
        .hp = 320, .mp = 80, .max_hp = 320, .max_mp = 80,
        .stats = {10, 12, 11, 9}, .attack = 15, .defence = 11,
        .level = 1, .experience = 84, .next_experience = 0,
        .play_frames = 60 * 3723,
    };
    assert(aos_pause_graphics_render_hub(&graphics, &values, 0,
                                         rgba, 240 * 160 * 4));
    assert((graphics.bg0_map[12 * 32 + 2] & 0xF000) == 0x3000);
    assert((graphics.bg0_map[13 * 32 + 2] & 0xF000) == 0x7000);
    assert(graphics.bg0_map[2 * 32 + 14] == 0x3300);
    aos_pause_graphics_free(&graphics);
    free(rgba);
    free(decoded);
    free(bytes);

    if (argc >= 2) {
        FusionRom local = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &local,
                               error, sizeof error));
        assert(aos_pause_graphics_init(&graphics, &local, 1,
                                       error, sizeof error));
        rgba = malloc(240 * 160 * 4);
        assert(rgba);
        assert(aos_pause_graphics_render_hub(&graphics, &values, 0,
                                             rgba, 240 * 160 * 4));
        if (argc >= 3) write_ppm(argv[2], rgba);
        free(rgba);
        aos_pause_graphics_free(&graphics);
        fusion_rom_close(&local);
    }
    puts("Aria pause graphics tests passed");
    return 0;
}
