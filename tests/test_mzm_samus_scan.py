# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.mzm_samus_scan import scan_tables, valid_frame

BASE = 0x08000000


def fixture():
    rom = bytearray(0x2000)
    # A ROM-local 4bpp graphics bundle, 1+1 tiles, repeated in synthetic frames.
    rom[0x500:0x502] = bytes((1, 1))
    rom[0x502:0x542] = bytes([0x11]) * 64
    for i in range(4):
        off = 0x800 + i * 16
        struct.pack_into('<III', rom, off, BASE + 0x500, BASE + 0x500, BASE + 0x600)
        rom[off + 12] = i + 2
        struct.pack_into('<I', rom, 0x100 + i * 4, BASE + off)
    return rom


class ScannerTest(unittest.TestCase):
    def test_find_four_frames(self):
        tables = scan_tables(fixture(), min_frames=3)
        self.assertEqual(len(tables), 1)
        self.assertEqual(tables[0]['table_offset'], 0x100)
        self.assertEqual(tables[0]['frames'], 4)

    def test_reject_invalid_frame(self):
        rom = fixture()
        self.assertFalse(valid_frame(rom, BASE + 0x900))
        rom[0x80C] = 0
        self.assertFalse(valid_frame(rom, BASE + 0x800))

    def test_reject_invalid_limits(self):
        with self.assertRaises(ValueError):
            scan_tables(fixture(), 5, 2)


if __name__ == '__main__':
    unittest.main()
