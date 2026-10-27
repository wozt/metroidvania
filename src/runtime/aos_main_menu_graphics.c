/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_main_menu_graphics.h"

#include "aos_rom_graphics.h"

#include <stdio.h>
#include <stdlib.h>

enum {
    AOS_MAIN_PALETTE = 0x080E5E24,
    AOS_MAIN_GRAPHICS_FIRST = 0x080E5BB0,
    AOS_MAIN_GRAPHICS_SECOND = 0x080E5BB8,
    AOS_MAIN_GRAPHICS_ENGLISH = 0x080E5BC0,
    AOS_MAIN_GRAPHICS_FRENCH = 0x08277994,
    AOS_MAIN_GRAPHICS_GERMAN = 0x0826C850,
    AOS_MAIN_GRAPHICS_BLOCK = 0x2000,
};

static const uint32_t metadata[AOS_MAIN_GRAPHICS_SCREEN_COUNT]
                              [AOS_MAIN_GRAPHICS_LAYER_COUNT] = {
    [AOS_MAIN_GRAPHICS_BASE] = {0x080E73A4, 0x080E7434, 0x080E74C4},
    [AOS_MAIN_GRAPHICS_PLAY] = {0x080E7314, 0x080E7044, 0x080E74C4},
    [AOS_MAIN_GRAPHICS_COPY] = {0x080E7284, 0x080E6FB4, 0x080E74C4},
    [AOS_MAIN_GRAPHICS_DELETE] = {0x080E71F4, 0x080E6E04, 0x080E74C4},
    [AOS_MAIN_GRAPHICS_NAME] = {0x080E70D4, 0x080E6E94, 0x080E74C4},
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static void color_rgb(const uint8_t *palette, unsigned index,
                      uint8_t rgb[3]) {
    const uint8_t *entry = palette + index * 2;
    uint16_t color = (uint16_t)entry[0] | (uint16_t)entry[1] << 8;
    rgb[0] = (uint8_t)(((color & 31) * 255) / 31);
    rgb[1] = (uint8_t)((((color >> 5) & 31) * 255) / 31);
    rgb[2] = (uint8_t)((((color >> 10) & 31) * 255) / 31);
}

void aos_main_menu_graphics_free(AosMainMenuGraphics *graphics) {
    if (!graphics) return;
    for (unsigned screen = 0; screen < AOS_MAIN_GRAPHICS_SCREEN_COUNT;
         ++screen)
        for (unsigned layer = 0; layer < AOS_MAIN_GRAPHICS_LAYER_COUNT;
             ++layer)
            fusion_aos_free_rendered_background(
                &graphics->backgrounds[screen][layer]);
    *graphics = (AosMainMenuGraphics){0};
}

bool aos_main_menu_graphics_init(AosMainMenuGraphics *graphics,
                                 const FusionRom *rom, uint8_t language,
                                 char *error, size_t error_size) {
    if (!graphics || !rom || rom->game != FUSION_ASSET_GAME_ARIA ||
        language < 1 || language > 3) {
        set_error(error, error_size, "invalid Aria main-menu graphics request");
        return false;
    }
    *graphics = (AosMainMenuGraphics){0};
    FusionAosVideoMemory *memory = calloc(1, sizeof *memory);
    if (!memory) {
        set_error(error, error_size, "cannot allocate Aria main-menu video memory");
        return false;
    }
    const uint32_t language_graphics[] = {
        AOS_MAIN_GRAPHICS_ENGLISH,
        AOS_MAIN_GRAPHICS_FRENCH,
        AOS_MAIN_GRAPHICS_GERMAN,
    };
    bool loaded = aos_rom_load_palette_resource(
                      rom, AOS_MAIN_PALETTE, memory->palette,
                      error, error_size) &&
        aos_rom_load_graphics_resource(
            rom, AOS_MAIN_GRAPHICS_FIRST, memory->vram,
            sizeof memory->vram, error, error_size) &&
        aos_rom_load_graphics_resource(
            rom, AOS_MAIN_GRAPHICS_SECOND,
            memory->vram + AOS_MAIN_GRAPHICS_BLOCK,
            sizeof memory->vram - AOS_MAIN_GRAPHICS_BLOCK,
            error, error_size) &&
        aos_rom_load_graphics_resource(
            rom, language_graphics[language - 1],
            memory->vram + AOS_MAIN_GRAPHICS_BLOCK * 2,
            sizeof memory->vram - AOS_MAIN_GRAPHICS_BLOCK * 2,
            error, error_size);
    if (!loaded) {
        free(memory);
        return false;
    }

    for (unsigned screen = 0; screen < AOS_MAIN_GRAPHICS_SCREEN_COUNT;
         ++screen) {
        for (unsigned layer = 0; layer < AOS_MAIN_GRAPHICS_LAYER_COUNT;
             ++layer) {
            FusionAosBackground background = {
                .layer = (uint8_t)(layer + 1),
                .control = 0,
                .metadata_pointer = metadata[screen][layer],
            };
            FusionAosBackgroundTilemap tilemap = {0};
            bool ok = fusion_aos_read_background_tiles(
                          rom, &background, &tilemap, error, error_size) &&
                      tilemap.status == FUSION_AOS_TILEMAP_DECODED &&
                      fusion_aos_render_background(
                          &background, &tilemap, memory,
                          &graphics->backgrounds[screen][layer],
                          error, error_size) &&
                      graphics->backgrounds[screen][layer].unresolved_count == 0;
            fusion_aos_free_background_tilemap(&tilemap);
            if (!ok) {
                if (error && error_size && error[0] == '\0')
                    set_error(error, error_size,
                              "unsupported Aria main-menu background");
                free(memory);
                aos_main_menu_graphics_free(graphics);
                return false;
            }
        }
    }
    color_rgb(memory->palette, 0, graphics->backdrop_rgb);
    graphics->language = language;
    free(memory);
    if (error && error_size) error[0] = '\0';
    return true;
}
