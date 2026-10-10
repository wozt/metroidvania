# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest

from scripts.mzm_samus_special_body_0173 import cannon_oam_header


class SpecialBody0173Tests(unittest.TestCase):
    def test_empty_cannon_header(self):
        rom = bytearray(64)
        struct.pack_into("<II", rom, 0, 0x08000018, 0x08000020)
        self.assertEqual(cannon_oam_header(rom, 0x08000000), 0)

    def test_nonempty_header_retained(self):
        rom = bytearray(64)
        struct.pack_into("<II", rom, 0, 0x08000018, 0x08000020)
        struct.pack_into("<H", rom, 32, 0x2003)
        self.assertEqual(cannon_oam_header(rom, 0x08000000), 0x2003)

    def test_bad_pointer_fails(self):
        rom = bytearray(64)
        struct.pack_into("<II", rom, 0, 0x08000018, 0)
        with self.assertRaises(ValueError):
            cannon_oam_header(rom, 0x08000000)
