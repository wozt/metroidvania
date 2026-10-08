# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
import zlib

from scripts.aos_native_workspace import (
    MAX_TILES, TRANSPARENT, pack_atlas, png_rgba, serialize, split_tiles,
)


class NativeAriaWorkspaceTests(unittest.TestCase):
    def test_rgba_atlas_and_tile_ids(self):
        red = bytes((255, 0, 0, 255)) * 256
        tiles = [TRANSPARENT]
        keys = {TRANSPARENT: 0}
        w, h, entries = split_tiles(red + red, 16, 32, keys, tiles)
        self.assertEqual((w, h, entries), (1, 2, [1, 1]))
        self.assertEqual(len(tiles), 2)
        png = pack_atlas(tiles)
        self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
        # Verify the deflate payload really contains transparent+opaque pixels.
        start = 8 + 12 + 13
        self.assertEqual(png[start + 4:start + 8], b'IDAT')
        length = struct.unpack_from('>I', png, start)[0]
        raw = zlib.decompress(png[start + 8:start + 8 + length])
        self.assertEqual(raw[0], 0)
        self.assertEqual(raw[4], 0)

    def test_one_tile_png(self):
        payload = png_rgba(16, 16, TRANSPARENT)
        self.assertEqual(payload[-8:], b'IEND' + struct.pack('>I', zlib.crc32(b'IEND')))

    def test_editable_document(self):
        layers = {'BG1': (1, 1, [1]), 'BG2': (1, 1, [0])}
        blob = serialize(0, 10, layers, 2, 'rooms/aria/tilesets/area_00_room_010_atlas.png')
        self.assertIn(b'ROOM aria:00:010', blob)
        self.assertIn(b'LAYER BG2 1 1\n0000', blob)

    def test_reject_invalid(self):
        with self.assertRaises(ValueError):
            split_tiles(b'\0' * (16*16*4), 32, 16, {TRANSPARENT: 0}, [TRANSPARENT])
        with self.assertRaises(ValueError):
            serialize(12, 10, {'BG1': (1, 1, [0]), 'BG2': (1, 1, [0])}, 1,
                      'rooms/aria/tilesets/a_atlas.png')
        with self.assertRaises(ValueError):
            png_rgba(0, 16, b'')


if __name__ == '__main__':
    unittest.main()
