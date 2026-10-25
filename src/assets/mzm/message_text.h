/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission message banner and location text, drawn as the game draws
 * it (TextProcessMessageBanner, TextProcessCurrentMessage,
 * TextDrawMessageCharacter and TextDrawLocationTextCharacters of src/text.c)
 * on an OBJ VRAM image. Every table is read from the ROM by the caller. */
#ifndef FUSION_ASSETS_MZM_MESSAGE_TEXT_H
#define FUSION_ASSETS_MZM_MESSAGE_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    FUSION_MZM_LANGUAGE_ENGLISH = 2,
    FUSION_MZM_TEXT_CAPACITY = 4097,
};

typedef struct {
    const uint8_t *gfx;     /* sCharactersGfx */
    size_t gfx_size;
    const uint8_t *widths;  /* sCharacterWidths */
    size_t width_count;
} FusionMzmFont;

/* VRAM as little-endian u32 words, edited in place. */
typedef struct {
    uint8_t *data;
    size_t size;
} FusionMzmTextVram;

/* Runs TextProcessMessageBanner until it reports its line, drawing the text
 * into the banner graphics at byte offset ``base``; ``calls`` receives the
 * number of calls (one a frame). */
bool fusion_mzm_render_message(FusionMzmTextVram *vram, const FusionMzmFont *font,
                               const uint16_t *text, size_t length, size_t base,
                               unsigned *calls);
/* TextDrawLocationTextCharacters(1, ...): the first line of a location text
 * in mask mode at word 0 of ``vram``. */
bool fusion_mzm_draw_location(FusionMzmTextVram *vram, const FusionMzmFont *font,
                              const uint16_t *text, size_t length);

#endif /* FUSION_ASSETS_MZM_MESSAGE_TEXT_H */
