# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.mzm_samus_oam_layout import candidates


class CandidateTests(unittest.TestCase):
    def test_count_bounds_prevent_overread(self):
        rom = bytearray(16)
        struct.pack_into('<H', rom, 0, 4)
        self.assertEqual(candidates(rom, 0x08000000, 4), [])

    def test_six_byte_two_entry_candidate(self):
        rom = bytearray(64)
        struct.pack_into('<H', rom, 0, 2)
        struct.pack_into('<3H', rom, 2, 0, 0, 5)
        struct.pack_into('<3H', rom, 8, 0, 8, 6)
        matches = candidates(rom, 0x08000000, 2)
        item = next(c for c in matches if c['header_bytes'] == 2 and c['stride_bytes'] == 6)
        self.assertTrue(item['complete'])
        self.assertEqual([e['tile'] for e in item['entries']], [5, 6])

    def test_reject_invalid_count(self):
        with self.assertRaises(ValueError):
            candidates(bytes(32), 0x08000000)

    def test_reject_invalid_pointer(self):
        with self.assertRaises(ValueError):
            candidates(bytes(32), 0x07000000)


if __name__ == '__main__':
    unittest.main()
