# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for the Aria runtime room export."""
import unittest

from scripts.aos_runtime_room import encode_room, native_cell


class AriaRuntimeRoomTests(unittest.TestCase):
    def test_only_slope_bytes_toggle_direction_on_x_flip(self):
        self.assertEqual(native_cell(0x41, True), 0x45)
        self.assertEqual(native_cell(0x45, True), 0x41)
        self.assertEqual(native_cell(0x41, False), 0x41)
        self.assertEqual(native_cell(0x04, True), 0x04)
        self.assertEqual(native_cell(0x03, True), 0x03)

    def test_rows_are_hex_cells_after_the_header(self):
        background = {"width_tiles": 2, "height_tiles": 2, "width_screens": 1,
                      "height_screens": 1, "collision": [0, 0x41, 0x03, 0xFF],
                      "collision_xflip": [False, True, False, False]}
        self.assertEqual(encode_room(3, 7, background).splitlines(), [
            "AOSROOM-NATIVE\t1\t3\t7\t1\t1\t2\t2", "R\t0045", "R\t03ff", "END"])


if __name__ == "__main__":
    unittest.main()
