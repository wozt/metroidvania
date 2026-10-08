# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest

from scripts.mzm_samus_oam_decode import decode_raw_samus_oam


class SamusRawOamTests(unittest.TestCase):
    def test_two_six_byte_entries_and_flags(self):
        rom = bytearray(32)
        struct.pack_into('<H', rom, 0, 0x3002)
        struct.pack_into('<HHH', rom, 2, 0, 9, 7)
        struct.pack_into('<HHH', rom, 8, 0, 19, 8)
        result = decode_raw_samus_oam(rom, 0x08000000)
        self.assertEqual(result['count'], 2)
        self.assertEqual([e['tile'] for e in result['entries']], [7, 8])
        self.assertEqual(result['entries'][1]['x'], 19)
        self.assertTrue(result['arm_cannon_front'])
        self.assertTrue(result['arm_cannon_behind'])

    def test_reject_truncated(self):
        rom = struct.pack('<H', 2) + bytes(6)
        with self.assertRaisesRegex(ValueError, 'truncated'):
            decode_raw_samus_oam(rom, 0x08000000)

    def test_reject_bad_count_and_limit(self):
        with self.assertRaises(ValueError):
            decode_raw_samus_oam(bytes(16), 0x08000000)
        with self.assertRaises(ValueError):
            decode_raw_samus_oam(struct.pack('<H', 2) + bytes(12), 0x08000000, 1)

    def test_reject_bad_pointer(self):
        with self.assertRaises(ValueError):
            decode_raw_samus_oam(bytes(32), 0x07000000)


if __name__ == '__main__':
    unittest.main()
