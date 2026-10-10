# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free regression for native all-zero animation terminators."""
import unittest
from unittest.mock import patch
from scripts.mzm_samus_export_0150 import frame_metadata


class SamusTerminator0153Tests(unittest.TestCase):
    def test_zero_terminates_without_discarding_hold_frame(self):
        rom = bytearray(64)
        rom[:12] = bytes([1]) * 12
        rom[12] = 255
        rom[32:44] = bytes([2]) * 12
        with patch("scripts.mzm_samus_export_0150.stage", return_value=(b"", {
                "duration": 255, "frame_pointer": 0x08000000})) as decoder:
            frames = frame_metadata(bytes(rom), 0x08000000, 64)
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0]["duration"], 255)
        decoder.assert_called_once()

    def test_zero_first_frame_rejected(self):
        with self.assertRaisesRegex(ValueError, "begins with empty frame"):
            frame_metadata(bytes(16), 0x08000000, 16)
