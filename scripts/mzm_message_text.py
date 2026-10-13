# SPDX-License-Identifier: GPL-3.0-only
"""Zero Mission message banner text, drawn as the game draws it.

``TextProcessMessageBanner`` (src/text.c) fills the banner's text tiles with
color 15, then ``TextProcessCurrentMessage`` reads the message's u16 text
(``sMessageTextPointers[LANGUAGE_ENGLISH]``) and ``TextDrawMessageCharacter``
ANDs each glyph of ``sCharactersGfx`` (two 8-row passes, widths from
``sCharacterWidths``) into OBJ VRAM, eight characters a frame. This module
transliterates those routines on a VRAM image so the exporter can render the
banner's frames with their text, and counts the ``TextProcessMessageBanner``
calls until it reports its line (the banner's pop-up processing time).
Nothing here is stored: every table is read from the ROM by the caller.
"""
from __future__ import annotations

import struct

WORD = 0xFFFFFFFF
LANGUAGE_ENGLISH = 2

# TEXT_STATE_*
NONE, NOTHING, NEW_LINE, ENDED, UNK_3, NEW_PAGE = -1, 0, 1, 2, 3, 4
# The CHAR_*_MASK values of include/constants/text.h.
CHAR_MASK = 0xFF00
CHAR_WIDTH_MASK = 0x8000
CHAR_COLOR_MASK = 0x8100
CHAR_INDENT_MASK = 0x8300
CHAR_DELAY_MASK = 0xE100
CHAR_NEW_PAGE = 0xFD00
CHAR_NEW_LINE = 0xFE00
CHAR_TERMINATOR = 0xFF00

# sArray_7602f0 / sArray_760314: the kept low / high nibbles of a row.
LOW_KEPT = [WORD ^ ((WORD << (4 * i)) & WORD) for i in range(9)]
HIGH_KEPT = [WORD ^ (WORD >> (4 * i)) for i in range(9)]


