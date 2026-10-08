# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic-only tests for Zero Mission candidate table inspection."""
import struct
import unittest

from scripts.mzm_samus_frame import ROM_BASE
from scripts.mzm_samus_inspect import inspect_table


class InspectTests(unittest.TestCase):
    def make_rom(self):
        rom = bytearray(0x2000)
        # A plausible 13-byte frame record using two nonempty tile bundles.
        for at in (0x400, 0x600):
            rom[at:at + 2] = bytes((1, 1))
            rom[at + 2:at + 66] = bytes([0x12] * 64)
        frame = ROM_BASE + 0x200
        struct.pack_into('<III', rom, 0x200, ROM_BASE + 0x400,
                         ROM_BASE + 0x600, ROM_BASE + 0x800)
        rom[0x20c] = 5
        rom[0x800:0x810] = bytes(range(16))
        struct.pack_into('<I', rom, 0x100, frame)
        return rom

    def test_valid_frame(self):
        rom = self.make_rom()
        rows = inspect_table(rom, ROM_BASE + 0x100, 3)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['duration'], 5)
        self.assertEqual(rows[0]['tile_counts'], [1, 1, 1, 1])
        self.assertEqual(rows[0]['oam_prefix'], bytes(range(16)).hex(' '))

    def test_bad_alignment(self):
        with self.assertRaisesRegex(ValueError, 'aligned'):
            inspect_table(self.make_rom(), ROM_BASE + 0x101)

    def test_missing_frames(self):
        with self.assertRaisesRegex(ValueError, 'no valid'):
            inspect_table(self.make_rom(), ROM_BASE + 0x110)

    def test_bad_limit(self):
        with self.assertRaisesRegex(ValueError, 'max_frames'):
            inspect_table(self.make_rom(), ROM_BASE + 0x100, 0)


if __name__ == '__main__':
    unittest.main()
