# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for the Aria object sprite exporter."""
import struct
import unittest
from unittest import mock

from scripts import aos_object_sprites as objects


class ObjectSpriteTests(unittest.TestCase):
    def test_constructor_table_batch_uses_traced_native_descriptors(self):
        expected = {
            "skeleton": (0x02, 0x081F424C, 0x0820BE3C, 0x0824C910),
            "peeping_eye": (0x06, 0x081CBFEC, 0x0820A86C, 0x0821D7B4),
            "killer_fish": (0x07, 0x081CBF3C, 0x0820A464, 0x08212FB4),
            "bone_pillar": (0x08, 0x081E41D4, 0x0820B8E8, 0x08242F8C),
            "buer": (0x0A, 0x081F428C, 0x0820BF9C, 0x0824FDB0),
            "white_dragon": (0x0B, 0x081CBF74, 0x0820A580, 0x0821631C),
            "ghost": (0x0E, 0x081CBFFC, 0x0820A8D4, 0x0821EE0C),
            "rock_armor": (0x12, 0x081E41BC, 0x0820B83C, 0x0823FFD8),
            "giant_ghost": (0x13, 0x081CBFA4, 0x0820A698, 0x08219140),
            "winged_skeleton": (0x14, 0x081CBF64, 0x0820A518, 0x08215CBC),
        }
        for name, values in expected.items():
            row = objects.ENEMIES[name]
            self.assertEqual((row["id"], row["graphics"], row["palette"], row["frames"]),
                             values)

    def test_palette_scripts_list_bank_and_duration(self):
        script = struct.pack("<HH", 3, 3) + bytes((3, 7, 0, 0, 4, 7, 0, 0, 5, 7, 0, 0))
        with mock.patch.object(objects, "_rom_slice",
                               side_effect=lambda rom, p, n, label: script[p - 0x100:p - 0x100 + n]):
            self.assertEqual(objects.palette_script(b"", 0x100), [(3, 7), (4, 7), (5, 7)])

    def test_components_are_placed_relative_to_the_entity(self):
        # A 1x1-tile sheet whose pixel (0, 0) uses color 1.
        tiles = bytes([0x01]) + bytes(31)
        colors = [(0, 0, 0)] + [(10, 20, 30)] * 15
        pixels = objects.render_frame(tiles, 1, [(-8, -16, 0, 0, 8, 8, 0)], colors)
        self.assertEqual(pixels, {(-8, -16): (10, 20, 30)})

    def test_lower_components_are_drawn_on_top(self):
        tiles = bytes([0x01]) + bytes(31) + bytes([0x02]) + bytes(31)
        colors = [(0, 0, 0), (1, 1, 1), (2, 2, 2)] + [(0, 0, 0)] * 13
        pixels = objects.render_frame(tiles, 2, [(0, 0, 0, 0, 8, 8, 0),
                                                 (0, 0, 8, 0, 8, 8, 0)], colors)
        self.assertEqual(pixels[(0, 0)], (1, 1, 1))

    def test_palette_row_attribute_does_not_reject_component(self):
        tiles = bytes([0x01]) + bytes(31)
        colors = [(0, 0, 0)] + [(10, 20, 30)] * 15
        pixels = objects.render_frame(tiles, 1, [(0, 0, 0, 0, 8, 8, 0x01000000)], colors)
        self.assertEqual(pixels[(0, 0)], (10, 20, 30))


if __name__ == "__main__":
    unittest.main()
