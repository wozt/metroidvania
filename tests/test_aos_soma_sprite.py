# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest

from scripts.aos_soma_sprite import (
    decode_cell,
    extract_cell_tiles,
    load_palette,
    parse_animation,
)


BASE = 0x08000000


class SomaSpriteTests(unittest.TestCase):
    def make_rom(self):
        rom = bytearray(0xA000)

        animation_descriptor = 0x100
        animation_table = 0x200
        animation = 0x300
        struct.pack_into("<HHIII", rom, animation_descriptor,
                         1, 1, BASE + 0x80, 0, BASE + animation_table)
        struct.pack_into("<I", rom, animation_table, BASE + animation)
        struct.pack_into("<HH", rom, animation, 2, 1)
        struct.pack_into("<BBH", rom, animation + 4, 12, 30, 0)
        struct.pack_into("<BBH", rom, animation + 8, 13, 11, 0)

        graphics_descriptor = 0x400
        struct.pack_into("<BBBB", rom, graphics_descriptor, 2, 4, 1, 1)
        for index in range(4):
            pointer = 0x500 + index * 0x2100
            struct.pack_into("<I", rom, graphics_descriptor + 4 + index * 4,
                             BASE + pointer)
            struct.pack_into("<BBBB", rom, pointer, 0, 4, 16, 16)
        sheet = 0x500 + 3 * 0x2100 + 4
        for tile_y in range(8):
            start = sheet + (tile_y * 16 + 8) * 32
            rom[start:start + 8 * 32] = bytes([0x11]) * (8 * 32)

        palette_descriptor = 0x9000
        struct.pack_into("<BBBB", rom, palette_descriptor, 0, 4, 1, 0)
        struct.pack_into("<H", rom, palette_descriptor + 6, 0x001F)
        return bytes(rom)

    def test_parse_type_one_animation(self):
        animation = parse_animation(self.make_rom(), 0, BASE + 0x100)
        self.assertEqual(animation["animation_pointer"], BASE + 0x300)
        self.assertEqual(animation["frames"], [
            {"frame_id": 12, "duration": 30},
            {"frame_id": 13, "duration": 11},
        ])

    def test_extract_right_quadrant_from_sheet(self):
        tiles, metadata = extract_cell_tiles(self.make_rom(), 13, BASE + 0x400)
        self.assertEqual(len(tiles), 64 * 32)
        self.assertEqual(tiles, bytes([0x11]) * len(tiles))
        self.assertEqual(metadata["sheet_index"], 3)
        self.assertEqual(metadata["quadrant"], (1, 0))

    def test_decode_cell_uses_transparency_and_bgr555(self):
        rom = self.make_rom()
        palette = load_palette(rom, BASE + 0x9000)
        rgba, width, height = decode_cell(bytes([0x01]) * (64 * 32), palette)
        self.assertEqual((width, height), (64, 64))
        self.assertEqual(rgba[0:4], bytes((255, 0, 0, 255)))
        self.assertEqual(rgba[4:8], bytes((0, 0, 0, 0)))

    def test_reject_unknown_animation_encoding(self):
        rom = bytearray(self.make_rom())
        struct.pack_into("<H", rom, 0x302, 3)
        with self.assertRaisesRegex(ValueError, "unsupported"):
            parse_animation(bytes(rom), 0, BASE + 0x100)


if __name__ == "__main__":
    unittest.main()
