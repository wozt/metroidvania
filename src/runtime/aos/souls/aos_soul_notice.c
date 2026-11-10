/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_notice.h"

#include "../../aos/core/aos_rom_graphics.h"

#include <string.h>

enum {
    AOS_HUD_TILES = 0x0827B208,
    AOS_HUD_PALETTE = 0x0820C428,
    AOS_TILE_BASE = 0x200,
    AOS_DYNAMIC_BASE = 0x280,
    AOS_BG_SCROLL_Y = 6,
};

static void store(AosSoulNotice *notice, int x, int y, uint16_t entry) {
    if (x >= 0 && x < 32 && y >= 0 && y < 32)
        notice->map[y * 32 + x] = entry;
}

static void fill_dynamic_pixel(AosIntroTextTiles *tiles, unsigned x,
                               unsigned y, uint8_t color) {
    size_t tile = (size_t)(y / 8) * 32 + x / 8;
    size_t offset = tile * 32 + (y & 7u) * 4 + (x & 7u) / 2;
    unsigned shift = (x & 1u) * 4u;
    if (((tiles->data[offset] >> shift) & 0xFu) == 0)
        tiles->data[offset] |= (uint8_t)(color << shift);
}

bool aos_soul_notice_init(AosSoulNotice *notice, const FusionRom *rom,
                          char *error, size_t error_size) {
    if (!notice) return false;
    memset(notice, 0, sizeof *notice);
    return aos_rom_load_graphics_resource(rom, AOS_HUD_TILES,
                                           notice->static_tiles,
                                           sizeof notice->static_tiles,
                                           error, error_size) &&
           aos_rom_load_palette_range(rom, AOS_HUD_PALETTE, 1, 1, 14,
                                      notice->palette, error, error_size) &&
           aos_rom_load_palette_range(rom, AOS_HUD_PALETTE, 7, 1, 15,
                                      notice->palette, error, error_size);
}

void aos_soul_notice_build(AosSoulNotice *notice,
                           const AosIntroTextTiles *rendered,
                           unsigned name_tiles, bool player_low,
                           bool description) {
    if (!notice || !rendered) return;
    memset(notice->map, 0, sizeof notice->map);
    notice->dynamic_tiles = *rendered;
    if (name_tiles > 20) name_tiles = 20;
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < name_tiles * 8; ++x)
            fill_dynamic_pixel(&notice->dynamic_tiles, x, y, 1);
    if (description) {
        /* sub_0800E380: five 30-tile rows at VRAM 0x5C00 use colour 2. */
        for (unsigned y = 24; y < 64; ++y)
            for (unsigned x = 0; x < 240; ++x)
                fill_dynamic_pixel(&notice->dynamic_tiles, x, y, 2);
        /* The solid top and bottom description tiles at 0x5600/0x5620. */
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 8; ++x) {
                unsigned top_y = y, bottom_y = y;
                size_t top = 0x30u * 32u + top_y * 4u + x / 2u;
                size_t bottom = 0x31u * 32u + bottom_y * 4u + x / 2u;
                unsigned shift = (x & 1u) * 4u;
                notice->dynamic_tiles.data[top] =
                    (uint8_t)((notice->dynamic_tiles.data[top] & ~(0xFu << shift)) |
                              ((y == 0 ? 15u : 2u) << shift));
                notice->dynamic_tiles.data[bottom] =
                    (uint8_t)((notice->dynamic_tiles.data[bottom] & ~(0xFu << shift)) |
                              ((y == 7 ? 15u : 2u) << shift));
            }
    }

    int banner_y = player_low ? 3 : 12;
    int x = 15 - (int)((name_tiles + 4) / 2);
    store(notice, x++, banner_y, 0xE220);
    store(notice, x++, banner_y, 0xE234);
    store(notice, x++, banner_y, 0xE235);
    for (unsigned i = 2; i < name_tiles; ++i) store(notice, x++, banner_y, 0xE223);
    store(notice, x++, banner_y, 0xE622);
    store(notice, x++, banner_y, 0xE621);
    store(notice, x, banner_y, 0xE620);
    x = 15 - (int)((name_tiles + 4) / 2);
    store(notice, x++, banner_y + 1, 0xE230);
    store(notice, x++, banner_y + 1, 0xE231);
    for (unsigned i = 0; i < name_tiles; ++i) store(notice, x++, banner_y + 1,
                                                    (uint16_t)(0xE280 + i));
    store(notice, x++, banner_y + 1, 0xE631);
    store(notice, x, banner_y + 1, 0xE630);
    x = 15 - (int)((name_tiles + 4) / 2);
    store(notice, x++, banner_y + 2, 0xE240);
    store(notice, x++, banner_y + 2, 0xE241);
    store(notice, x++, banner_y + 2, 0xE242);
    for (unsigned i = 2; i < name_tiles; ++i) store(notice, x++, banner_y + 2, 0xE243);
    store(notice, x++, banner_y + 2, 0xE642);
    store(notice, x++, banner_y + 2, 0xE641);
    store(notice, x, banner_y + 2, 0xE640);

    if (description) {
        int description_y = player_low ? 7 : 16;
        for (int column = 0; column < 30; ++column) {
            store(notice, column, description_y - 1, 0xE2B0);
            for (int row = 0; row < 3; ++row)
                store(notice, column, description_y + row,
                      (uint16_t)(0xE2E0 + row * 0x20 + column));
            store(notice, column, description_y + 3, 0xE2B1);
        }
    }
}

bool aos_soul_notice_pixel(const AosSoulNotice *notice, uint32_t frame,
                           unsigned x, unsigned y, uint16_t *color) {
    if (!notice || !color || x >= 240 || y >= 160) return false;
    unsigned map_y = (y + AOS_BG_SCROLL_Y) & 0xFFu;
    uint16_t entry = notice->map[(map_y / 8) * 32 + x / 8];
    /* sub_0800E40C's folded-page marker beside the description. */
    if (x / 8 == 28 && (map_y / 8 == 8 || map_y / 8 == 9))
        entry = (uint16_t)(0xF226 + (map_y / 8 == 9 ? 0x10 : 0) +
                           ((frame & 8u) ? 0 : 1));
    unsigned tile = entry & 0x3FFu, px = x & 7u, py = map_y & 7u;
    if (entry & 0x400u) px = 7 - px;
    if (entry & 0x800u) py = 7 - py;
    const uint8_t *source = NULL;
    if (tile >= AOS_DYNAMIC_BASE && tile < AOS_DYNAMIC_BASE + 0x100)
        source = notice->dynamic_tiles.data + (tile - AOS_DYNAMIC_BASE) * 32u;
    else if (tile >= AOS_TILE_BASE && tile < AOS_TILE_BASE + 0x100)
        source = notice->static_tiles + (tile - AOS_TILE_BASE) * 32u;
    if (!source) return false;
    unsigned index = (source[py * 4 + px / 2] >> ((px & 1u) * 4u)) & 0xFu;
    if (!index) return false;
    size_t slot = ((size_t)(entry >> 12) * 16u + index) * 2u;
    *color = (uint16_t)(notice->palette[slot] | notice->palette[slot + 1] << 8);
    return true;
}
