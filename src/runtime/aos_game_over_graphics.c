/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_over_graphics.h"

#include "aos_rom_graphics.h"

#include <stdio.h>
#include <stdlib.h>

void aos_game_over_graphics_free(AosGameOverGraphics *graphics) {
    if (!graphics) return;
    fusion_aos_free_rendered_background(&graphics->background);
    free(graphics->sprite_bmp);
    *graphics = (AosGameOverGraphics){0};
}

bool aos_game_over_graphics_init(AosGameOverGraphics *graphics, const FusionRom *rom,
                                 uint8_t language, char *error, size_t error_size) {
    if (!graphics || !rom || language < 1 || language > 3) {
        if (error && error_size) snprintf(error, error_size, "invalid Aria game-over request");
        return false;
    }
    *graphics = (AosGameOverGraphics){0};
    FusionAosVideoMemory *memory = calloc(1, sizeof *memory);
    if (!memory) {
        if (error && error_size) snprintf(error, error_size, "cannot allocate video memory");
        return false;
    }
    /* sub_0803FD9C twice, sub_0803C8B0, then sub_0803F8A8(1, 0x080E9584) on
     * BGCNT(0, 29) | 256 colours. */
    FusionAosBackground background = {.layer = 1, .control = 0x80,
                                      .metadata_pointer = 0x080E9584};
    FusionAosBackgroundTilemap tilemap = {0};
    bool ok = aos_rom_load_graphics_resource(rom, 0x080E5C08, memory->vram, 0x4000, error,
                                             error_size) &&
              aos_rom_load_graphics_resource(rom, 0x080E5C10, memory->vram + 0x4000,
                                             sizeof memory->vram - 0x4000, error, error_size) &&
              aos_rom_load_palette_resource(rom, 0x080E6A3C, memory->palette, error,
                                            error_size) &&
              fusion_aos_read_background_tiles(rom, &background, &tilemap, error, error_size) &&
              tilemap.status == FUSION_AOS_TILEMAP_DECODED &&
              fusion_aos_render_background(&background, &tilemap, memory,
                                           &graphics->background, error, error_size);
    fusion_aos_free_background_tilemap(&tilemap);
    free(memory);
    /* The language's sprite (the switch on unk_60.language: 2 French, 3
     * German, otherwise English), frame 0, palette row 0 of 0x0825C368. */
    static const uint32_t sheets[3] = {0x0825B220, 0x082799A0, 0x0826E85C};
    static const uint32_t frames[3] = {0x0825E06C, 0x0827A9FC, 0x0826F894};
    ok = ok && fusion_aos_render_object_frame(rom, sheets[language - 1], 0x0825C368, 0,
                                              frames[language - 1], 0, &graphics->sprite_bmp,
                                              &graphics->sprite_bmp_size,
                                              &graphics->sprite_left, &graphics->sprite_top,
                                              error, error_size);
    if (!ok) {
        if (error && error_size && !error[0])
            snprintf(error, error_size, "unsupported Aria game-over graphics");
        aos_game_over_graphics_free(graphics);
        return false;
    }
    graphics->language = language;
    return true;
}
