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

    def test_two_dimensional_mapping_uses_32_tile_row_stride(self):
        tiles = bytearray(34 * 32)
        tiles[2 * 32] = 0x01
        tiles[32 * 32] = 0x02
        palette = bytearray(512)
        struct.pack_into('<H', palette, 2, 0x001f)
        struct.pack_into('<H', palette, 4, 0x03e0)
        entry = unpack_oam(struct.pack('<4H', 0, 0x4000, 0, 0))
        one_d = compose(tiles, palette, [entry], 0, 0, 16, 16, "1d")
        two_d = compose(tiles, palette, [entry], 0, 0, 16, 16, "2d")
        second_row = (8 * 16) * 4
        self.assertEqual(list(one_d[second_row:second_row + 4]), [255, 0, 0, 255])
        self.assertEqual(list(two_d[second_row:second_row + 4]), [0, 255, 0, 255])

    def test_invalid_mapping(self):
        with self.assertRaisesRegex(ValueError, 'mapping'):
            compose(bytes(32), bytes(512), [], 0, 0, 1, 1, "invalid")


if __name__ == '__main__':
    unittest.main()
