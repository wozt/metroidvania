# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest

from scripts.aos_soma_sprite import (
    compose_rgba_layers,
    crop_rgba,
    decode_cell,
    extract_cell_tiles,
    extract_knife_frame,
    load_palette,
    make_soma_animation_frame,
    merge_attack_frames,
    opaque_bounds,
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

    def test_reject_unknown_animation_name(self):
        with self.assertRaisesRegex(ValueError, "unsupported Soma animation"):
            make_soma_animation_frame(b"", "unknown")

    def test_crop_preserves_requested_cell_coordinates(self):
        rgba = bytearray(4 * 4 * 4)
        rgba[(2 * 4 + 1) * 4:(2 * 4 + 1) * 4 + 4] = b"\x01\x02\x03\xff"
        bounds = opaque_bounds(rgba, 4, 4)
        cropped, width, height = crop_rgba(rgba, 4, 4, bounds)
        self.assertEqual(bounds, (1, 2, 2, 3))
        self.assertEqual((width, height), (1, 1))
        self.assertEqual(cropped, b"\x01\x02\x03\xff")

    def test_extract_single_component_knife_frame(self):
        rom = bytearray(0xB000)
        graphics = 0xA000
        records = 0xA900
        component = 0xAA00
        animation = 0xAB00
        struct.pack_into("<BBBB", rom, graphics, 0, 4, 16, 4)
        source = graphics + 4 + (1 * 16 + 2) * 32
        rom[source:source + 64] = bytes([0x11]) * 64
        struct.pack_into("<HHIII", rom, animation,
                         1, 1, BASE + records, 0, BASE + 0xAC00)
        rom[records + 5] = 1
        struct.pack_into("<I", rom, records + 12, BASE + component)
        struct.pack_into("<bbHBBBBI", rom, component,
                         -5, -6, 0, 16, 8, 16, 8, 0x301)
        tiles, metadata = extract_knife_frame(
            bytes(rom), 0, BASE + graphics, BASE + animation)
        self.assertEqual(tiles, bytes([0x11]) * 64)
        self.assertEqual(metadata["position"], (27, 41))
        self.assertEqual((metadata["width"], metadata["height"]), (16, 8))
        self.assertEqual(metadata["source_pointer"], BASE + source)

    def test_attack_timeline_merges_body_and_weapon_boundaries(self):
        body = [{"frame_id": frame_id, "duration": duration}
                for frame_id, duration in zip(
                    (20, 21, 136, 22, 137, 23, 24, 25, 26),
                    (5, 9, 2, 3, 3, 5, 7, 7, 7))]
        weapon = [{"frame_id": frame_id, "duration": duration}
                  for frame_id, duration in zip(
                      range(9, 15), (3, 2, 3, 6, 5, 8))]
        merged = merge_attack_frames(body, weapon, 6)
        self.assertEqual([frame["duration"] for frame in merged],
                         [3, 2, 3, 6, 2, 3, 3, 5, 7, 7, 7])
        self.assertEqual([frame["weapon_frame_id"] for frame in merged],
                         [9, 10, 11, 12, 13, 13, 14, 14,
                          None, None, None])

    def test_composite_draws_later_opaque_layer_on_top(self):
        clear_red = bytes((255, 0, 0, 0))
        opaque_blue = bytes((0, 0, 255, 255))
        rgba, width, height = compose_rgba_layers([
            (clear_red, 1, 1, 0, 0),
            (opaque_blue, 1, 1, 0, 0),
        ], (0, 0, 1, 1))
        self.assertEqual((width, height), (1, 1))
        self.assertEqual(rgba, opaque_blue)


if __name__ == "__main__":
    unittest.main()