class Vram:
    """OBJ VRAM as little-endian u32 words."""

    def __init__(self, data: bytes):
        self.words = list(struct.unpack(f"<{len(data) // 4}I", data))

    def fill(self, byte_offset: int, size: int, value: int = WORD) -> None:
        for index in range(byte_offset // 4, (byte_offset + size) // 4):
            self.words[index] = value

    def to_bytes(self) -> bytes:
        return struct.pack(f"<{len(self.words)}I", *self.words)


class Font:
    def __init__(self, gfx: bytes, widths: bytes):
        self.gfx = gfx
        self.widths = widths

    def width(self, char: int) -> int:
        """TextGetCharacterWidth"""
        return self.widths[char] if char < len(self.widths) else 10

    def word(self, byte_offset: int) -> int:
        return struct.unpack_from("<I", self.gfx, byte_offset)[0]


def draw_character(vram: Vram, font: Font, char: int, dst: int, indent: int, color: int) -> None:
    """TextDrawMessageCharacter (US): `dst` is a VRAM word index."""
    glyph = [0] * 32                        # gCurrentCharacterGfx
    width = font.width(char)
    for part in range(2):
        source = char * 0x20 + part * 0x400
        size = 16 if width > 8 else 8
        for i in range(size):
            pixels = font.word(source + i * 4)
            out = 0
            palette = color if color != 0 else 4
            for nibble in range(8):
                value = pixels & (15 << (4 * nibble))
                if value:
                    out |= (palette << (4 * nibble)) if value & (4 << (4 * nibble)) else value
                else:
                    out |= 15 << (4 * nibble)
            glyph[part * 16 + i] = out & WORD
        base = dst + ((indent & 0xFF) // 8) * 8 + part * 0x100 + (indent // 256) * 0x200
        src = glyph[part * 16:part * 16 + 16]
        words = vram.words
        offset = indent & 7
        if offset:
            shift, keep = offset * 4, 8 - offset
            size = 32 - shift
            if offset + width > 8:
                for row in range(8):
                    value = src[row]
                    words[base + row] &= ((value << shift) & WORD) | LOW_KEPT[offset]
                    words[base + 8 + row] &= (value >> size) | HIGH_KEPT[keep]
                if width > 8:
                    if offset + width > 16:
                        for row in range(8):
                            value = src[8 + row]
                            words[base + 8 + row] &= ((value << shift) & WORD) | LOW_KEPT[offset]
                            words[base + 16 + row] &= (value >> size) | HIGH_KEPT[keep]
                    else:
                        for row in range(8):
                            words[base + 8 + row] &= ((src[8 + row] << shift) & WORD) | LOW_KEPT[offset]
            else:
                for row in range(8):
                    words[base + row] &= ((src[row] << shift) & WORD) | LOW_KEPT[offset]
        else:
            for row in range(8):
                words[base + row] &= src[row]
            if width > 8:
                for row in range(8):
                    words[base + 8 + row] &= src[8 + row]


class Message:
    """struct Message as sMessage_Empty starts it (isMessage TRUE)."""

    def __init__(self, text: list[int]):
        self.text = text
        self.text_index = 0
        self.indent = 0
        self.timer = 0
        self.color = 0
        self.line = 0
        self.continuous_delay = 0
        self.delay = 0
        self.stage = 0
        self.ended = False


def process_current(message: Message, vram: Vram, font: Font, dst: int) -> int:
    """TextProcessCurrentMessage with A not held."""
    state = NONE
    message.timer = (message.timer + 1) & 0xFFFF
    if message.delay > message.timer:
        state = NOTHING
    if message.continuous_delay > message.timer:
        state = NOTHING
    if state >= NOTHING:
        return state
    message.delay = 0
    message.timer = 0
    index = message.text_index
    while state < NOTHING:
        width = 0
        char = message.text[index]
        kind = char & CHAR_MASK
        if kind == CHAR_WIDTH_MASK:
            width = char & 0xFF
        elif kind == CHAR_INDENT_MASK:
            message.indent = char & 0xFF
        elif kind == CHAR_COLOR_MASK:
            message.color = char & 0xFF
        elif kind == CHAR_DELAY_MASK:
            message.delay = char & 0xFF
        elif kind == CHAR_NEW_LINE:
            state = NEW_LINE
        elif kind == CHAR_NEW_PAGE:
            state = NEW_PAGE
        elif kind == 0xFC00:
            state = UNK_3
        elif kind == CHAR_TERMINATOR:
            message.ended = True
            state = ENDED
        else:
            width = font.width(char)
            state = NOTHING
        if message.indent + width > 0xE0:
            # The overflowing character waits for the next line; the line
            # counter does not move here.
            state = NEW_LINE
            break
        if state < NOTHING:
            index += 1
        elif state == NOTHING:
            draw_character(vram, font, char, dst, message.indent, message.color)
        elif state != ENDED:
            message.line += 1
        message.indent = (message.indent + width) & 0xFFFF
        message.text_index += 1
    return state


def process_banner(message: Message, vram: Vram, font: Font, base: int) -> int:
    """TextProcessMessageBanner: `base` is the byte offset of the banner's
    graphics slot in `vram`; returns the line once the text is done."""
    if message.stage == 0:
        vram.fill(base, 0x380)
        vram.fill(base + 0x400, 0x380)
        message.stage += 1
        return 0
    if message.stage == 1:
        vram.fill(base + 0x800, 0x380)
        vram.fill(base + 0xC00, 0x380)
        message.stage += 1
        return 0
    if message.stage == 2:
        if message.line > 1 or message.ended:
            message.stage += 1
            return 0
        count = 8
        while count:
            dst = (base + message.line * 0x800) // 4
            state = process_current(message, vram, font, dst)
            if state == ENDED:
                message.stage += 1
            if state in (ENDED, NEW_LINE, NEW_PAGE):
                message.indent = 0
                break
            if message.line > 1:
                break
            count -= 1
        return 0
    if message.stage == 3:
        message.line += 1
        message.stage += 1
    return message.line


def draw_location(vram: Vram, font: Font, text: list[int]) -> None:
    """TextDrawLocationTextCharacters(1, ...): the first line of a location
    text, in mask mode, at word 0 of `vram`."""
    indent = color = 0
    for char in text:
        if char in (CHAR_TERMINATOR, CHAR_NEW_LINE):
            break
        kind = char & CHAR_MASK
        if kind == CHAR_WIDTH_MASK:
            width = char & 0xFF
        elif kind == CHAR_INDENT_MASK:
            width = 0
            indent = char & 0xFF
        elif kind == CHAR_COLOR_MASK:
            width = 0
            color = char & 0xFF
        else:
            width = font.width(char)
            draw_character(vram, font, char, 0, indent, color)
        indent += width


def read_text(rom: bytes, pointer: int, rom_offset) -> list[int]:
    """A u16 message text up to and including its terminator."""
    offset = rom_offset(pointer, rom)
    text = []
    while True:
        value = struct.unpack_from("<H", rom, offset + 2 * len(text))[0]
        text.append(value)
        if value & CHAR_MASK == CHAR_TERMINATOR:
            return text
        if len(text) > 4096:
            raise ValueError("unterminated message text")


def render_message(vram: Vram, font: Font, text: list[int], base: int, limit: int = 256) -> int:
    """Run TextProcessMessageBanner until it reports its line; returns the
    number of calls (one a frame)."""
    message = Message(text)
    for calls in range(1, limit + 1):
        if process_banner(message, vram, font, base):
            return calls
    raise ValueError("message banner text never finished")
