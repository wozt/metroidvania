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
    AOS_WEAPON_RECORDS = 0x08505D3C,
    AOS_ARMOR_RECORDS = 0x085063B0,
    AOS_EQUIPMENT_NAME_IDS = 0x08506734,
    AOS_EQUIPMENT_DESCRIPTION_IDS = 0x08506936,
    AOS_ROM_BASE = 0x08000000,
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
        memcpy(graphics->bg0_base_chars, memory->vram + 0x4000,
               sizeof graphics->bg0_base_chars);
        memcpy(graphics->bg0_chars, graphics->bg0_base_chars,
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

static void composite_on_backdrop(const AosPauseGraphics *graphics,
                                  uint8_t *rgba) {
    for (unsigned y = 0; y < AOS_PAUSE_HEIGHT; ++y)
        for (unsigned x = 0; x < AOS_PAUSE_WIDTH; ++x) {
            size_t out = ((size_t)y * AOS_PAUSE_WIDTH + x) * 4;
            memcpy(rgba + out, graphics->backdrop_rgb, 3);
            rgba[out + 3] = 255;
            bg0_pixel(graphics, x, y, rgba + out);
        }
}

static void reset_bg0(AosPauseGraphics *graphics) {
    memcpy(graphics->bg0_chars, graphics->bg0_base_chars,
           sizeof graphics->bg0_chars);
    for (size_t index = 0; index < 32 * 32; ++index)
        graphics->bg0_map[index] = AOS_PAUSE_BG0_FILL;
}

static void clear_bg0_tiles(AosPauseGraphics *graphics,
                            unsigned first, unsigned count) {
    if (first >= 0x400 || count > 0x400 - first) return;
    memset(graphics->bg0_chars + (size_t)first * 32, 0,
           (size_t)count * 32);
}

bool aos_pause_graphics_render_hub(AosPauseGraphics *graphics,
                                   const AosPauseHubValues *values,
                                   uint8_t selected, uint8_t *rgba,
                                   size_t rgba_size) {
    if (!values || selected >= 7 ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    reset_bg0(graphics);

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
    if (*next_tile + tile_width * tile_height > 0x200u) return false;

    aos_intro_text_tiles_clear(&graphics->text_tiles);
    for (size_t line = 0; line < line_count; ++line) {
        bool rendered = small
            ? aos_intro_font_render_small_span(
                  &graphics->font, &graphics->text_tiles, offset_x,
                  offset_y + (unsigned)line * line_height, width, 1,
                  text->glyphs[line], text->glyph_counts[line])
            : aos_intro_font_render_span(
                  &graphics->font, &graphics->text_tiles, offset_x,
                  offset_y + (unsigned)line * line_height, width, 1,
                  text->glyphs[line], text->glyph_counts[line]);
        if (!rendered) return false;
    }

    unsigned destination_x = x / 8u, destination_y = y / 8u;
    for (unsigned row = 0; row < tile_height; ++row)
        for (unsigned column = 0; column < tile_width; ++column) {
            unsigned tile = (*next_tile)++;
            size_t source = ((size_t)row * 32 + column) * 32;
            unsigned map_x = destination_x + column;
            unsigned map_y = destination_y + row;
            uint8_t *destination =
                graphics->bg0_chars + (size_t)tile * 32;
            if (map_x < 32 && map_y < 32) {
                uint16_t previous = graphics->bg0_map[map_y * 32 + map_x];
                memcpy(destination,
                       graphics->bg0_chars + (size_t)(previous & 0x3FF) * 32,
                       32);
                const uint8_t *glyph = graphics->text_tiles.data + source;
                for (unsigned byte = 0; byte < 32; ++byte) {
                    uint8_t low = glyph[byte] & 0x0F;
                    uint8_t high = glyph[byte] & 0xF0;
                    if (low) destination[byte] =
                        (uint8_t)((destination[byte] & 0xF0) | low);
                    if (high) destination[byte] =
                        (uint8_t)((destination[byte] & 0x0F) | high);
                }
                graphics->bg0_map[map_y * 32 + map_x] =
                    (uint16_t)(tile | palette << 12);
            }
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
    reset_bg0(graphics);
    /* sub_08041204 clears the language-bank placeholders before the small
     * font renderer rewrites their character data. */
    clear_bg0_tiles(graphics, 0x380, 0x80);

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

    unsigned next_tile = 0;
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

    composite_on_backdrop(graphics, rgba);
    return true;
}

static bool rom_u16(const AosPauseGraphics *graphics, uint32_t address,
                    uint16_t *value) {
    if (!graphics || !graphics->text.rom || !graphics->text.rom->data ||
        !value || address < AOS_ROM_BASE)
        return false;
    size_t offset = (size_t)(address - AOS_ROM_BASE);
    if (offset + 2 > graphics->text.rom->size) return false;
    const uint8_t *bytes = graphics->text.rom->data + offset;
    *value = (uint16_t)(bytes[0] | bytes[1] << 8);
    return true;
}

static bool equipment_text_ids(const AosPauseGraphics *graphics,
                               unsigned slot, unsigned item,
                               uint16_t *name, uint16_t *description) {
    if (slot >= AOS_PAUSE_EQUIP_SLOTS || !name || !description) return false;
    if (item == AOS_PAUSE_EQUIP_NONE) {
        *name = 0x34F;
        *description = 0x351;
        return true;
    }
    if ((slot == 0 && item >= 59) || (slot != 0 && item >= 45)) return false;
    uint32_t records = slot == 0 ? AOS_WEAPON_RECORDS : AOS_ARMOR_RECORDS;
    unsigned record_size = slot == 0 ? 28 : 20;
    uint16_t text_index;
    return rom_u16(graphics, records + item * record_size, &text_index) &&
           rom_u16(graphics, AOS_EQUIPMENT_NAME_IDS + text_index * 2, name) &&
           rom_u16(graphics,
                   AOS_EQUIPMENT_DESCRIPTION_IDS + text_index * 2,
                   description);
}

static void put_comparison(AosPauseGraphics *graphics, int y,
                           int16_t current, int16_t preview) {
    unsigned kind = preview == current ? 0 : preview > current ? 1 : 2;
    static const uint16_t tiles[] = {0x34C, 0x34A, 0x34B};
    static const unsigned palettes[] = {3, 9, 8};
    put_tiles(graphics, 24, y,
              (uint16_t)(tiles[kind] | palettes[kind] << 12), 2);
    put_number(graphics, 27, y, preview, 3, 0x3300, false);
}

bool aos_pause_graphics_render_equip(AosPauseGraphics *graphics,
                                     const AosPauseEquipValues *values,
                                     uint8_t *rgba, size_t rgba_size) {
    if (!values || values->slot >= AOS_PAUSE_EQUIP_SLOTS ||
        values->item_count > AOS_PAUSE_EQUIP_VISIBLE ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    reset_bg0(graphics);
    clear_bg0_tiles(graphics, 0x340, 0x80);

    put_number(graphics, 22, 2, values->current[0], 3, 0x3300, false);
    put_number(graphics, 22, 3, values->current[1], 3, 0x3300, false);
    for (int stat = 0; stat < 4; ++stat)
        put_number(graphics, 22, 5 + stat, values->current[2 + stat],
                   3, 0x3300, false);
    if (values->preview_active) {
        put_comparison(graphics, 2, values->current[0], values->preview[0]);
        put_comparison(graphics, 3, values->current[1], values->preview[1]);
        for (int stat = 0; stat < 4; ++stat)
            put_comparison(graphics, 5 + stat, values->current[2 + stat],
                           values->preview[2 + stat]);
    }

    put_tiles(graphics, 2, 3, (uint16_t)(0x33D0 + values->slot * 0x10), 11);
    put_tiles(graphics, 3, 5, 0x3310, 9);
    put_tiles(graphics, 3, 7, 0x3320, 9);
    put_tiles(graphics, 3, 9, 0x3330, 9);
    for (unsigned entry = 0; entry < AOS_PAUSE_EQUIP_VISIBLE; ++entry)
        put_tiles(graphics, entry & 1u ? 17 : 3,
                  11 + (int)(entry / 2),
                  (uint16_t)(0x3340 + entry * 0x10), 9);
    for (unsigned row = 0; row < 3; ++row)
        put_tiles(graphics, 5, 16 + (int)row,
                  (uint16_t)(0x3200 + row * 0x20), 24);

    unsigned next_tile = 0;
    static const unsigned equipped_x[] = {126, 0, 126};
    static const unsigned equipped_y[] = {64, 72, 72};
    for (unsigned slot = 0; slot < AOS_PAUSE_EQUIP_SLOTS; ++slot) {
        uint16_t name, description;
        if (!equipment_text_ids(graphics, slot, values->equipped[slot],
                                &name, &description) ||
            !place_text_id(graphics, name, true, equipped_x[slot],
                           equipped_y[slot], 84,
                           slot == values->slot ? 9 : 3, &next_tile))
            return false;
    }

    for (unsigned entry = 0; entry < values->item_count; ++entry) {
        uint16_t name, description;
        unsigned item = values->items[entry];
        if (!equipment_text_ids(graphics, values->slot, item,
                                &name, &description))
            return false;
        unsigned x = entry & 1u ? 126 : 0;
        unsigned y = (10 + entry / 2) * 8;
        if (!place_text_id(graphics, name, true, x, y, 78,
                           item == values->selected_item ? 9 : 3,
                           &next_tile))
            return false;
        put_number(graphics, entry & 1u ? 27 : 13,
                   11 + (int)(entry / 2), values->quantities[entry],
                   1, 0x3300, false);
    }

    uint16_t name, description;
    if (!equipment_text_ids(graphics, values->slot, values->selected_item,
                            &name, &description))
        return false;
    if (values->selected_item == AOS_PAUSE_EQUIP_NONE && values->preview_active)
        description = 0x350;
    if (!place_text_id(graphics, description, false, 40, 128, 192,
                       3, &next_tile))
        return false;

    composite_on_backdrop(graphics, rgba);
    return true;
}

bool aos_pause_graphics_render_config(AosPauseGraphics *graphics,
                                      const uint8_t config[AOS_BUTTON_ACTIONS],
                                      uint8_t *rgba, size_t rgba_size) {
    if (!config || !valid_output(graphics, rgba, rgba_size)) return false;
    reset_bg0(graphics);
    /* sub_0804C264: four rows at tile rows 4,6,8,10. The action label is a
     * baked seven- or six-tile strip at entry 0x3359+row*0x10 (row 1 is six
     * tiles wide natively); the assigned button name is the three-tile
     * strip at 0x32A0+index*3 in column 0x12. Row order follows table
     * 0x08525510 {0,1,3,2} = attack, jump, guardian, ability. */
    static const uint8_t row_actions[AOS_BUTTON_ACTIONS] = {0, 1, 3, 2};
    static const unsigned label_tiles[AOS_BUTTON_ACTIONS] = {7, 6, 7, 7};
    for (unsigned row = 0; row < AOS_BUTTON_ACTIONS; ++row) {
        unsigned y = 4 + row * 2;
        put_tiles(graphics, 6, (int)y,
                  (uint16_t)(0x3359 + row * 0x10), label_tiles[row]);
        unsigned index = config[row_actions[row]];
        if (index > 3) index = 0;
        put_tiles(graphics, 0x12, (int)y,
                  (uint16_t)(0x32A0 + index * 3), 3);
    }
    composite(graphics, rgba);
    return true;
}

/* sub_080472B0: fills a tilemap rectangle with consecutive tile indices.
 * Native column/row counts derive from the last two arguments as
 * (arg3 * 6) >> 3 and (arg4 * 12) >> 3; tile (i, j) is arg2 + i * 32 + j. */
static void window_fill(AosPauseGraphics *graphics, unsigned column,
                        unsigned row, uint16_t tile_base, unsigned dim_a,
                        unsigned dim_b) {
    unsigned columns = (dim_a * 3 * 2) >> 3;
    unsigned rows = (dim_b * 3 * 4) >> 3;
    for (unsigned i = 0; i < rows; ++i)
        for (unsigned j = 0; j < columns; ++j) {
            size_t cell = (size_t)(row + i) * 32 + column + j;
            if (cell < 32 * 32)
                graphics->bg0_map[cell] =
                    (uint16_t)(tile_base + i * 32 + j);
        }
}

bool aos_pause_graphics_render_ability(AosPauseGraphics *graphics,
                                       const AosPauseAbilityValues *values,
                                       uint8_t *rgba, size_t rgba_size) {
    if (!values || values->cursor >= AOS_PAUSE_ABILITY_COUNT ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    reset_bg0(graphics);
    /* sub_08041204-style placeholder clear shared with the soul pages. */
    clear_bg0_tiles(graphics, 0x380, 0x80);
    /* sub_0804BA18 init: the bottom message window then, for each of the
     * six abilities, a ten-tile strip at column 3 (tiles 0x3380+i*0x20)
     * and one at column 0x11 (tiles 0x3390+i*0x20), rows 4+2i. */
    window_fill(graphics, 4, 0xD, 0x3200, 0x1F, 4);
    for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i) {
        put_tiles(graphics, 3, (int)(4 + i * 2),
                  (uint16_t)(0x3380 + i * 0x20), 10);
        put_tiles(graphics, 0x11, (int)(4 + i * 2),
                  (uint16_t)(0x3390 + i * 0x20), 10);
    }
    /* sub_0804BB40's per-frame loop: each owned ability gets a two-tile
     * ON/OFF marker. Column is 0xD for even indices, 0x1B for odd; row is
     * 4 + 2*(i>>1). sub_08032AB8 returns the ability's mask bit, so
     * (state >> i) << 4 adds 0x10 to 0xB30E when enabled (0xB31E). */
    for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i) {
        if (!values->owned[i]) continue;
        put_tiles(graphics, (int)(i & 1 ? 0x1B : 0xD),
                  (int)(4 + (i >> 1) * 2),
                  (uint16_t)(values->enabled[i] ? 0xB31E : 0xB30E), 2);
    }
    /* Names print through the context-B cursor: window column 0 or 0x15
     * and row 0xC + i/2 give pixel (0|126, (12+i/2)*8); the window spans
     * 14 cells so text wraps at about 78 pixels. */
    unsigned next_tile = 0;
    for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i) {
        if (!place_text_id(graphics,
                           values->owned[i] ? (uint16_t)(0x156 + i) : 0x34F,
                           true, (i & 1u) ? 126u : 0u,
                           (12u + i / 2u) * 8u, 78, 3, &next_tile))
            return false;
    }
    composite(graphics, rgba);
    return true;
}

bool aos_pause_graphics_render_item_use(
    AosPauseGraphics *graphics, const AosPauseItemUseValues *values,
    uint8_t *rgba, size_t rgba_size) {
    if (!values ||
        (values->selected != 0xFF &&
         values->selected >= AOS_PAUSE_ITEM_USE_RECORDS) ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    reset_bg0(graphics);
    clear_bg0_tiles(graphics, 0x380, 0x80);
    /* sub_0804B6C4: four slot rows of strips at columns 2 and 0x11, tile
     * rows 11-14 (entries 0x3380/0x3390+i*0x20), then three 24-tile
     * description bars at column 5, rows 16-18 (0x3200+i*0x20). */
    for (unsigned i = 0; i < 4; ++i) {
        put_tiles(graphics, 2, (int)(11 + i),
                  (uint16_t)(0x3380 + i * 0x20), 10);
        put_tiles(graphics, 0x11, (int)(11 + i),
                  (uint16_t)(0x3390 + i * 0x20), 10);
    }
    for (unsigned i = 0; i < 3; ++i)
        put_tiles(graphics, 5, (int)(16 + i),
                  (uint16_t)(0x3200 + i * 0x20), 24);
    /* sub_0804B494: owned items in slot order fill the 4x2 grid; names
     * are text 0x5B+item at context-B cells column 0/0x15 row 0xC+j/2,
     * quantities are 0x3300 digits ending at column 0xD/0x1C row
     * 0xB+j/2, and unused slots get two 0x330D dashes (sub_08047098). */
    uint8_t owned[AOS_PAUSE_ITEM_USE_RECORDS];
    unsigned owned_count = 0;
    for (unsigned item = 0; item < AOS_PAUSE_ITEM_USE_RECORDS; ++item)
        if (values->counts[item]) owned[owned_count++] = (uint8_t)item;
    unsigned next_tile = 0;
    for (unsigned slot = 0; slot < AOS_PAUSE_ITEM_USE_VISIBLE; ++slot) {
        unsigned ordinal = values->top + slot;
        bool filled = ordinal < owned_count;
        uint8_t item = filled ? owned[ordinal] : 0;
        unsigned x = slot & 1u ? 126u : 0u;
        unsigned y = (12u + slot / 2u) * 8u;
        if (filled &&
            !place_text_id(graphics, (uint16_t)(0x5B + item), true, x, y,
                           78, 3, &next_tile))
            return false;
        unsigned quantity_x = slot & 1u ? 0x1Cu : 0xDu;
        unsigned quantity_y = 0xBu + slot / 2u;
        if (filled)
            put_number(graphics, (int)quantity_x, (int)quantity_y,
                       values->counts[item], 1, 0x3300, false);
        else {
            /* sub_08047098 writes the same 0x330D tile to both cells,
             * walking left from the quantity column. */
            size_t dash = (size_t)quantity_y * 32 + quantity_x;
            if (dash < 32 * 32) graphics->bg0_map[dash] = 0x330D;
            if (dash >= 1 && dash - 1 < 32 * 32)
                graphics->bg0_map[dash - 1] = 0x330D;
        }
    }
    /* The selected item's description (text 0x15C+item via table
     * 0x08506936) prints into the bottom message window; its exact
     * cursor placement is approximated on the description bars. */
    if (values->selected < AOS_PAUSE_ITEM_USE_RECORDS &&
        values->counts[values->selected] &&
        !place_text_id(graphics, (uint16_t)(0x15C + values->selected),
                       false, 40, 130, 192, 3, &next_tile))
        return false;
    composite(graphics, rgba);
    return true;
}

static bool enemy_defeated(const AosPauseEnemyValues *values, unsigned enemy) {
    return enemy < AOS_PAUSE_ENEMY_RECORDS &&
           (values->defeated[enemy >> 5] & (1u << (enemy & 31))) != 0;
}

bool aos_pause_graphics_render_enemy(AosPauseGraphics *graphics,
                                     const AosPauseEnemyValues *values,
                                     uint8_t *rgba, size_t rgba_size) {
    if (!values || values->top >= AOS_PAUSE_ENEMY_RECORDS ||
        values->cursor >= AOS_PAUSE_ENEMY_RECORDS ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    reset_bg0(graphics);
    clear_bg0_tiles(graphics, 0x380, 0x80);
    unsigned next_tile = 0;
    for (unsigned slot = 0; slot < AOS_PAUSE_ENEMY_PAGE; ++slot) {
        unsigned enemy = values->top + slot;
        if (enemy >= AOS_PAUSE_ENEMY_RECORDS) break;
        /* sub_0804D2C8 intro loop: ten-tile row strips at columns 6/0x14,
         * rows 5+2*(slot/2), entries 0xF320+slot*0x10 (palette 15). */
        put_tiles(graphics, (int)(slot & 1u ? 0x14 : 6),
                  (int)(5 + (slot / 2) * 2),
                  (uint16_t)(0xF320 + slot * 0x10), 10);
        /* Names through the context-B cursor: column 0/0x15, row
         * 8+slot/2 -> pixel (0|126, (8+slot/2)*8). Defeated enemies use
         * text 0x25D+enemy; undiscovered ones 0x34E "????????". */
        uint16_t id = enemy_defeated(values, enemy)
                          ? (uint16_t)(0x25D + enemy) : 0x34E;
        if (!place_text_id(graphics, id, true, slot & 1u ? 126u : 0u,
                           (8u + slot / 2u) * 8u, 110, 3, &next_tile))
            return false;
        /* Enemy number via the two-row digit path (sub_08046FFC),
         * right-aligned ending at column 5/0x15 on the strip row. */
        put_large_number(graphics, (int)(slot & 1u ? 0x15 : 5),
                         (int)(5 + (slot / 2) * 2), enemy + 1,
                         enemy + 1 > 99 ? 3 : 2, 3);
    }
    composite(graphics, rgba);
    return true;
}

bool aos_pause_graphics_render_sleep(AosPauseGraphics *graphics,
                                     bool can_sleep, uint8_t *rgba,
                                     size_t rgba_size) {
    if (!valid_output(graphics, rgba, rgba_size)) return false;
    reset_bg0(graphics);
    unsigned next_tile = 0;
    if (!can_sleep) {
        /* sub_0804BD38's refusal: text 0x361 "Cannot save until you have
         * saved once in a save room." */
        if (!place_text_id(graphics, 0x361, false, 24, 64, 192, 3,
                           &next_tile))
            return false;
    } else {
        /* sub_0804BD38/sub_0804BE0C: text 0x36A "Pause the game?" then the
         * Yes/No labels. Native cursor cells (4,1) and (0xC,1) of
         * sub_0804066C map to pixel (24,128) and (72,128) on the pause
         * background's bottom text band; the question sits one 12-pixel
         * row above. The selection arrow is OBJ and stays unrendered. */
        if (!place_text_id(graphics, 0x36A, true, 24, 116, 192, 3,
                           &next_tile) ||
            !place_text_id(graphics, 0x34B, true, 24, 128, 96, 3,
                           &next_tile) ||
            !place_text_id(graphics, 0x34D, true, 72, 128, 96, 3,
                           &next_tile))
            return false;
    }
    composite(graphics, rgba);
    return true;
}
