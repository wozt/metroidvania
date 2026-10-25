/* SPDX-License-Identifier: GPL-3.0-only */
/* Transliteration of the US text routines of src/text.c (formerly
 * scripts/mzm_message_text.py). */
#include "message_text.h"

enum {
    STATE_NONE = -1,
    STATE_NOTHING = 0,
    STATE_NEW_LINE = 1,
    STATE_ENDED = 2,
    STATE_UNK_3 = 3,
    STATE_NEW_PAGE = 4,
};

/* The CHAR_*_MASK values of include/constants/text.h. */
enum {
    CHAR_MASK = 0xFF00,
    CHAR_WIDTH_MASK = 0x8000,
    CHAR_COLOR_MASK = 0x8100,
    CHAR_INDENT_MASK = 0x8300,
    CHAR_DELAY_MASK = 0xE100,
    CHAR_NEW_PAGE = 0xFD00,
    CHAR_NEW_LINE = 0xFE00,
    CHAR_TERMINATOR = 0xFF00,
};

#define WORD 0xFFFFFFFFu

typedef struct {
    const uint16_t *text;
    size_t length;
    size_t text_index;
    unsigned indent, timer, color, line, continuous_delay, delay, stage;
    bool ended;
} Message;

static uint32_t read_word(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void write_word(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

/* sArray_7602f0 / sArray_760314: the kept low / high nibbles of a row. */
static uint32_t low_kept(unsigned nibbles) {
    return (uint32_t)(WORD ^ (((uint64_t)WORD << (4 * nibbles)) & WORD));
}

static uint32_t high_kept(unsigned nibbles) {
    return (uint32_t)(WORD ^ ((uint64_t)WORD >> (4 * nibbles)));
}

static unsigned char_width(const FusionMzmFont *font, unsigned character) {
    return character < font->width_count ? font->widths[character] : 10;
}

static bool and_word(FusionMzmTextVram *vram, size_t index, uint32_t mask) {
    if (index >= vram->size / 4) return false;
    uint8_t *data = vram->data + index * 4;
    write_word(data, read_word(data) & mask);
    return true;
}

/* TextDrawMessageCharacter (US): ``dst`` is a VRAM word index. */
static bool draw_character(FusionMzmTextVram *vram, const FusionMzmFont *font,
                           unsigned character, size_t dst, unsigned indent,
                           unsigned color) {
    uint32_t glyph[32] = {0};   /* gCurrentCharacterGfx */
    unsigned width = char_width(font, character);
    for (unsigned part = 0; part < 2; ++part) {
        size_t source = (size_t)character * 0x20 + part * 0x400;
        unsigned size = width > 8 ? 16 : 8;
        for (unsigned i = 0; i < size; ++i) {
            if (source + i * 4 + 4 > font->gfx_size) return false;
            uint32_t pixels = read_word(font->gfx + source + i * 4);
            uint64_t out = 0;
            uint64_t palette = color ? color : 4;
            for (unsigned nibble = 0; nibble < 8; ++nibble) {
                uint64_t value = pixels & ((uint64_t)15 << (4 * nibble));
                if (value)
                    out |= value & ((uint64_t)4 << (4 * nibble))
                        ? palette << (4 * nibble) : value;
                else
                    out |= (uint64_t)15 << (4 * nibble);
            }
            glyph[part * 16 + i] = (uint32_t)(out & WORD);
        }
        size_t base = dst + ((indent & 0xFF) / 8) * 8 + part * 0x100 +
                      (indent / 256) * 0x200;
        const uint32_t *src = glyph + part * 16;
        unsigned offset = indent & 7;
        bool ok = true;
        if (offset) {
            unsigned shift = offset * 4, keep = 8 - offset, rest = 32 - shift;
            if (offset + width > 8) {
                for (unsigned row = 0; row < 8; ++row) {
                    ok = ok && and_word(vram, base + row,
                                        (src[row] << shift) | low_kept(offset)) &&
                         and_word(vram, base + 8 + row,
                                  (src[row] >> rest) | high_kept(keep));
                }
                if (width > 8) {
                    if (offset + width > 16) {
                        for (unsigned row = 0; row < 8; ++row)
                            ok = ok && and_word(vram, base + 8 + row,
                                                (src[8 + row] << shift) | low_kept(offset)) &&
                                 and_word(vram, base + 16 + row,
                                          (src[8 + row] >> rest) | high_kept(keep));
                    } else {
                        for (unsigned row = 0; row < 8; ++row)
                            ok = ok && and_word(vram, base + 8 + row,
                                                (src[8 + row] << shift) | low_kept(offset));
                    }
                }
            } else {
                for (unsigned row = 0; row < 8; ++row)
                    ok = ok && and_word(vram, base + row,
                                        (src[row] << shift) | low_kept(offset));
            }
        } else {
            for (unsigned row = 0; row < 8; ++row)
                ok = ok && and_word(vram, base + row, src[row]);
            if (width > 8)
                for (unsigned row = 0; row < 8; ++row)
                    ok = ok && and_word(vram, base + 8 + row, src[8 + row]);
        }
        if (!ok) return false;
    }
    return true;
}

/* TextProcessCurrentMessage with A not held. */
static bool process_current(Message *message, FusionMzmTextVram *vram,
                            const FusionMzmFont *font, size_t dst, int *result) {
    int state = STATE_NONE;
    message->timer = (message->timer + 1) & 0xFFFF;
    if (message->delay > message->timer) state = STATE_NOTHING;
    if (message->continuous_delay > message->timer) state = STATE_NOTHING;
    if (state >= STATE_NOTHING) {
        *result = state;
        return true;
    }
    message->delay = 0;
    message->timer = 0;
    size_t index = message->text_index;
    while (state < STATE_NOTHING) {
        unsigned width = 0;
        if (index >= message->length) return false;
        unsigned character = message->text[index];
        unsigned kind = character & CHAR_MASK;
        if (kind == CHAR_WIDTH_MASK) {
            width = character & 0xFF;
        } else if (kind == CHAR_INDENT_MASK) {
            message->indent = character & 0xFF;
        } else if (kind == CHAR_COLOR_MASK) {
            message->color = character & 0xFF;
        } else if (kind == CHAR_DELAY_MASK) {
            message->delay = character & 0xFF;
        } else if (kind == CHAR_NEW_LINE) {
            state = STATE_NEW_LINE;
        } else if (kind == CHAR_NEW_PAGE) {
            state = STATE_NEW_PAGE;
        } else if (kind == 0xFC00) {
            state = STATE_UNK_3;
        } else if (kind == CHAR_TERMINATOR) {
            message->ended = true;
            state = STATE_ENDED;
        } else {
            width = char_width(font, character);
            state = STATE_NOTHING;
        }
        if (message->indent + width > 0xE0) {
            /* The overflowing character waits for the next line; the line
             * counter does not move here. */
            state = STATE_NEW_LINE;
            break;
        }
        if (state < STATE_NOTHING) {
            ++index;
        } else if (state == STATE_NOTHING) {
            if (!draw_character(vram, font, character, dst, message->indent,
                                message->color))
                return false;
        } else if (state != STATE_ENDED) {
            ++message->line;
        }
        message->indent = (message->indent + width) & 0xFFFF;
        ++message->text_index;
    }
    *result = state;
    return true;
}

static bool fill(FusionMzmTextVram *vram, size_t offset, size_t size) {
    if (offset > vram->size || size > vram->size - offset) return false;
    for (size_t index = offset / 4; index < (offset + size) / 4; ++index)
        write_word(vram->data + index * 4, WORD);
    return true;
}

/* TextProcessMessageBanner: returns the line once the text is done. */
static bool process_banner(Message *message, FusionMzmTextVram *vram,
                           const FusionMzmFont *font, size_t base, unsigned *line) {
    *line = 0;
    if (message->stage == 0) {
        ++message->stage;
        return fill(vram, base, 0x380) && fill(vram, base + 0x400, 0x380);
    }
    if (message->stage == 1) {
        ++message->stage;
        return fill(vram, base + 0x800, 0x380) && fill(vram, base + 0xC00, 0x380);
    }
    if (message->stage == 2) {
        if (message->line > 1 || message->ended) {
            ++message->stage;
            return true;
        }
        for (unsigned count = 8; count; --count) {
            size_t dst = (base + message->line * 0x800) / 4;
            int state = STATE_NONE;
            if (!process_current(message, vram, font, dst, &state)) return false;
            if (state == STATE_ENDED) ++message->stage;
            if (state == STATE_ENDED || state == STATE_NEW_LINE || state == STATE_NEW_PAGE) {
                message->indent = 0;
                break;
            }
            if (message->line > 1) break;
        }
        return true;
    }
    if (message->stage == 3) {
        ++message->line;
        ++message->stage;
    }
    *line = message->line;
    return true;
}

bool fusion_mzm_render_message(FusionMzmTextVram *vram, const FusionMzmFont *font,
                               const uint16_t *text, size_t length, size_t base,
                               unsigned *calls) {
    Message message = {.text = text, .length = length};
    for (unsigned call = 1; call <= 256; ++call) {
        unsigned line = 0;
        if (!process_banner(&message, vram, font, base, &line)) return false;
        if (line) {
            *calls = call;
            return true;
        }
    }
    return false;
}

bool fusion_mzm_draw_location(FusionMzmTextVram *vram, const FusionMzmFont *font,
                              const uint16_t *text, size_t length) {
    unsigned indent = 0, color = 0;
    for (size_t index = 0; index < length; ++index) {
        unsigned character = text[index], width;
        if (character == CHAR_TERMINATOR || character == CHAR_NEW_LINE) break;
        unsigned kind = character & CHAR_MASK;
        if (kind == CHAR_WIDTH_MASK) {
            width = character & 0xFF;
        } else if (kind == CHAR_INDENT_MASK) {
            width = 0;
            indent = character & 0xFF;
        } else if (kind == CHAR_COLOR_MASK) {
            width = 0;
            color = character & 0xFF;
        } else {
            width = char_width(font, character);
            if (!draw_character(vram, font, character, 0, indent, color)) return false;
        }
        indent += width;
    }
    return true;
}
