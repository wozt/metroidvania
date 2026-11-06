/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_pause_graphics.h"

#include "aos_rom_graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AOS_PAUSE_PALETTE = 0x080E5C20,
    AOS_PAUSE_GRAPHICS = 0x0815E02C,
    AOS_PAUSE_BG1_METADATA = 0x0815E940,
    AOS_PAUSE_BG1_CONTROL = 0x1D00,
    AOS_PAUSE_BANK_ENGLISH = 0x0827B200,
    AOS_PAUSE_BANK_FRENCH = 0x08277984,
    AOS_PAUSE_BANK_GERMAN = 0x0826C848,
    AOS_PAUSE_BG0_FILL = 0x330D,
    AOS_PAUSE_WIDTH = 240,
    AOS_PAUSE_HEIGHT = 160,
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || !error_size) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static void color_rgb(const uint8_t *palette, unsigned index, uint8_t rgb[3]) {
    uint16_t color = (uint16_t)palette[index * 2] |
                     (uint16_t)palette[index * 2 + 1] << 8;
    rgb[0] = (uint8_t)(((color & 31) * 255) / 31);
    rgb[1] = (uint8_t)((((color >> 5) & 31) * 255) / 31);
    rgb[2] = (uint8_t)((((color >> 10) & 31) * 255) / 31);
}

void aos_pause_graphics_free(AosPauseGraphics *graphics) {
    if (!graphics) return;
    fusion_aos_free_rendered_background(&graphics->background);
    *graphics = (AosPauseGraphics){0};
}

bool aos_pause_graphics_init(AosPauseGraphics *graphics, const FusionRom *rom,
                             uint8_t language,
                             char *error, size_t error_size) {
    if (!graphics || !rom || rom->game != FUSION_ASSET_GAME_ARIA ||
        language < 1 || language > 3) {
        set_error(error, error_size, "invalid Aria pause graphics request");
        return false;
    }
    *graphics = (AosPauseGraphics){0};
    FusionAosVideoMemory *memory = calloc(1, sizeof *memory);
    if (!memory) {
        set_error(error, error_size, "cannot allocate Aria pause video memory");
        return false;
    }
    static const uint32_t language_banks[] = {
        AOS_PAUSE_BANK_ENGLISH, AOS_PAUSE_BANK_FRENCH, AOS_PAUSE_BANK_GERMAN,
    };
    bool loaded = aos_rom_load_palette_resource(
                          rom, AOS_PAUSE_PALETTE, memory->palette,
                          error, error_size) &&
                      aos_rom_load_graphics_resource(
                          rom, AOS_PAUSE_GRAPHICS, memory->vram,
                          sizeof memory->vram, error, error_size) &&
                      aos_rom_load_graphics_resource(
                          rom, language_banks[language - 1],
                          memory->vram + 0xA000,
                          sizeof memory->vram - 0xA000, error, error_size);
    if (!loaded) {
        free(memory);
        return false;
    }
    FusionAosBackground background = {
        .layer = 1,
        .control = AOS_PAUSE_BG1_CONTROL,
        .metadata_pointer = AOS_PAUSE_BG1_METADATA,
    };
    FusionAosBackgroundTilemap tilemap = {0};
    bool ok = fusion_aos_read_background_tiles(rom, &background, &tilemap,
                                                error, error_size) &&
              tilemap.status == FUSION_AOS_TILEMAP_DECODED &&
              fusion_aos_render_background(&background, &tilemap, memory,
                                           &graphics->background,
                                           error, error_size) &&
              graphics->background.unresolved_count == 0;
    fusion_aos_free_background_tilemap(&tilemap);
    if (ok) {
        color_rgb(memory->palette, 0, graphics->backdrop_rgb);
        memcpy(graphics->palette, memory->palette, sizeof graphics->palette);
        memcpy(graphics->bg0_chars, memory->vram + 0x4000,
               sizeof graphics->bg0_chars);
        graphics->language = language;
        graphics->text_ready =
            aos_intro_resources_init(&graphics->text, rom, NULL, 0) &&
            aos_intro_font_init(&graphics->font, rom, NULL, 0);
    }
    free(memory);
    if (!ok) {
        if (error && error_size && !error[0])
            set_error(error, error_size, "unsupported Aria pause background");
        aos_pause_graphics_free(graphics);
        return false;
    }
    if (error && error_size) error[0] = '\0';
    return true;
}

static void put_tiles(AosPauseGraphics *graphics, int x, int y,
                      uint16_t entry, int count) {
    int cell = y * 32 + x;
    for (int index = 0; index < count; ++index, ++cell)
        if (cell >= 0 && cell < 32 * 32)
            graphics->bg0_map[cell] = (uint16_t)(entry + index);
}

