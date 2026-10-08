# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.gba_oam import unpack_oam, compose, to_bmp


class OamTests(unittest.TestCase):
    def test_dimensions_and_signed_coords(self):
        a = unpack_oam(struct.pack('<4H', 0x8000 | 255, 0xC000 | 511, 0x2002, 0))
        self.assertEqual((a['x'], a['y'], a['w'], a['h'], a['tile']), (-1, -1, 32, 64, 2))

    def test_flip_and_bank_and_transparency(self):
        tiles = bytearray(32)
        tiles[0] = 0x21  # x0=index1, x1=index2
        palette = bytearray(512)
        struct.pack_into('<H', palette, (3 * 16 + 1) * 2, 0x001f) # red
        struct.pack_into('<H', palette, (3 * 16 + 2) * 2, 0x03e0) # green
        entry = unpack_oam(struct.pack('<4H', 0, 0, 3 << 12, 0))
        pixels = compose(tiles, palette, [entry], 0, 0, 8, 8)
        self.assertEqual(list(pixels[:8]), [255, 0, 0, 255, 0, 255, 0, 255])
        self.assertEqual(list(pixels[8:12]), [0, 0, 0, 0])
        entry['hflip'] = True
        flipped = compose(tiles, palette, [entry], 0, 0, 8, 8)
        self.assertEqual(list(flipped[6*4:8*4]), [0, 255, 0, 255, 255, 0, 0, 255])

    def test_invalid_and_bmp(self):
        with self.assertRaises(ValueError):
            unpack_oam(struct.pack('<4H', 0x0100, 0, 0, 0))
        bmp = to_bmp(bytearray([255, 0, 0, 255]), 1, 1)
        self.assertEqual(bmp[:2], b'BM')
        self.assertEqual(bmp[122:126], bytes([0, 0, 255, 255]))


if __name__ == '__main__':
    unittest.main()
