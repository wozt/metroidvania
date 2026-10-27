/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_font.h"

#include "aos_intro_entities.h"

#include <stdio.h>
#include <string.h>

enum {
    AOS_ROM_BASE = 0x08000000,
    AOS_FONT_METRICS = 0x080E2B4C,
    AOS_FONT_GLYPHS = 0x0850A136,
    AOS_FONT_FIRST_GLYPH = 0x20,
    AOS_FONT_GLYPH_COUNT = 0xE0,
    AOS_FONT_METRIC_SIZE = 4,
    AOS_FONT_GLYPH_SIZE = 14,
    AOS_FONT_DRAW_ROWS = 12,
    AOS_FONT_FOREGROUND = 8,
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static const uint8_t *slice(const FusionRom *rom, uint32_t pointer,
                            size_t size) {
    if (!rom || rom->game != FUSION_ASSET_GAME_ARIA || !rom->data ||
        pointer < AOS_ROM_BASE)
        return NULL;
    size_t offset = (size_t)(pointer - AOS_ROM_BASE);
    if (offset > rom->size || size > rom->size - offset) return NULL;
    return rom->data + offset;
}

bool aos_intro_font_init(AosIntroFont *font, const FusionRom *rom,
                         char *error, size_t error_size) {
    if (!font) {
        set_error(error, error_size, "invalid Aria introduction font output");
        return false;
    }
    const uint8_t *metrics =
        slice(rom, AOS_FONT_METRICS,
              AOS_FONT_GLYPH_COUNT * AOS_FONT_METRIC_SIZE);
    const uint8_t *glyphs =
        slice(rom, AOS_FONT_GLYPHS,
              AOS_FONT_GLYPH_COUNT * AOS_FONT_GLYPH_SIZE);
    if (!metrics || !glyphs) {
        *font = (AosIntroFont){0};
        set_error(error, error_size,
                  "Aria proportional font tables are outside the ROM");
        return false;
    }
    *font = (AosIntroFont){.metrics = metrics, .glyphs = glyphs};
    if (error && error_size) error[0] = '\0';
    return true;
}

void aos_intro_text_tiles_clear(AosIntroTextTiles *tiles) {
    if (tiles) memset(tiles->data, 0, sizeof tiles->data);
}

static void set_pixel(AosIntroTextTiles *tiles, unsigned x, unsigned y,
                      uint8_t color) {
    if (!tiles || x >= AOS_INTRO_TEXT_CANVAS_SIZE ||
        y >= AOS_INTRO_TEXT_CANVAS_SIZE)
        return;
    size_t tile = (size_t)(y / 8) * 32 + x / 8;
    size_t offset = tile * 32 + (y % 8) * 4 + (x % 8) / 2;
    uint8_t shift = (uint8_t)((x & 1u) * 4u);
    tiles->data[offset] = (uint8_t)((tiles->data[offset] & ~(0xFu << shift)) |
                                    ((color & 0xFu) << shift));
}

uint8_t aos_intro_text_tiles_pixel(const AosIntroTextTiles *tiles,
                                   unsigned x, unsigned y) {
    if (!tiles || x >= AOS_INTRO_TEXT_CANVAS_SIZE ||
        y >= AOS_INTRO_TEXT_CANVAS_SIZE)
        return 0;
    size_t tile = (size_t)(y / 8) * 32 + x / 8;
    size_t offset = tile * 32 + (y % 8) * 4 + (x % 8) / 2;
    return (uint8_t)((tiles->data[offset] >> ((x & 1u) * 4u)) & 0xFu);
}

static void clear_row(AosIntroTextTiles *tiles, uint8_t row) {
    unsigned first_x = AOS_INTRO_TEXT_X * 6;
    unsigned width = AOS_INTRO_TEXT_WIDTH * 6;
    unsigned first_y = (unsigned)(row % AOS_INTRO_TEXT_ROWS) * 12;
    for (unsigned y = first_y; y < first_y + AOS_FONT_DRAW_ROWS; ++y)
        for (unsigned x = first_x; x < first_x + width; ++x)
            set_pixel(tiles, x, y, 0);
}

bool aos_intro_font_render_line(const AosIntroFont *font,
                                AosIntroTextTiles *tiles, uint8_t row,
                                const uint8_t *glyphs, size_t glyph_count) {
    if (!font || !font->metrics || !font->glyphs || !tiles ||
        (!glyphs && glyph_count != 0))
        return false;
    row = (uint8_t)(row % AOS_INTRO_TEXT_ROWS);
    clear_row(tiles, row);

    unsigned cell_x = AOS_INTRO_TEXT_X;
    unsigned pixel_x = cell_x * 6;
    unsigned pixel_y = (unsigned)row * 12;
    unsigned remainder = 0;
    const unsigned last_cell =
        AOS_INTRO_TEXT_X + AOS_INTRO_TEXT_WIDTH - 2;
    for (size_t index = 0; index < glyph_count; ++index) {
        uint8_t code = glyphs[index];
        if (code < AOS_FONT_FIRST_GLYPH) continue;
        size_t glyph_index = (size_t)(code - AOS_FONT_FIRST_GLYPH);
        if (glyph_index >= AOS_FONT_GLYPH_COUNT) continue;
        const uint8_t *metric =
            font->metrics + glyph_index * AOS_FONT_METRIC_SIZE;
        const uint8_t *bitmap =
            font->glyphs + glyph_index * AOS_FONT_GLYPH_SIZE;
        unsigned left = metric[0];
        unsigned width = metric[1];
        if (left > 7 || width > 8 || left + width > 8) return false;

        for (unsigned y = 0; y < AOS_FONT_DRAW_ROWS; ++y) {
            uint8_t bits = (uint8_t)(bitmap[y] << left);
            for (unsigned x = 0; x < width; ++x) {
                uint8_t color = bits & (0x80u >> x) ?
                    AOS_FONT_FOREGROUND : 0;
                set_pixel(tiles, pixel_x + x, pixel_y + y, color);
            }
        }

        remainder += width;
        if (remainder > 5) {
            remainder -= 6;
            ++cell_x;
            if (cell_x > last_cell) {
                cell_x = AOS_INTRO_TEXT_X;
                remainder = 0;
                pixel_x = cell_x * 6;
                continue;
            }
        }
        pixel_x += width;
    }
    return true;
}

bool aos_intro_font_render_span(const AosIntroFont *font,
                                AosIntroTextTiles *tiles,
                                unsigned pixel_x, unsigned pixel_y,
                                unsigned max_width, uint8_t color,
                                const uint8_t *glyphs, size_t glyph_count) {
    if (!font || !font->metrics || !font->glyphs || !tiles ||
        (!glyphs && glyph_count != 0) ||
        pixel_x >= AOS_INTRO_TEXT_CANVAS_SIZE ||
        pixel_y + AOS_FONT_DRAW_ROWS > AOS_INTRO_TEXT_CANVAS_SIZE ||
        max_width > AOS_INTRO_TEXT_CANVAS_SIZE - pixel_x)
        return false;
    for (unsigned y = pixel_y; y < pixel_y + AOS_FONT_DRAW_ROWS; ++y)
        for (unsigned x = pixel_x; x < pixel_x + max_width; ++x)
            set_pixel(tiles, x, y, 0);

    unsigned cursor = 0;
    for (size_t index = 0; index < glyph_count; ++index) {
        uint8_t code = glyphs[index];
        if (code < AOS_FONT_FIRST_GLYPH) continue;
        size_t glyph_index = (size_t)(code - AOS_FONT_FIRST_GLYPH);
        if (glyph_index >= AOS_FONT_GLYPH_COUNT) continue;
        const uint8_t *metric =
            font->metrics + glyph_index * AOS_FONT_METRIC_SIZE;
        const uint8_t *bitmap =
            font->glyphs + glyph_index * AOS_FONT_GLYPH_SIZE;
        unsigned left = metric[0];
        unsigned width = metric[1];
        if (left > 7 || width > 8 || left + width > 8) return false;
        if (cursor + width > max_width) break;
        for (unsigned y = 0; y < AOS_FONT_DRAW_ROWS; ++y) {
            uint8_t bits = (uint8_t)(bitmap[y] << left);
            for (unsigned x = 0; x < width; ++x)
                set_pixel(tiles, pixel_x + cursor + x, pixel_y + y,
                          bits & (0x80u >> x) ? color : 0);
        }
        cursor += width;
    }
    return true;
}
