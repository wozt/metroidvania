/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_INTRO_FONT_H
#define AOS_INTRO_FONT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/rom.h"

enum {
    AOS_INTRO_TEXT_TILE_BYTES = 0x8000,
    AOS_INTRO_TEXT_CANVAS_SIZE = 256,
};

typedef struct {
    const uint8_t *metrics;
    const uint8_t *glyphs;
} AosIntroFont;

typedef struct {
    uint8_t data[AOS_INTRO_TEXT_TILE_BYTES];
} AosIntroTextTiles;

bool aos_intro_font_init(AosIntroFont *font, const FusionRom *rom,
                         char *error, size_t error_size);
void aos_intro_text_tiles_clear(AosIntroTextTiles *tiles);
bool aos_intro_font_render_line(const AosIntroFont *font,
                                AosIntroTextTiles *tiles, uint8_t row,
                                const uint8_t *glyphs, size_t glyph_count);
bool aos_intro_font_render_span(const AosIntroFont *font,
                                AosIntroTextTiles *tiles,
                                unsigned pixel_x, unsigned pixel_y,
                                unsigned max_width, uint8_t color,
                                const uint8_t *glyphs, size_t glyph_count);
/* Draws one glyph (code 0x20-0xFF) with its native left offset and width at
 * a pixel position: the glyph rows of sub_0803FF64/sub_08040048. */
bool aos_intro_font_draw_glyph(const AosIntroFont *font, AosIntroTextTiles *tiles,
                               unsigned pixel_x, unsigned pixel_y, uint8_t code);
/* Erases a rectangle of the canvas, clipped to it. */
void aos_intro_text_tiles_clear_rect(AosIntroTextTiles *tiles, unsigned pixel_x,
                                     unsigned pixel_y, unsigned width, unsigned height);
uint8_t aos_intro_text_tiles_pixel(const AosIntroTextTiles *tiles,
                                   unsigned x, unsigned y);

#endif
