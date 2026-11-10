/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_portrait.h"

#include "../core/aos_rom_graphics.h"
#include "fusion/assets/gba.h"

#include <stdio.h>
#include <string.h>

/* Native references:
 * cvaos: sub_08067F24 reads 3 words per entry at 0x08526C78;
 *        sub_080676BC loads graphic resources to VRAM 0x06006000, palette
 *        row 13, and BG0 metadata via sub_0803F970.
 *        sub_08001800 / sub_08001994 expand 4x4 metatiles, including flips.
 */
enum {
    AOS_ROM_BASE = 0x08000000,
    AOS_PORTRAIT_TABLE = 0x08526C78,
    AOS_TILESET_BYTES = 0x2000,
    AOS_BG_TILE_WORDS = 0x400,
    AOS_BG_TILE_BYTES = AOS_BG_TILE_WORDS * 2,
    AOS_BG_TILEMAP_MAX_DECOMPRESSED = AOS_BG_TILE_BYTES + 4,
    AOS_BG_METATILE_COLUMNS_PER_PAGE = 8,
    AOS_PORTRAIT_TILE_BASE = 0x100, /* (0x06006000 - 0x06004000) / 32 */
    AOS_PORTRAIT_PALETTE_ROW = 13,
    AOS_PORTRAIT_SCREEN_OR = 0xD100,
    AOS_PORTRAIT_SCROLL_TABLE = 0x08526CE4,
};

static const uint32_t portrait_graphics[AOS_PORTRAIT_COUNT] = {
    0x0815E03C, 0x0815E04C, 0x0815E054, 0x0815E05C,
    0x0815E074, 0x0815E07C, 0x0815E06C, 0x0815E044,
};

static void set_error(char *error, size_t size, const char *message) {
    if (!error || size == 0) return;
    snprintf(error, size, "%s", message);
    error[size - 1] = '\0';
}

uint32_t aos_portrait_graphics_pointer(uint8_t portrait) {
    return portrait < AOS_PORTRAIT_COUNT ? portrait_graphics[portrait] : 0;
}

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const uint8_t *rom_slice(const FusionRom *rom, uint32_t address,
                                size_t count) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA ||
        address < AOS_ROM_BASE) return NULL;
    size_t offset = (size_t)(address - AOS_ROM_BASE);
    if (offset > rom->size || count > rom->size - offset) return NULL;
    return rom->data + offset;
}

static uint8_t expand_channel(unsigned value) {
    return (uint8_t)((value * 255u) / 31u);
}

static bool load_tile_map(const FusionRom *rom, uint32_t pointer,
                          bool compressed, uint8_t map[AOS_BG_TILE_BYTES],
                          size_t *word_count,
                          char *error, size_t error_size) {
    memset(map, 0, AOS_BG_TILE_BYTES);
    if (!compressed) {
        const uint8_t *src = rom_slice(rom, pointer, AOS_BG_TILE_BYTES);
        if (!src) {
            set_error(error, error_size, "Aria portrait BG tile data outside ROM");
            return false;
        }
        memcpy(map, src, AOS_BG_TILE_BYTES);
        *word_count = AOS_BG_TILE_WORDS;
        return true;
    }
    const uint8_t *src = rom_slice(rom, pointer, 4);
    if (!src) {
        set_error(error, error_size, "Aria portrait compressed tile map outside ROM");
        return false;
    }
    size_t available = rom->size - (size_t)(pointer - AOS_ROM_BASE);
    size_t expanded = fusion_gba_uncompressed_size(src, available);
    if (expanded < 4 || expanded > AOS_BG_TILEMAP_MAX_DECOMPRESSED ||
        ((expanded - 4) & 1u)) {
        set_error(error, error_size, "Aria portrait tile map has an invalid size");
        return false;
    }
    uint8_t decompressed[AOS_BG_TILEMAP_MAX_DECOMPRESSED];
    size_t written = 0;
    if (!fusion_gba_lz77_uncompress(src, available, decompressed,
                                   expanded, &written) || written != expanded) {
        set_error(error, error_size, "Aria portrait LZ77 tile map is invalid");
        return false;
    }
    /* Native LZ77UnCompWram writes to EWRAM bgData - 2 u16. The first four
     * expanded bytes therefore precede the tile array and are not read by
     * sub_08001994. Portraits carry only the metatile blocks they reference,
     * rather than a fixed 0x800-byte map. */
    size_t tile_bytes = expanded - 4;
    memcpy(map, decompressed + 4, tile_bytes);
    *word_count = tile_bytes / 2;
    return true;
}