static void put_number(AosPauseGraphics *graphics, int x, int y,
                       int32_t value, int digits, uint16_t base,
                       bool zero_pad) {
    int cell = y * 32 + x;
    if (value < 0) {
        if (cell >= 0 && cell < 32 * 32) graphics->bg0_map[cell] = base;
        return;
    }
    int count = 1;
    do {
        uint16_t entry = (uint16_t)(base + value % 10);
        if (count != 1 && value == 0 && !zero_pad) entry = (uint16_t)(base + 0xD);
        if (cell >= 0 && cell < 32 * 32) graphics->bg0_map[cell] = entry;
        value /= 10;
        --cell;
        ++count;
    } while (count <= digits || value != 0);
}

static void put_large_number(AosPauseGraphics *graphics, int x, int y,
                             uint32_t value, int digits, unsigned palette) {
    uint16_t base = (uint16_t)(0x300 | (palette << 12));
    for (int count = 1; count <= digits || value != 0; ++count, --x) {
        unsigned digit = value % 10;
        value /= 10;
        graphics->bg0_map[y * 32 + x] = (uint16_t)(base + 0x10 + digit);
        graphics->bg0_map[(y + 1) * 32 + x] = (uint16_t)(base + 0x20 + digit);
    }
}

static void put_meter(AosPauseGraphics *graphics, int x, int y,
                      int32_t current, int32_t maximum, unsigned palette) {
    int32_t eighths = maximum > 0 ? current * 100 / maximum * 8 / 100 : 0;
    if (eighths < 0) eighths = 0;
    if (eighths > 56) eighths = 56;
    for (int cell = 0; cell < 7; ++cell) {
        int remaining = eighths - cell * 8;
        uint16_t tile = remaining >= 8 ? 0x330
                        : remaining <= 0 ? 0x338
                                         : (uint16_t)(0x338 - remaining);
        graphics->bg0_map[y * 32 + x + cell] =
            (uint16_t)(tile | palette << 12);
    }
}

static bool bg0_pixel(const AosPauseGraphics *graphics, unsigned x, unsigned y,
                      uint8_t rgba[4]) {
    uint16_t entry = graphics->bg0_map[(y / 8) * 32 + x / 8];
    unsigned px = x & 7, py = y & 7;
    if (entry & 0x400) px = 7 - px;
    if (entry & 0x800) py = 7 - py;
    size_t offset = (size_t)(entry & 0x3FF) * 32 + py * 4 + px / 2;
    if (offset >= sizeof graphics->bg0_chars) return false;
    unsigned color_index =
        (graphics->bg0_chars[offset] >> ((px & 1u) * 4)) & 0xF;
    if (!color_index) return false;
    size_t color = ((size_t)(entry >> 12) * 16 + color_index) * 2;
    uint16_t bgr = (uint16_t)(graphics->palette[color] |
                              graphics->palette[color + 1] << 8);
    rgba[0] = (uint8_t)(((bgr & 31) * 255) / 31);
    rgba[1] = (uint8_t)((((bgr >> 5) & 31) * 255) / 31);
    rgba[2] = (uint8_t)((((bgr >> 10) & 31) * 255) / 31);
    rgba[3] = 255;
    return true;
}

static bool valid_output(const AosPauseGraphics *graphics, const uint8_t *rgba,
                         size_t rgba_size) {
    return graphics && rgba &&
           rgba_size >= AOS_PAUSE_WIDTH * AOS_PAUSE_HEIGHT * 4 &&
           graphics->background.rgba &&
           graphics->background.width_pixels >= AOS_PAUSE_WIDTH &&
           graphics->background.height_pixels >= AOS_PAUSE_HEIGHT;
}

static void composite(const AosPauseGraphics *graphics, uint8_t *rgba) {
    for (unsigned y = 0; y < AOS_PAUSE_HEIGHT; ++y)
        for (unsigned x = 0; x < AOS_PAUSE_WIDTH; ++x) {
            size_t out = ((size_t)y * AOS_PAUSE_WIDTH + x) * 4;
            size_t background =
                ((size_t)y * graphics->background.width_pixels + x) * 4;
            memcpy(rgba + out, graphics->background.rgba + background, 4);
            bg0_pixel(graphics, x, y, rgba + out);
        }
}

