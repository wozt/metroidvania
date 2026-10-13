/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission text engine (text.c): the character widths, the append
 * mode character drawing into a 4bpp tile buffer and the message processing
 * of the status screen descriptions and the messages, read from the ROM
 * bank's font (sCharactersGfx, sCharacterWidths) and text. */
#ifndef MZM_TEXT_H
#define MZM_TEXT_H

#include <stdbool.h>
#include <stdint.h>

/* TEXT_STATE_* */
enum {
    MZM_TEXT_NONE = -1, MZM_TEXT_NOTHING = 0, MZM_TEXT_NEW_LINE = 1, MZM_TEXT_ENDED = 2,
    MZM_TEXT_UNK_3 = 3, MZM_TEXT_NEW_PAGE = 4
};

/* struct Message */
typedef struct {
    uint16_t text_index, indent, timer;
    uint8_t color, line, continuous_delay, delay;
    uint8_t message_id, gfx_slot, stage;
    bool is_message, message_ended;
} MzmMessage;

/* A 16-byte struct Message in ROM (sMessageDescription_Empty...). */
MzmMessage mzm_text_message_from_rom(uint32_t address);
/* TextGetCharacterWidth */
uint32_t mzm_text_char_width(uint16_t character);
/* TextDrawCharacter: ORs the character into the two 32-tile rows of a 4bpp
 * buffer (`dst`, 0x800 bytes for a 224-pixel line) at `indent` pixels. */
void mzm_text_draw_character(uint16_t character, uint8_t *dst, uint16_t indent, uint8_t color);
/* TextDrawMessageCharacter: the mask mode, ANDed into a buffer filled
 * with color 15. */
void mzm_text_draw_message_character(uint16_t character, uint8_t *dst, uint16_t indent, uint8_t color);
/* TextProcessCurrentMessage on the u16 text at the ROM address `text` (the
 * mask mode when `is_message`); `button_input` is gButtonInput (GBA key
 * bits). Returns a MZM_TEXT_*. */
int mzm_text_process_message(MzmMessage *message, uint32_t text, uint8_t *dst, uint16_t button_input);

#endif
