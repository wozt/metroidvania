# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.gba_tiles import bgr555, bounded_slice, decode_tiles

class GBATileTests(unittest.TestCase):
    def test_palette(self):
        self.assertEqual(bgr555(0x001f), (255, 0, 0))
        self.assertEqual(bgr555(0x03e0), (0, 255, 0))
        self.assertEqual(bgr555(0x7c00), (0, 0, 255))

    def test_pixel_and_dimensions(self):
        palette = b'\x00\x00' + struct.pack('<H', 0x001f) + b'\x00' * 28
        bitmap = decode_tiles(bytes([0x11] * 32), palette, 1)
        self.assertEqual(bitmap[:2], b'BM')
        self.assertEqual(struct.unpack_from('<ii', bitmap, 18), (8, 8))
        self.assertEqual(bitmap[54:57], bytes((0, 0, 255)))

    def test_bounds(self):
        with self.assertRaises(ValueError):
            bounded_slice(b'1234', 2, 3, 'test')
        with self.assertRaises(ValueError):
            decode_tiles(b'broken', bytes(32), 1)

if __name__ == '__main__':
    unittest.main()