bool aos_portrait_decode(const FusionRom *rom, uint8_t portrait,
                         AosPortraitImage *image, char *error, size_t error_size) {
    if (!image || portrait >= AOS_PORTRAIT_COUNT) {
        set_error(error, error_size, "invalid Aria dialogue portrait");
        return false;
    }

    /* The table entry is {graphics resource, palette resource, BG metadata}.
     * A graphics resource alone is only a tile sheet: it is not an image. */
    const uint8_t *entry = rom_slice(rom, AOS_PORTRAIT_TABLE + portrait * 12u, 12);
    if (!entry) {
        set_error(error, error_size, "Aria portrait descriptor outside ROM");
        return false;
    }
    uint32_t graphics_pointer = read_u32(entry);
    uint32_t palette_pointer = read_u32(entry + 4);
    uint32_t metadata_pointer = read_u32(entry + 8);
    const uint8_t *meta = rom_slice(rom, metadata_pointer, 16);
    if (!meta) {
        set_error(error, error_size, "Aria portrait BG metadata outside ROM");
        return false;
    }
    unsigned pages_x = meta[0], pages_y = meta[1];
    unsigned flags = read_u16(meta + 2);
    uint32_t map_pointer = read_u32(meta + 4);
    uint32_t metatile_pointer = read_u32(meta + 12);
    const uint8_t *scroll = rom_slice(rom,
        AOS_PORTRAIT_SCROLL_TABLE + portrait * 2u, 2);
    /* This dialogue path uses a regular 4bpp text BG, not affine tiles. */
    if (!pages_x || !pages_y || pages_x > 4 || pages_y > 4 ||
        (flags & 1u) || !scroll) {
        set_error(error, error_size, "unsupported Aria portrait BG layout");
        return false;
    }
    size_t block_columns = pages_x * AOS_BG_METATILE_COLUMNS_PER_PAGE;
    size_t block_rows = pages_y * AOS_BG_METATILE_COLUMNS_PER_PAGE;
    const uint8_t *metatiles = rom_slice(rom, metatile_pointer,
                                         block_columns * block_rows * 2u);
    if (!metatiles) {
        set_error(error, error_size, "Aria portrait metatile map outside ROM");
        return false;
    }
    uint8_t tiles[AOS_TILESET_BYTES] = {0};
    uint8_t palette[0x200] = {0};
    uint8_t map[AOS_BG_TILE_BYTES];
    size_t map_word_count = 0;
    if (!aos_rom_load_graphics_resource(rom, graphics_pointer, tiles,
                                        sizeof tiles, error, error_size) ||
        !aos_rom_load_palette_range(rom, palette_pointer, 0, 1,
                                     AOS_PORTRAIT_PALETTE_ROW, palette,
                                     error, error_size) ||
        !load_tile_map(rom, map_pointer, (flags & 2u) != 0,
                       map, &map_word_count, error, error_size))
        return false;

    memset(image, 0, sizeof *image);
    unsigned h_offset = (unsigned)scroll[0] * 8u;
    unsigned v_offset = (unsigned)scroll[1] * 8u - 4u;
    for (unsigned y = 0; y < AOS_PORTRAIT_HEIGHT; ++y) {
        unsigned map_y = (y + v_offset) & 0xFFu;
        unsigned by = map_y / 8u;
        unsigned py = map_y & 7u;
        for (unsigned x = 0; x < AOS_PORTRAIT_WIDTH; ++x) {
            unsigned map_x = (x + h_offset) & 0xFFu;
            unsigned bx = map_x / 8u;
            unsigned px = map_x & 7u;
            unsigned block_x = bx / 4u, block_y = by / 4u;
            if (block_x >= block_columns || block_y >= block_rows) continue;
            size_t mi = ((size_t)block_y * block_columns + block_x) * 2;
            uint16_t meta_word = read_u16(metatiles + mi);
            unsigned block = meta_word & 0x3FFFu;
            if (!block) continue; /* Transparent 4x4 metatile. */
            --block; /* Native block indices are one-based. */
            unsigned ix = (meta_word & 0x4000u) ? 3u - (bx & 3u) : (bx & 3u);
            unsigned iy = (meta_word & 0x8000u) ? 3u - (by & 3u) : (by & 3u);
            size_t tile_index_in_map = (size_t)block * 16u + iy * 4u + ix;
            if (tile_index_in_map >= map_word_count) {
                set_error(error, error_size, "Aria portrait metatile references invalid tile map");
                return false;
            }
            uint16_t entry_word = read_u16(map + tile_index_in_map * 2u);
            /* sub_08001994 XORs the block flip into each 8x8 tile. */
            entry_word ^= (uint16_t)((meta_word & 0xC000u) >> 4);
            /* sub_080676BC then applies this value to all 0x400 BG0 entries,
             * selecting the portrait tiles at character base 1 and palette
             * row 13. */
            entry_word |= AOS_PORTRAIT_SCREEN_OR;
            unsigned tile_id = entry_word & 0x3FFu;
            unsigned bank = entry_word >> 12;
            /* The native portrait graphics occupy character base 1, tiles
             * 0x100..0x1FF, and use palette row 13. Other BG tiles belong
             * to the scene/text UI and must not be painted as portraits. */
            if (tile_id < AOS_PORTRAIT_TILE_BASE ||
                tile_id >= AOS_PORTRAIT_TILE_BASE + AOS_TILESET_BYTES / 32u ||
                bank != AOS_PORTRAIT_PALETTE_ROW) continue;
            size_t tile_offset = (tile_id - AOS_PORTRAIT_TILE_BASE) * 32u;
            unsigned sy = (entry_word & 0x800u) ? 7u - py : py;
            unsigned sx = (entry_word & 0x400u) ? 7u - px : px;
            uint8_t packed = tiles[tile_offset + sy * 4u + sx / 2u];
            unsigned color_index = (packed >> (4u * (sx & 1u))) & 15u;
            if (!color_index) continue;
            size_t pi = (AOS_PORTRAIT_PALETTE_ROW * 16u + color_index) * 2u;
            uint16_t color = read_u16(palette + pi);
            uint8_t *pixel = image->rgba + (y * AOS_PORTRAIT_WIDTH + x) * 4u;
            pixel[0] = expand_channel(color & 31u);
            pixel[1] = expand_channel((color >> 5) & 31u);
            pixel[2] = expand_channel((color >> 10) & 31u);
            pixel[3] = 255;
        }
    }
    if (error && error_size) error[0] = '\0';
    return true;
}
