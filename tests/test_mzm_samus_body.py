# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.mzm_samus_body import make_body


class SamusBodyTests(unittest.TestCase):
    @staticmethod
    def sample():
        rom = bytearray(512)
        struct.pack_into("<III", rom, 0, 0x08000020, 0x08000080, 0x080000c0)
        rom[12] = 3
        rom[0x20:0x22] = b"\x01\x00"
        rom[0x22:0x42] = b"\x11" * 32
        rom[0x80:0x82] = b"\x00\x00"
        struct.pack_into("<H3H", rom, 0xc0, 1, 0, 0, 0)
        struct.pack_into("<H", rom, 0x102, 0x7fff)
        return rom

    def test_bmp(self):
        bmp, metadata, oam = make_body(self.sample(), 0x08000000, 0x100,
                                       width=8, height=8, origin_x=0, origin_y=0)
        self.assertEqual(bmp[:2], b"BM")
        self.assertEqual(metadata["duration"], 3)
        self.assertEqual(oam["count"], 1)

    def test_invalid_palette(self):
        with self.assertRaises(ValueError):
            make_body(self.sample(), 0x08000000, 0x1f0)

    def test_invalid_frame(self):
        with self.assertRaises(ValueError):
            make_body(self.sample(), 0x07000000, 0x100)
