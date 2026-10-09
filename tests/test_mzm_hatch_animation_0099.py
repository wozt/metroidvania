# SPDX-License-Identifier: GPL-3.0-only
"""MZM native BG1 hatch state/frame mapping without proprietary ROM data."""
import struct
import unittest
import zlib

from scripts.mzm_hatch_preview import hatch_metatile_index, render_hatch


class HatchAnimation0099(unittest.TestCase):
    def test_exact_native_indices(self):
        self.assertEqual(hatch_metatile_index('normal', False), 0x92)
        self.assertEqual(hatch_metatile_index('normal', True), 0x93)
        self.assertEqual(hatch_metatile_index('normal', False, 'opening', 1), 0x11)
        self.assertEqual(hatch_metatile_index('normal', False, 'opening', 4), 0x14)
        self.assertEqual(hatch_metatile_index('normal', True, 'opening', 1), 0x16)
        self.assertEqual(hatch_metatile_index('normal', True, 'opening', 4), 0x19)
        self.assertEqual(hatch_metatile_index('normal', False, 'closing', 1), 0x53)
        self.assertEqual(hatch_metatile_index('normal', False, 'closing', 3), 0x51)
        self.assertEqual(hatch_metatile_index('normal', True, 'closing', 1), 0x58)
        self.assertEqual(hatch_metatile_index('no_hatch', False, 'opening', 1), 0x91)
        self.assertEqual(hatch_metatile_index('no_hatch', False, 'closing', 1), 0x93)
        with self.assertRaises(ValueError):
            hatch_metatile_index('normal', True, 'closing', 4)
        with self.assertRaises(ValueError):
            hatch_metatile_index('blue', True, 'opening', 1)

    def test_real_frame_selects_distinct_source_metatile(self):
        table = bytearray(0x680)
        graphics = bytearray(0x1000)
        palette = bytearray(96)
        # 0x11 opening frame => native tile #64, red.
        # 0x92 closed frame => native tile #65, blue.
        # 0x53 closing frame => native tile #66, green.
        for idx, tile in ((0x11, 64), (0x92, 65), (0x53, 66)):
            for row in range(4):
                struct.pack_into('<4H', table, (idx + 16 * row) * 8,
                                 tile, tile, tile, tile)
        graphics[0:32] = bytes([0x11]) * 32
        graphics[32:64] = bytes([0x22]) * 32
        graphics[64:96] = bytes([0x33]) * 32
        for color_index, color in ((1, 0x001f), (2, 0x7c00), (3, 0x03e0)):
            struct.pack_into('<H', palette, 2 * color_index, color)

        def first_rgba(png):
            offset = 8
            payload = b''
            while offset < len(png):
                size = struct.unpack_from('>I', png, offset)[0]
                kind = png[offset + 4:offset + 8]
                if kind == b'IDAT':
                    payload += png[offset + 8:offset + 8 + size]
                offset += size + 12
            return zlib.decompress(payload)[1:5]

        a = render_hatch(bytes(table), bytes(graphics), bytes(palette),
                         'normal', False, 'opening', 1)
        b = render_hatch(bytes(table), bytes(graphics), bytes(palette),
                         'normal', False, 'closed', 0)
        c = render_hatch(bytes(table), bytes(graphics), bytes(palette),
                         'normal', False, 'closing', 1)
        self.assertEqual(first_rgba(a), bytes((255, 0, 0, 255)))
        self.assertEqual(first_rgba(b), bytes((0, 0, 255, 255)))
        self.assertEqual(first_rgba(c), bytes((0, 255, 0, 255)))


if __name__ == '__main__':
    unittest.main()