bool aos_pause_graphics_render_hub(AosPauseGraphics *graphics,
                                   const AosPauseHubValues *values,
                                   uint8_t selected, uint8_t *rgba,
                                   size_t rgba_size) {
    if (!values || selected >= 7 ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    for (size_t index = 0; index < 32 * 32; ++index)
        graphics->bg0_map[index] = AOS_PAUSE_BG0_FILL;

    /* sub_08047C4C: seven six-tile labels, palette 3 selected and palette 7
     * otherwise. The tile sequence is the language bank at VRAM 0xA000. */
    for (unsigned entry = 0; entry < 7; ++entry)
        put_tiles(graphics, 2, 12 + (int)entry,
                  (uint16_t)((0x340 + entry * 0x10) |
                             (entry == selected ? 3 : 7) << 12), 6);

    put_number(graphics, 14, 2, values->hp, 5, 0x3300, false);
    put_number(graphics, 14, 3, values->mp, 5, 0x3300, false);
    put_number(graphics, 19, 2, values->max_hp, 5, 0x3300, false);
    put_number(graphics, 19, 3, values->max_mp, 5, 0x3300, false);
    put_meter(graphics, 21, 2, values->hp, values->max_hp, 3);
    put_meter(graphics, 21, 3, values->mp, values->max_mp, 4);
    for (int stat = 0; stat < 4; ++stat)
        put_number(graphics, 14, 5 + stat, values->stats[stat], 3, 0x3300, false);
    put_number(graphics, 24, 5, values->attack, 3, 0x3300, false);
    put_number(graphics, 24, 6, values->defence, 3, 0x3300, false);
    uint32_t seconds = values->play_frames / 60;
    put_number(graphics, 28, 8, (int32_t)(seconds % 60), 2, 0x3300, true);
    put_number(graphics, 25, 8, (int32_t)(seconds / 60 % 60), 2, 0x3300, true);
    put_number(graphics, 22, 8, (int32_t)(seconds / 3600), 2, 0x3300, true);
    put_large_number(graphics, 5, 9, values->level, 2, 3);
    put_number(graphics, 26, 10, (int32_t)values->experience, 6, 0x3300, false);
    put_number(graphics, 25, 12, (int32_t)values->next_experience, 8, 0x3300, false);
    put_number(graphics, 25, 13, (int32_t)values->gold, 8, 0x3300, false);
    /* sub_080471E4: normal/poison/curse/stone status is a four-tile word. */
    unsigned status_variant = values->status & 1 ? 2
                              : values->status & 2 ? 3
                              : values->status & 4 ? 1 : 0;
    put_tiles(graphics, 12, 9, (uint16_t)(0x330A + status_variant * 0x10), 4);

    composite(graphics, rgba);
    return true;
}

static bool read_text(AosPauseGraphics *graphics, uint16_t id,
                      AosNativeText *text) {
    return aos_intro_resources_read_text(&graphics->text, graphics->language,
                                         id, text, NULL, 0);
}

static bool place_text(AosPauseGraphics *graphics, const AosNativeText *text,
                       bool small, unsigned x, unsigned y,
                       unsigned width, unsigned palette,
                       unsigned *next_tile) {
    if (!graphics || !text || !next_tile || !text->line_count || x >= 240 ||
        y >= 160 || !width)
        return false;
    size_t line_count = text->line_count;
    if (line_count > (small ? 1u : 2u)) line_count = small ? 1u : 2u;
    unsigned offset_x = x & 7u, offset_y = y & 7u;
    unsigned line_height = small ? 8u : 12u;
    unsigned height = offset_y + (unsigned)line_count * line_height;
    unsigned tile_width = (offset_x + width + 7u) / 8u;
    unsigned tile_height = (height + 7u) / 8u;
    if (*next_tile + tile_width * tile_height > 0x300u) return false;

    aos_intro_text_tiles_clear(&graphics->text_tiles);
    for (size_t line = 0; line < line_count; ++line) {
        bool rendered = small
            ? aos_intro_font_render_small_span(
                  &graphics->font, &graphics->text_tiles, offset_x,
                  offset_y + (unsigned)line * line_height, width, 8,
                  text->glyphs[line], text->glyph_counts[line])
            : aos_intro_font_render_span(
                  &graphics->font, &graphics->text_tiles, offset_x,
                  offset_y + (unsigned)line * line_height, width, 8,
                  text->glyphs[line], text->glyph_counts[line]);
        if (!rendered) return false;
    }

    unsigned destination_x = x / 8u, destination_y = y / 8u;
    for (unsigned row = 0; row < tile_height; ++row)
        for (unsigned column = 0; column < tile_width; ++column) {
            unsigned tile = (*next_tile)++;
            size_t source = ((size_t)row * 32 + column) * 32;
            memcpy(graphics->bg0_chars + (size_t)tile * 32,
                   graphics->text_tiles.data + source, 32);
            unsigned map_x = destination_x + column;
            unsigned map_y = destination_y + row;
            if (map_x < 32 && map_y < 32)
                graphics->bg0_map[map_y * 32 + map_x] =
                    (uint16_t)(tile | palette << 12);
        }
    return true;
}

static uint16_t soul_name_id(unsigned category, unsigned soul) {
    static const uint16_t bases[] = {0xE2, 0x11A, 0x132};
    return soul ? (uint16_t)(bases[category] + soul) : 0x34F;
}

static uint16_t soul_description_id(unsigned category, unsigned soul) {
    static const uint16_t bases[] = {0x1E3, 0x21B, 0x233};
    static const uint16_t none[] = {0x37D, 0x37E, 0x37F};
    return soul ? (uint16_t)(bases[category] + soul) : none[category];
}

static bool place_text_id(AosPauseGraphics *graphics, uint16_t id, bool small,
                          unsigned x, unsigned y, unsigned width,
                          unsigned palette, unsigned *next_tile) {
    AosNativeText text;
    return read_text(graphics, id, &text) &&
           place_text(graphics, &text, small, x, y, width, palette, next_tile);
}

bool aos_pause_graphics_render_soul_set(
    AosPauseGraphics *graphics, const AosPauseSoulSetValues *values,
    uint8_t *rgba, size_t rgba_size) {
    static const unsigned soul_counts[] = {56, 25, 36};
    static const unsigned selected_palettes[] = {8, 9, 6};
    if (!values || values->category >= AOS_PAUSE_SOUL_FAMILIES ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    for (size_t index = 0; index < 32 * 32; ++index)
        graphics->bg0_map[index] = AOS_PAUSE_BG0_FILL;

    put_number(graphics, 22, 2, values->attack, 3, 0x3300, false);
    put_number(graphics, 22, 3, values->defence, 3, 0x3300, false);
    for (int stat = 0; stat < 4; ++stat)
        put_number(graphics, 22, 4 + stat, values->stats[stat], 3, 0x3300, false);
    unsigned status_variant = values->status & 1 ? 2
                              : values->status & 2 ? 3
                              : values->status & 4 ? 1 : 0;
    put_tiles(graphics, 21, 8, (uint16_t)(0x330A + status_variant * 0x10), 4);

    for (unsigned category = 0; category < AOS_PAUSE_SOUL_FAMILIES; ++category) {
        unsigned palette = category == values->category
                               ? selected_palettes[category]
                               : 3;
        put_tiles(graphics, 6, 3 + (int)category * 2,
                  (uint16_t)((0x350 + category * 0x10) | (palette << 12)), 10);
    }
    for (unsigned entry = 0; entry < 8; ++entry)
        put_tiles(graphics, entry & 1u ? 17 : 3,
                  10 + (int)(entry / 2),
                  (uint16_t)(0x3380 + entry * 0x10), 10);

    unsigned next_tile = 0x200;
    for (unsigned category = 0; category < AOS_PAUSE_SOUL_FAMILIES; ++category) {
        unsigned x = category == 0 ? 126 : category == 1 ? 0 : 126;
        unsigned y = category == 0 ? 80 : 88;
        unsigned palette = category == values->category
                               ? selected_palettes[category]
                               : 3;
        if (!place_text_id(graphics,
                           soul_name_id(category, values->equipped[category]),
                           true, x, y, 84, palette, &next_tile))
            return false;
    }

    unsigned available[AOS_PAUSE_SOUL_RECORDS];
    unsigned available_count = 1;
    available[0] = 0;
    for (unsigned soul = 1; soul < soul_counts[values->category]; ++soul)
        if (values->counts[values->category][soul - 1])
            available[available_count++] = soul;
    unsigned selected_soul = values->cursor[values->category];
    unsigned selected = 0;
    for (unsigned index = 0; index < available_count; ++index)
        if (available[index] == selected_soul) selected = index;
    unsigned top = selected > 3 ? selected - 3 : 0;
    if (top + 8 > available_count)
        top = available_count > 8 ? available_count - 8 : 0;
    for (unsigned entry = 0; entry < 8 && top + entry < available_count; ++entry) {
        unsigned soul = available[top + entry];
        unsigned x = entry & 1u ? 126 : 0;
        unsigned y = (12 + entry / 2) * 8;
        unsigned palette = soul == selected_soul
                               ? selected_palettes[values->category]
                               : 3;
        if (!place_text_id(graphics,
                           soul_name_id(values->category, soul), true,
                           x, y, 84, palette, &next_tile))
            return false;
        put_number(graphics, entry & 1u ? 27 : 13,
                   10 + (int)(entry / 2),
                   soul ? values->counts[values->category][soul - 1] : 0,
                   1, 0x3300, false);
    }
    if (!place_text_id(graphics,
                       soul_description_id(values->category, selected_soul),
                       false, 56, 120, 174, 3, &next_tile))
        return false;

    composite(graphics, rgba);
    return true;
}
