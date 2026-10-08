# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.mzm_samus_frame import stage, rom_offset, parse_frame

BASE = 0x08000000

class TestSamusFrame(unittest.TestCase):
    def make_rom(self):
        rom = bytearray(512)
        struct.pack_into('<III', rom, 0, BASE+32, BASE+128, BASE+250)
        rom[12] = 5
        rom[32:34] = bytes((1, 1))
        rom[34:66] = bytes([0x11])*32
        rom[66:98] = bytes([0x22])*32
        rom[128:130] = bytes((1, 1))
        rom[130:162] = bytes([0x33])*32
        rom[162:194] = bytes([0x44])*32
        return bytes(rom)

    def test_four_banks(self):
        image, info = stage(self.make_rom(), BASE)
        self.assertEqual(len(image), 0x8000)
        for addr, value in ((0,0x11),(0x400,0x22),(0x280,0x33),(0x680,0x44)):
            self.assertEqual(image[addr:addr+32], bytes([value])*32)
        self.assertEqual(info['duration'], 5)
        self.assertEqual(info['oam_pointer'], BASE+250)

    def test_invalid_pointer(self):
        with self.assertRaises(ValueError):
            rom_offset(0x02000000, self.make_rom())
        with self.assertRaises(ValueError):
            parse_frame(self.make_rom(), BASE+508)

    def test_reject_slot_overflow(self):
        rom = bytearray(self.make_rom())
        rom[32] = 21 # > 0x280/32 = 20
        with self.assertRaises(ValueError):
            stage(bytes(rom), BASE)

if __name__ == '__main__':
    unittest.main()
