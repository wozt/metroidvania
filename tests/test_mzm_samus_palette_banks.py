# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_body import load_palette_banks


class PaletteBanksTests(unittest.TestCase):
    def test_multiple_palette_rows_in_requested_banks(self):
        rom = bytes(range(96))
        palette = load_palette_banks(rom, 0, rows=3, first_bank=4)
        self.assertEqual(len(palette), 512)
        self.assertEqual(palette[4*32:7*32], rom)
        self.assertEqual(palette[:4*32], bytes(128))
        self.assertEqual(palette[7*32:], bytes(512 - 224))

    def test_defaults_compatible(self):
        self.assertEqual(load_palette_banks(bytes([7])*32, 0)[:32], bytes([7])*32)

    def test_reject_overflow(self):
        with self.assertRaises(ValueError):
            load_palette_banks(bytes(96), 0, rows=3, first_bank=14)

    def test_reject_truncated(self):
        with self.assertRaises(ValueError):
            load_palette_banks(bytes(64), 0, rows=3)

    def test_reject_misaligned(self):
        with self.assertRaises(ValueError):
            load_palette_banks(bytes(128), 1)
