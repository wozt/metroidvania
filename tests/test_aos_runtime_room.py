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
            "AOSROOM-NATIVE\t3\t3\t7\t1\t1\t2\t2", "R\t0045", "R\t03ff", "END"])

    def test_transitions_follow_the_rows_with_a_signed_x_adjustment(self):
        background = {"width_tiles": 1, "height_tiles": 1, "width_screens": 1,
                      "height_screens": 1, "collision": [0], "collision_xflip": [False]}
        transition = {"source_screen_x": -1, "source_screen_y": 0, "field_6": 0xFFF0,
                      "load_x": 1040, "load_y": 0, "target_engine_area": 0,
                      "target_room": 3}
        self.assertEqual(encode_room(0, 5, background, [transition]).splitlines()[-2:],
                         ["T\t-1\t0\t-16\t1040\t0\t0\t3", "END"])

    def test_entities_follow_the_transitions(self):
        background = {"width_tiles": 1, "height_tiles": 1, "width_screens": 1,
                      "height_screens": 1, "collision": [0], "collision_xflip": [False]}
        door = {"kind": 2, "entity_id": 0, "x": 8, "y": 160, "parameters": [0, 0],
                "flags": 0}
        self.assertEqual(encode_room(0, 3, background, [], [door]).splitlines()[-2:],
                         ["E\t2\t0\t8\t160\t0\t0\t0", "END"])


if __name__ == "__main__":
    unittest.main()
