/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/menus/aos_portrait.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

enum { ROM_SIZE = 0x600000, TABLE = 0x526C78 };

static void word(uint8_t *p, uint16_t n) {
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
}
static void dword(uint8_t *p, uint32_t n) {
    word(p, (uint16_t)n);
    word(p + 2, (uint16_t)(n >> 16));
}

static void test_table(void) {
    static const uint32_t expected[AOS_PORTRAIT_COUNT] = {
        0x0815E03C, 0x0815E04C, 0x0815E054, 0x0815E05C,
        0x0815E074, 0x0815E07C, 0x0815E06C, 0x0815E044,
    };
    for (uint8_t id = 0; id < AOS_PORTRAIT_COUNT; ++id)
        assert(aos_portrait_graphics_pointer(id) == expected[id]);
    assert(aos_portrait_graphics_pointer(AOS_PORTRAIT_COUNT) == 0);
}

static void test_rejects_invalid_input(void) {
    AosPortraitImage *image = calloc(1, sizeof *image);
    assert(image);
    char error[64] = {0};
    FusionRom rom = {.game = FUSION_ASSET_GAME_ARIA};
    assert(!aos_portrait_decode(&rom, 0, image, error, sizeof error));
    assert(error[0] != '\0');
    memset(error, 0, sizeof error);
    assert(!aos_portrait_decode(&rom, AOS_PORTRAIT_COUNT, image,
                                error, sizeof error));
    assert(error[0] != '\0');
    free(image);
}

static void test_metatile_and_palette(void) {
    uint8_t *data = calloc(1, ROM_SIZE);
    AosPortraitImage *image = malloc(sizeof *image);
    assert(data && image);
    char error[128] = {0};
    FusionRom rom = {.game = FUSION_ASSET_GAME_ARIA, .size = ROM_SIZE, .data = data};

    /* Three resource pointers at the native portrait descriptor. */
    dword(data + TABLE, 0x08002000);
    dword(data + TABLE + 4, 0x08005000);
    dword(data + TABLE + 8, 0x08006000);

    /* Raw 0x2000-byte 4bpp graphics resource, tile 0 and tile 1. */
    data[0x2003] = 0x10;
    data[0x2004] = 0x01;      /* Tile 0: pixel (0, 0) index 1. */
    data[0x2024] = 0x02;      /* Tile 1: pixel (0, 0) index 2. */

    /* One palette row, pink/red and green; destination row 13. */
    data[0x5002] = 1;
    word(data + 0x5004 + 2, 0x001F);
    word(data + 0x5004 + 4, 0x03E0);

    /* One 32x32-tile page; raw map with one 4x4-block. */
    data[0x6000] = data[0x6001] = 1;
    dword(data + 0x6004, 0x08007000);
    dword(data + 0x600C, 0x08007800);
    word(data + 0x7800, 1);
    word(data + 0x7000, 0xD100);
    word(data + 0x7002, 0xD101);

    assert(aos_portrait_decode(&rom, 0, image, error, sizeof error));
    /* A zero vertical-table entry wraps the native -4 BG offset, so map row
     * zero appears at screen row four. */
    size_t red = (AOS_PORTRAIT_WIDTH * 4u) * 4u;
    assert(image->rgba[red + 0] == 255 && image->rgba[red + 1] == 0 &&
           image->rgba[red + 2] == 0 && image->rgba[red + 3] == 255);
    size_t green = (AOS_PORTRAIT_WIDTH * 4u + 8u) * 4u;
    assert(image->rgba[green + 0] == 0 && image->rgba[green + 1] == 255 &&
           image->rgba[green + 2] == 0 && image->rgba[green + 3] == 255);
    assert(image->rgba[(AOS_PORTRAIT_WIDTH * 100 + 100) * 4u + 3] == 0);

    /* Native tile-level H flip moves tile-0 pixel from x=0 to x=7. */
    word(data + 0x7000, 0xD500);
    assert(aos_portrait_decode(&rom, 0, image, error, sizeof error));
    assert(image->rgba[red + 3] == 0);
    assert(image->rgba[red + 7u * 4u + 3] == 255);

    /* Metatile H flip swaps tile positions and flips their pixels. */
    word(data + 0x7800, 0x4001);
    word(data + 0x7006, 0xD100); /* tile at source x=3 */
    assert(aos_portrait_decode(&rom, 0, image, error, sizeof error));
    assert(image->rgba[red + 7u * 4u + 3] == 255);

    /* Invalid 3rd descriptor must be rejected safely. */
    dword(data + TABLE + 8, 0x0EFFFFF0);
    assert(!aos_portrait_decode(&rom, 0, image, error, sizeof error));
    assert(error[0]);
    free(image);
    free(data);
}

static void test_verified_rom(const char *path) {
    FusionRom rom = {0};
    AosPortraitImage *image = malloc(sizeof *image);
    char error[128] = {0};
    assert(image);
    assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom,
                           error, sizeof error));
    for (uint8_t portrait = 0; portrait < AOS_PORTRAIT_COUNT; ++portrait) {
        assert(aos_portrait_decode(&rom, portrait, image, error, sizeof error));
        size_t opaque = 0;
        for (size_t pixel = 0;
             pixel < (size_t)AOS_PORTRAIT_WIDTH * AOS_PORTRAIT_HEIGHT;
             ++pixel)
            if (image->rgba[pixel * 4u + 3]) ++opaque;
        assert(opaque > 1000);
    }
    fusion_rom_close(&rom);
    free(image);
}

int main(int argc, char **argv) {
    test_table();
    test_rejects_invalid_input();
    test_metatile_and_palette();
    if (argc == 2) test_verified_rom(argv[1]);
    return 0;
}
