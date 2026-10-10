# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for the Aria object sprite exporter."""
import struct
import unittest
from unittest import mock

from scripts import aos_object_sprites as objects


class ObjectSpriteTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
