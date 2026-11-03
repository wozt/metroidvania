/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's title-screen backgrounds: see aos_title_graphics.h. */
#include "aos_title_graphics.h"

#include "aos_rom_graphics.h"
#include "fusion/assets/gba.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TITLE_PALETTE = 0x080E6430,
    TITLE_GRAPHICS_BLOCK0 = 0x080E5BE0,
    TITLE_GRAPHICS_BLOCK2 = 0x080E5BE8,
    TITLE_BG0 = 0x080E9024,
    TITLE_BG1 = 0x080E90B4,
    TITLE_BG2 = 0x080E8F94,
    CHARACTER_BLOCK = 0x4000,
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

static uint16_t read_u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static const uint8_t *rom_at(const FusionRom *rom, uint32_t pointer, size_t size) {
    if (pointer < 0x08000000u) return NULL;
    size_t offset = pointer - 0x08000000u;
    if (offset > rom->size || size > rom->size - offset) return NULL;
    return rom->data + offset;
}

/* sub_0803F710 for a 4bpp layer on character block 2: each tile word's
 * index, flips and row into row * 16 + colour. */
static bool decode_text(uint8_t *out, const FusionRom *rom, uint32_t metadata,
                        const uint8_t *vram, char *error, size_t error_size) {
    FusionAosBackground background = {.control = 2 << 2, .metadata_pointer = metadata};
    FusionAosBackgroundTilemap tilemap = {0};
    bool ok = fusion_aos_read_background_tiles(rom, &background, &tilemap, error, error_size) &&
              tilemap.status == FUSION_AOS_TILEMAP_DECODED;
    for (size_t ty = 0; ok && ty < 32 && ty < tilemap.height_tiles; ++ty)
        for (size_t tx = 0; tx < 32 && tx < tilemap.width_tiles; ++tx) {
            uint16_t word = tilemap.cells[ty * tilemap.width_tiles + tx];
            const uint8_t *tile = vram + 2 * CHARACTER_BLOCK + (size_t)(word & 0x3FF) * 32;
            uint8_t row = (uint8_t)((word >> 12) & 0xF);
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    int sx = (word & 0x400) ? 7 - x : x, sy = (word & 0x800) ? 7 - y : y;
                    uint8_t c = (tile[sy * 4 + sx / 2] >> (4 * (sx & 1))) & 0xF;
                    if (c)
                        out[(ty * 8 + (size_t)y) * AOS_TITLE_LAYER_SIZE + tx * 8 + (size_t)x] =
                            (uint8_t)(row * 16 + c);
                }
        }
    fusion_aos_free_background_tilemap(&tilemap);
    if (!ok && error && error_size && !error[0]) set_error(error, error_size, "title text layer");
    return ok;
}

/* sub_0803F7C8 for BG2: the 8 x 8 block map of the metadata (+ 0x0C), each
 * block 4 rows of 4 tile bytes in the expanded block table (+ 0x04, LZ77
 * with a 4-byte header when + 0x02 bit 1), block 0 empty. */
static bool decode_affine(AosTitleGraphics *graphics, const FusionRom *rom, const uint8_t *vram,
                          char *error, size_t error_size) {
    const uint8_t *metadata = rom_at(rom, TITLE_BG2, 16);
    if (!metadata) {
        set_error(error, error_size, "title BG2 metadata outside the ROM");
        return false;
    }
    uint16_t flags = read_u16(metadata + 2);
    const uint8_t *map = rom_at(rom, read_u32(metadata + 12), 8 * 8 * 2);
    const uint8_t *blocks_rom = rom_at(rom, read_u32(metadata + 4), 4);
    if (!map || !blocks_rom || !(flags & 1)) {
        set_error(error, error_size, "title BG2 is not the expected affine layer");
        return false;
    }
    size_t rest = rom->size - (size_t)(blocks_rom - rom->data);
    uint8_t *expanded = NULL;
    const uint8_t *blocks = blocks_rom;
    size_t blocks_size = rest;
    if (flags & 2) {
        uint32_t size = fusion_gba_uncompressed_size(blocks_rom, rest);
        size_t written = 0;
        expanded = size >= 4 ? malloc(size) : NULL;
        if (!expanded || !fusion_gba_lz77_uncompress(blocks_rom, rest, expanded, size, &written) ||
            written < 4) {
            free(expanded);
            set_error(error, error_size, "title BG2 blocks do not expand");
            return false;
        }
        blocks = expanded + 4;
        blocks_size = written - 4;
    }
    for (int ty = 0; ty < 32; ++ty)
        for (int tx = 0; tx < 32; ++tx) {
            uint16_t block = read_u16(map + ((tx >> 2) + (ty >> 2) * 8) * 2);
            size_t at = (size_t)(block - 1) * 16 + (size_t)(ty & 3) * 4 + (size_t)(tx & 3);
            uint8_t tile = block && at < blocks_size ? blocks[at] : 0;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    graphics->affine[((size_t)ty * 8 + (size_t)y) * AOS_TITLE_LAYER_SIZE +
                                     (size_t)tx * 8 + (size_t)x] =
                        vram[(size_t)tile * 64 + (size_t)y * 8 + (size_t)x];
        }
    free(expanded);
    return true;
}

bool aos_title_graphics_init(AosTitleGraphics *graphics, const FusionRom *rom, char *error,
                             size_t error_size) {
    if (!graphics || !rom || rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "invalid Aria title graphics request");
        return false;
    }
    memset(graphics, 0, sizeof *graphics);
    FusionAosVideoMemory *memory = calloc(1, sizeof *memory);
    if (!memory) {
        set_error(error, error_size, "cannot allocate Aria title video memory");
        return false;
    }
    bool ok = aos_rom_load_palette_resource(rom, TITLE_PALETTE, memory->palette, error,
                                            error_size) &&
              aos_rom_load_graphics_resource(rom, TITLE_GRAPHICS_BLOCK0, memory->vram,
                                             sizeof memory->vram, error, error_size) &&
              aos_rom_load_graphics_resource(rom, TITLE_GRAPHICS_BLOCK2,
                                             memory->vram + 2 * CHARACTER_BLOCK,
                                             sizeof memory->vram - 2 * CHARACTER_BLOCK, error,
                                             error_size) &&
              decode_text(graphics->text[0], rom, TITLE_BG0, memory->vram, error, error_size) &&
              decode_text(graphics->text[1], rom, TITLE_BG1, memory->vram, error, error_size) &&
              decode_affine(graphics, rom, memory->vram, error, error_size);
    if (ok)
        for (int k = 0; k < 256; ++k) graphics->palette[k] = read_u16(memory->palette + k * 2);
    free(memory);
    if (!ok) {
        if (error && error_size && !error[0]) set_error(error, error_size, "Aria title graphics");
        return false;
    }
    if (error && error_size) error[0] = '\0';
    return true;
}

