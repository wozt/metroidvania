/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_graphics.h"

#include "fusion/assets/gba.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AOS_ROM_BASE = 0x08000000,
    AOS_INTRO_PALETTE = 0x080E6634,
    AOS_INTRO_GRAPHICS_FIRST = 0x080E5BF0,
    AOS_INTRO_GRAPHICS_SECOND = 0x080E5BF8,
    AOS_INTRO_BG1_METADATA = 0x080E9464,
    AOS_INTRO_BG2_METADATA = 0x080E9244,
    AOS_INTRO_BG3_METADATA = 0x080E9354,
    AOS_INTRO_CHARACTER_BASE = 0x8000,
    AOS_INTRO_GRAPHICS_BLOCK = 0x2000,
    AOS_INTRO_PALETTE_BYTES = 0x200,
    AOS_INTRO_TEXT_PALETTE = 15,
    AOS_INTRO_TEXT_COLOR = 8,
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static const uint8_t *slice(const FusionRom *rom, uint32_t pointer,
                            size_t size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA ||
        pointer < AOS_ROM_BASE)
        return NULL;
    size_t offset = (size_t)(pointer - AOS_ROM_BASE);
    if (offset > rom->size || size > rom->size - offset) return NULL;
    return rom->data + offset;
}

static bool load_graphics_resource(const FusionRom *rom, uint32_t pointer,
                                   uint8_t *destination,
                                   char *error, size_t error_size) {
    const uint8_t *resource = slice(rom, pointer, 8);
    if (!resource || resource[3] != 0x10 ||
        (resource[1] != 0 && resource[1] != 4)) {
        set_error(error, error_size,
                  "invalid Aria introduction graphics resource");
        return false;
    }
    size_t size = (size_t)resource[3] << 9;
    if (size != AOS_INTRO_GRAPHICS_BLOCK) {
        set_error(error, error_size,
                  "unexpected Aria introduction graphics size");
        return false;
    }
    if (resource[0] == 0) {
        const uint8_t *source = slice(rom, pointer + 4, size);
        if (!source) {
            set_error(error, error_size,
                      "truncated Aria introduction graphics resource");
            return false;
        }
        memcpy(destination, source, size);
        return true;
    }
    if (resource[0] != 1) {
        set_error(error, error_size,
                  "unsupported Aria introduction graphics encoding");
        return false;
    }
    uint32_t compressed_pointer = read_u32(resource + 4);
    const uint8_t *compressed = slice(rom, compressed_pointer, 4);
    if (!compressed) {
        set_error(error, error_size,
                  "invalid Aria introduction graphics pointer");
        return false;
    }
    size_t available = rom->size - (size_t)(compressed_pointer - AOS_ROM_BASE);
    size_t expanded_size = fusion_gba_uncompressed_size(compressed, available);
    uint8_t *expanded = expanded_size == size + 4 ? malloc(expanded_size) : NULL;
    size_t written = 0;
    if (!expanded ||
        !fusion_gba_lz77_uncompress(compressed, available, expanded,
                                    expanded_size, &written) ||
        written != expanded_size || expanded[0] != 0) {
        if (error && error_size) {
            snprintf(error, error_size,
                     "invalid compressed Aria introduction graphics at "
                     "0x%08X (%zu bytes, header %u/%u/%u/%u)",
                     pointer, expanded_size,
                     expanded ? expanded[0] : 0,
                     expanded ? expanded[1] : 0,
                     expanded ? expanded[2] : 0,
                     expanded ? expanded[3] : 0);
            error[error_size - 1] = '\0';
        }
        free(expanded);
        return false;
    }
    memcpy(destination, expanded + 4, size);
    free(expanded);
    return true;
}

static bool load_palette(const FusionRom *rom, uint8_t *palette,
                         char *error, size_t error_size) {
    const uint8_t *resource = slice(rom, AOS_INTRO_PALETTE,
                                    4 + AOS_INTRO_PALETTE_BYTES);
    if (!resource || resource[0] != 0 || resource[2] < 16 ||
        resource[3] != 0) {
        set_error(error, error_size,
                  "invalid Aria introduction palette resource");
        return false;
    }
    memcpy(palette, resource + 4, AOS_INTRO_PALETTE_BYTES);
    return true;
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
    bool loaded = load_palette(rom, memory->palette, error, error_size) &&
        load_graphics_resource(rom, AOS_INTRO_GRAPHICS_FIRST,
                               memory->vram + AOS_INTRO_CHARACTER_BASE,
                               error, error_size) &&
        load_graphics_resource(rom, AOS_INTRO_GRAPHICS_SECOND,
                               memory->vram + AOS_INTRO_CHARACTER_BASE +
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
    color_rgb(memory->palette,
              AOS_INTRO_TEXT_PALETTE * 16 + AOS_INTRO_TEXT_COLOR,
              graphics->text_rgb);
    free(memory);
    if (error && error_size) error[0] = '\0';
    return true;
}
