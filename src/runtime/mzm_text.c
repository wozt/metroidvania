/* SPDX-License-Identifier: GPL-3.0-only */
/* The text engine (see mzm_text.h); comments name the pinned
 * decompilation's routines. */
#include "mzm_text.h"

#include <string.h>

#include "mzm_rom.h"

enum {
    CHAR_MASK = 0xFF00, CHAR_WIDTH_MASK = 0x8000, CHAR_COLOR_MASK = 0x8100, CHAR_INDENT_MASK = 0x8300,
    CHAR_DELAY_MASK = 0xE100, CHAR_UNK_FC = 0xFC00, CHAR_NEW_PAGE = 0xFD00, CHAR_NEW_LINE = 0xFE00,
    CHAR_TERMINATOR = 0xFF00,
    CHARACTER_WIDTHS = 1184, LINE_WIDTH = 0xE0,
    KEY_A = 1 << 0,
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

static uint32_t get32(const uint8_t *p, int i) {
    p += 4 * i;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void or32(uint8_t *p, int i, uint32_t value) {
    p += 4 * i;
    p[0] |= (uint8_t)value;
    p[1] |= (uint8_t)(value >> 8);
    p[2] |= (uint8_t)(value >> 16);
    p[3] |= (uint8_t)(value >> 24);
}

MzmMessage mzm_text_message_from_rom(uint32_t address) {
    MzmMessage m;
    memset(&m, 0, sizeof m);
    if (!address) return m;
    m.text_index = mzm_rom_u16(address);
    m.indent = mzm_rom_u16(address + 2);
    m.timer = mzm_rom_u16(address + 4);
    m.color = mzm_rom_u8(address + 6);
    m.line = mzm_rom_u8(address + 7);
    m.continuous_delay = mzm_rom_u8(address + 8);
    m.delay = mzm_rom_u8(address + 9);
    m.message_id = mzm_rom_u8(address + 10);
    m.gfx_slot = mzm_rom_u8(address + 11);
    m.stage = mzm_rom_u8(address + 12);
    m.is_message = mzm_rom_u8(address + 13) != 0;
    m.message_ended = mzm_rom_u8(address + 14) != 0;
    return m;
}

uint32_t mzm_text_char_width(uint16_t character) {
    if (character >= CHARACTER_WIDTHS) return 10;
    return mzm_rom_u8(sym("sCharacterWidths") + character);
}

void mzm_text_draw_character(uint16_t character, uint8_t *dst, uint16_t indent, uint8_t color) {
    uint8_t gfx[32 * 4];                     /* gCurrentCharacterGfx */
    memset(gfx, 0, sizeof gfx);
    uint32_t width = mzm_text_char_width(character);
    uint32_t characters = sym("sCharactersGfx");
    for (int pass = 0; pass < 2; ++pass) {
        uint8_t *cur = gfx + pass * 64;
        uint32_t src = characters + character * 32u + (uint32_t)pass * 0x400u;
        int size = width > 8 ? 16 : 8;
        for (int i = 0; i < size; ++i) {
            uint32_t pixels = mzm_rom_u32(src + 4u * (uint32_t)i);
            if (color && pixels) {
                /* Nibbles with bit 2 take the color, the others stay. */
                uint32_t out = 0;
                for (int n = 0; n < 8; ++n) {
                    uint32_t value = pixels & 15u << 4 * n;
                    if (!value) continue;
                    out |= value & 4u << 4 * n ? (uint32_t)color << 4 * n : value;
                }
                pixels = out;
            }
            or32(cur, i, pixels);
        }
        uint8_t *out = dst + 4 * (((indent & 0xFF) / 8) * 8 + pass * 0x100 + (indent / 256) * 0x200);
        int shift_tiles = indent & 7;
        if (shift_tiles) {
            int shift = shift_tiles * 4, rest = 32 - shift;
            if (shift_tiles + (int)width > 8) {
                for (int i = 0; i < 8; ++i) {
                    uint32_t value = get32(cur, i);
                    or32(out, i, value << shift);
                    or32(out, i + 8, value >> rest);
                }
                if (width > 8) {
                    if (shift_tiles + (int)width > 16) {
                        for (int i = 8; i < 16; ++i) {
                            uint32_t value = get32(cur, i);
                            or32(out, i, value << shift);
                            or32(out, i + 8, value >> rest);
                        }
                    } else {
                        for (int i = 8; i < 16; ++i) or32(out, i, get32(cur, i) << shift);
                    }
                }
            } else {
                for (int i = 0; i < 8; ++i) or32(out, i, get32(cur, i) << shift);
            }
        } else {
            for (int i = 0; i < 8; ++i) or32(out, i, get32(cur, i));
            if (width > 8)
                for (int i = 8; i < 16; ++i) or32(out, i, get32(cur, i));
        }
    }
}

int mzm_text_process_message(MzmMessage *m, uint32_t text, uint8_t *dst, uint16_t button_input) {
    int state = MZM_TEXT_NONE;
    m->timer++;
    if (!(button_input & KEY_A)) {
        if (m->delay > m->timer) state = MZM_TEXT_NOTHING;
        if (m->continuous_delay > m->timer) state = MZM_TEXT_NOTHING;
    }
    if (state >= MZM_TEXT_NOTHING) return state;
    m->delay = 0;
    m->timer = 0;
    uint32_t at = text + m->text_index * 2u;
    while (state < MZM_TEXT_NOTHING) {
        uint16_t c = mzm_rom_u16(at);
        uint32_t width = 0;
        switch (c & CHAR_MASK) {
        case CHAR_WIDTH_MASK: width = c & 0xFF; break;
        case CHAR_INDENT_MASK: m->indent = c & 0xFF; break;
        case CHAR_COLOR_MASK: m->color = (uint8_t)c; break;
        case CHAR_DELAY_MASK: m->delay = (uint8_t)c; break;
        case CHAR_NEW_LINE: state = MZM_TEXT_NEW_LINE; break;
        case CHAR_NEW_PAGE: state = MZM_TEXT_NEW_PAGE; break;
        case CHAR_UNK_FC: state = MZM_TEXT_UNK_3; break;
        case CHAR_TERMINATOR:
            m->message_ended = true;
            state = MZM_TEXT_ENDED;
            break;
        default:
            width = mzm_text_char_width(c);
            state = MZM_TEXT_NOTHING;
            break;
        }
        /* A line overflow: the character is drawn on the next line. */
        if (m->indent + width > LINE_WIDTH) {
            state = MZM_TEXT_NEW_LINE;
            break;
        }
        if (state < MZM_TEXT_NOTHING) at += 2;
        else if (state == MZM_TEXT_NOTHING) mzm_text_draw_character(c, dst, m->indent, m->color);
        else if (state != MZM_TEXT_ENDED) m->line++;
        m->indent = (uint16_t)(m->indent + width);
        m->text_index++;
    }
    return state;
}
