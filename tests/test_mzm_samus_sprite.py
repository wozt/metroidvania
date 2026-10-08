# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest

from scripts.mzm_samus_sprite import (
    aligned_canvas_bounds,
    crop_rgba,
    ordered_entries,
    parse_arm_cannon_animation,
    stage_arm_cannon,
    make_power_suit_idle_right,
    make_power_suit_run_right,
)


class SamusSpriteTests(unittest.TestCase):
    def test_parse_arm_cannon_animation_and_signed_offset(self):
        rom = bytearray(64)
        struct.pack_into("<II", rom, 0, 0x08000020, 0x08000030)
        struct.pack_into("<HH", rom, 0x20, 0x00E5, 0x0102)
        result = parse_arm_cannon_animation(rom, 0x08000000)
        self.assertEqual(result["oam_pointer"], 0x08000030)
        self.assertEqual(result["muzzle_offset"], (-254, -26))

    def test_stage_arm_cannon_in_two_dimensional_rows(self):
        rom = bytes(range(128))
        vram = stage_arm_cannon(rom, bytes(0x8000), 0x08000000, 0x08000040)
        self.assertEqual(vram[0x800:0x840], bytes(range(64)))
        self.assertEqual(vram[0xC00:0xC40], bytes(range(64, 128)))

    def test_front_and_behind_order(self):
        body = {"entries": ["body"]}
        cannon = {"entries": ["cannon"], "arm_cannon_front": True,
                  "arm_cannon_behind": False}
        self.assertEqual(ordered_entries(body, cannon), ["cannon", "body"])
        cannon["arm_cannon_front"] = False
        cannon["arm_cannon_behind"] = True
        self.assertEqual(ordered_entries(body, cannon), ["body", "cannon"])

    def test_crop_rgba(self):
        rgba = bytearray(4 * 3 * 4)
        rgba[(1 * 4 + 2) * 4:(1 * 4 + 2) * 4 + 4] = b"\x01\x02\x03\xff"
        cropped, width, height, left, top = crop_rgba(rgba, 4, 3)
        self.assertEqual((width, height), (1, 1))
        self.assertEqual((left, top), (2, 1))
        self.assertEqual(cropped, b"\x01\x02\x03\xff")

    def test_reject_invalid_idle_frame(self):
        with self.assertRaisesRegex(ValueError, "0..3"):
            make_power_suit_idle_right(bytes(64), 4)

    def test_reject_invalid_run_frame(self):
        with self.assertRaisesRegex(ValueError, "0..9"):
            make_power_suit_run_right(bytes(64), 10)

    def test_animation_canvas_preserves_oam_axis(self):
        bounds = aligned_canvas_bounds([
            {"pixel_bounds": (-4, -30, 11, 0)},
            {"pixel_bounds": (-14, -28, 7, 1)},
        ])
        self.assertEqual(bounds, (-14, -30, 14, 1))


if __name__ == "__main__":
    unittest.main()
