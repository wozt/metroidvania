# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.mzm_samus_oam_probe import probe_oam


class OamProbeTests(unittest.TestCase):
    def test_six_byte_objects(self):
        rom = bytearray(80)
        struct.pack_into('<H', rom, 0, 2)
        struct.pack_into('<3H', rom, 2, 0, 0, 5)
        struct.pack_into('<3H', rom, 8, 0, 8, 6)
        info = probe_oam(rom, 0x08000000, 2)
        candidate = next(h for h in info['hypotheses']
                         if h['header_bytes'] == 2 and h['entry_bytes'] == 6)
        self.assertEqual(candidate['decoded'], 2)
        self.assertEqual(candidate['entries'][1]['tile'], 6)
        self.assertEqual(candidate['entries'][1]['x'], 8)

    def test_invalid_pointer(self):
        with self.assertRaises(ValueError):
            probe_oam(bytes(20), 0x07000000)

    def test_invalid_limit(self):
        with self.assertRaises(ValueError):
            probe_oam(bytes(20), 0x08000000, 0)


if __name__ == '__main__':
    unittest.main()
