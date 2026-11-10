/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_graphics.h"
#include "../../aos_obj_rom.h"
#include "../core/aos_rom_graphics.h"

#include <stdio.h>
#include <stdlib.h>

enum {
    AOS_INTRO_PALETTE = 0x080E6634,
    AOS_INTRO_GRAPHICS_FIRST = 0x080E5BF0,
    AOS_INTRO_GRAPHICS_SECOND = 0x080E5BF8,
    AOS_INTRO_BG1_METADATA = 0x080E9464,
    AOS_INTRO_BG2_METADATA = 0x080E9244,
    AOS_INTRO_BG3_METADATA = 0x080E9354,
    AOS_INTRO_CHARACTER_BASE = 0x8000,
    AOS_INTRO_GRAPHICS_BLOCK = 0x2000,
    AOS_INTRO_MAIN_GRAPHICS = 0x08160490,
    AOS_INTRO_MAIN_FRAMES = 0x0820CB60,
    AOS_INTRO_SUPPORT_GRAPHICS = 0x08160498,
    AOS_INTRO_SUPPORT_FRAMES = 0x0820CD00,
    AOS_INTRO_OBJECT_PALETTE = 0x0820972C,
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

void aos_intro_graphics_free(AosIntroGraphics *graphics) {
    if (!graphics) return;
    for (unsigned index = 0; index < AOS_INTRO_BACKGROUND_COUNT; ++index)
        fusion_aos_free_rendered_background(&graphics->backgrounds[index]);
    *graphics = (AosIntroGraphics){0};
}

bool aos_intro_graphics_init(AosIntroGraphics *graphics,
                             const FusionRom *rom,
                             char *error, size_t error_size) {
    if (!graphics || !rom || rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size,
                  "invalid Aria introduction graphics request");
        return false;
    }
    *graphics = (AosIntroGraphics){0};
    FusionAosVideoMemory *memory = calloc(1, sizeof *memory);
    if (!memory) {
        set_error(error, error_size,
                  "cannot allocate Aria introduction video memory");
        return false;
    }
    bool loaded = aos_rom_load_palette_resource(
                      rom, AOS_INTRO_PALETTE, memory->palette,
                      error, error_size) &&
        aos_rom_load_graphics_resource(
            rom, AOS_INTRO_GRAPHICS_FIRST,
            memory->vram + AOS_INTRO_CHARACTER_BASE,
            sizeof memory->vram - AOS_INTRO_CHARACTER_BASE,
            error, error_size) &&
        aos_rom_load_graphics_resource(
            rom, AOS_INTRO_GRAPHICS_SECOND,
            memory->vram + AOS_INTRO_CHARACTER_BASE + AOS_INTRO_GRAPHICS_BLOCK,
            sizeof memory->vram - AOS_INTRO_CHARACTER_BASE -
                AOS_INTRO_GRAPHICS_BLOCK,
            error, error_size);
    if (!loaded) {
        free(memory);
        return false;
    }

    const uint32_t metadata[AOS_INTRO_BACKGROUND_COUNT] = {
        AOS_INTRO_BG1_METADATA,
        AOS_INTRO_BG2_METADATA,
        AOS_INTRO_BG3_METADATA,
    };
    for (unsigned index = 0; index < AOS_INTRO_BACKGROUND_COUNT; ++index) {
        FusionAosBackground background = {
            .layer = (uint8_t)(index + 1),
            .control = 2u << 2,
            .metadata_pointer = metadata[index],
        };
        FusionAosBackgroundTilemap tilemap = {0};
        bool ok = fusion_aos_read_background_tiles(
                      rom, &background, &tilemap, error, error_size) &&
                  tilemap.status == FUSION_AOS_TILEMAP_DECODED &&
                  fusion_aos_render_background(
                      &background, &tilemap, memory,
                      &graphics->backgrounds[index], error, error_size) &&
                  graphics->backgrounds[index].unresolved_count == 0;
        fusion_aos_free_background_tilemap(&tilemap);
        if (!ok) {
            if (error && error_size && error[0] == '\0')
                set_error(error, error_size,
                          "unsupported Aria introduction background");
            free(memory);
            aos_intro_graphics_free(graphics);
            return false;
        }
    }
    color_rgb(memory->palette, 0, graphics->backdrop_rgb);
    /* The introduction's text is the white colour written to BG palette
     * row 15 by sub_080412F0(8). Keep it explicit: the resource row is
     * reused while the cutscene runs and is not the final text colour. */
    graphics->text_rgb[0] = 255;
    graphics->text_rgb[1] = 255;
    graphics->text_rgb[2] = 255;
    uint8_t object_palette[0x200] = {0};
    if (!aos_obj_sheet_decode(&graphics->actors[0], rom,
                              AOS_INTRO_MAIN_GRAPHICS,
                              AOS_INTRO_MAIN_FRAMES, error, error_size) ||
        !aos_obj_sheet_decode(&graphics->actors[1], rom,
                              AOS_INTRO_SUPPORT_GRAPHICS,
                              AOS_INTRO_SUPPORT_FRAMES, error, error_size) ||
        !aos_rom_load_palette_range(rom, AOS_INTRO_OBJECT_PALETTE, 0, 4, 0,
                                    object_palette, error, error_size)) {
        free(memory);
        aos_intro_graphics_free(graphics);
        return false;
    }
    for (unsigned index = 0; index < 256; ++index)
        graphics->object_palette[index] =
            (uint16_t)(object_palette[index * 2] |
                       object_palette[index * 2 + 1] << 8);
    free(memory);
    if (error && error_size) error[0] = '\0';
    return true;
}
