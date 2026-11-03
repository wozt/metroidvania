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
    TITLE_DISC_GRAPHICS = 0x08160488,
    TITLE_DISC_FRAMES = 0x0820CB1C,
    TITLE_BATS_GRAPHICS = 0x08160480,
    TITLE_BATS_FRAMES = 0x0820C8A8,
    TITLE_MENU_GRAPHICS = 0x0825EAD4,
    TITLE_MENU_FRAMES = 0x0826693C,
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

bool aos_title_decode_text_layer(uint8_t *out, const FusionRom *rom, uint32_t metadata,
                                 unsigned character_block, const uint8_t *vram, char *error,
                                 size_t error_size) {
    FusionAosBackground background = {.control = (uint16_t)(character_block << 2),
                                      .metadata_pointer = metadata};
    FusionAosBackgroundTilemap tilemap = {0};
    bool ok = fusion_aos_read_background_tiles(rom, &background, &tilemap, error, error_size) &&
              tilemap.status == FUSION_AOS_TILEMAP_DECODED;
    for (size_t ty = 0; ok && ty < 32 && ty < tilemap.height_tiles; ++ty)
        for (size_t tx = 0; tx < 32 && tx < tilemap.width_tiles; ++tx) {
            uint16_t word = tilemap.cells[ty * tilemap.width_tiles + tx];
            const uint8_t *tile =
                vram + character_block * CHARACTER_BLOCK + (size_t)(word & 0x3FF) * 32;
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

/* sub_0803B800 then sub_0803B924: an object resource on its own sheet (the
 * 16 x 16-tile sheet of the graphics header, + 2 and + 3) and its frame
 * descriptor (+ 0: records, + 2: animations, + 4: the 16-byte records,
 * + 12: the animation table). */
static bool decode_sheet(AosTitleSheet *sheet, const FusionRom *rom, uint32_t graphics,
                         uint32_t frames, char *error, size_t error_size) {
    enum { TILE_BYTES = 32, SHEET_TILES = AOS_TITLE_SHEET_SIZE / 8 };
    uint8_t tiles[SHEET_TILES * SHEET_TILES * TILE_BYTES];
    const uint8_t *header = rom_at(rom, graphics, 4);
    const uint8_t *descriptor = rom_at(rom, frames, 16);
    if (!header || !descriptor || header[1] != 4 || header[2] != SHEET_TILES ||
        header[3] != SHEET_TILES) {
        set_error(error, error_size, "unexpected title object sheet");
        return false;
    }
    memset(tiles, 0, sizeof tiles);
    if (!aos_rom_load_graphics_resource(rom, graphics, tiles, sizeof tiles, error, error_size))
        return false;
    for (int y = 0; y < AOS_TITLE_SHEET_SIZE; ++y)
        for (int x = 0; x < AOS_TITLE_SHEET_SIZE; ++x) {
            const uint8_t *tile = tiles + (size_t)((y / 8) * SHEET_TILES + x / 8) * TILE_BYTES;
            sheet->pixels[y * AOS_TITLE_SHEET_SIZE + x] =
                (uint8_t)((tile[(y % 8) * 4 + (x % 8) / 2] >> (4 * (x & 1))) & 0xF);
        }
    uint16_t record_count = read_u16(descriptor), anim_count = read_u16(descriptor + 2);
    if (record_count > AOS_TITLE_MAX_FRAMES || anim_count > AOS_TITLE_MAX_ANIMS) {
        set_error(error, error_size, "title object descriptor exceeds its limits");
        return false;
    }
    sheet->frame_count = (uint8_t)record_count;
    for (unsigned f = 0; f < record_count; ++f) {
        const uint8_t *record = rom_at(rom, read_u32(descriptor + 4) + f * 16, 16);
        if (!record || record[5] > AOS_TITLE_MAX_PIECES) {
            set_error(error, error_size, "invalid title object frame");
            return false;
        }
        AosTitleFrame *frame = &sheet->frames[f];
        frame->count = record[5];
        for (unsigned k = 0; k < frame->count; ++k) {
            const uint8_t *c = rom_at(rom, read_u32(record + 12) + k * 12, 12);
            if (!c || c[4] + c[6] > AOS_TITLE_SHEET_SIZE || c[5] + c[7] > AOS_TITLE_SHEET_SIZE) {
                set_error(error, error_size, "invalid title object component");
                return false;
            }
            frame->pieces[k] = (AosTitlePiece){(int8_t)c[0], (int8_t)c[1], c[6], c[7], c[4],
                                               c[5], (c[10] & 2) != 0, (c[10] & 1) != 0};
        }
    }
    sheet->anim_count = (uint8_t)anim_count;
    for (unsigned a = 0; a < anim_count; ++a) {
        const uint8_t *entry = rom_at(rom, read_u32(descriptor + 12) + a * 4, 4);
        const uint8_t *anim = entry ? rom_at(rom, read_u32(entry), 4) : NULL;
        uint16_t steps = anim ? read_u16(anim) : 0;
        if (!anim || read_u16(anim + 2) != 1 || steps == 0 || steps > AOS_TITLE_MAX_STEPS ||
            !rom_at(rom, read_u32(entry) + 4, (size_t)steps * 4)) {
            set_error(error, error_size, "unsupported title object animation");
            return false;
        }
        sheet->anim_length[a] = (uint8_t)steps;
        for (unsigned k = 0; k < steps; ++k) {
            sheet->anim_frame[a][k] = anim[4 + k * 4];
            sheet->anim_duration[a][k] = anim[5 + k * 4];
        }
    }
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
              aos_title_decode_text_layer(graphics->text[0], rom, TITLE_BG0, 2, memory->vram,
                                          error, error_size) &&
              aos_title_decode_text_layer(graphics->text[1], rom, TITLE_BG1, 2, memory->vram,
                                          error, error_size) &&
              decode_affine(graphics, rom, memory->vram, error, error_size) &&
              decode_sheet(&graphics->sheets[AOS_TITLE_SHEET_DISC], rom, TITLE_DISC_GRAPHICS,
                           TITLE_DISC_FRAMES, error, error_size) &&
              decode_sheet(&graphics->sheets[AOS_TITLE_SHEET_BATS], rom, TITLE_BATS_GRAPHICS,
                           TITLE_BATS_FRAMES, error, error_size) &&
              decode_sheet(&graphics->sheets[AOS_TITLE_SHEET_MENU], rom, TITLE_MENU_GRAPHICS,
                           TITLE_MENU_FRAMES, error, error_size);
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

