# SPDX-License-Identifier: GPL-3.0-only
"""Regression: native hatch metatile rendering from extracted/private GBA data."""
import struct
import unittest
import zlib
from scripts.mzm_hatch_preview import HATCH_BASES, png_rgba, render_hatch
from scripts.room_annotations import mzm_hatch_from_clipdata


class HatchNativeGraphics0098(unittest.TestCase):
    def test_four_common_metatiles_with_original_palette(self):
        table = bytearray(0x680)
        graphics = bytearray(0x1000)
        palette = bytearray(96)
        # Palette row 0, color 1 = solid red in GBA BGR555.
        struct.pack_into('<H', palette, 2, 0x001F)
        graphics[:32] = bytes([0x11]) * 32  # BG tile #64, opaque color 1.
        for row in range(4):
            for side in (0, 1):
                offset = (HATCH_BASES['normal'] + side + row * 16) * 8
                struct.pack_into('<4H', table, offset, 64, 64, 64, 64)
        img = render_hatch(bytes(table), bytes(graphics), bytes(palette), 'normal', True)
        self.assertEqual(img[:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(struct.unpack('>II', img[16:24]), (16, 64))
        payload = img[41:-16]  # IDAT with 6-byte header? parse properly below
        offset = 8
        pixels = bytearray()
        while offset < len(img):
            length = struct.unpack_from('>I', img, offset)[0]
            kind = img[offset + 4:offset + 8]
            if kind == b'IDAT':
                pixels.extend(img[offset + 8:offset + 8 + length])
            offset += length + 12
        rows = zlib.decompress(pixels)
        self.assertEqual(rows[1:5], bytes((255, 0, 0, 255)))
        self.assertEqual(rows[-4:], bytes((255, 0, 0, 255)))

    def test_unknown_graphics_never_fabricated(self):
        table = bytearray(0x680)
        graphics = bytes(0x1000)
        palette = bytes(96)
        struct.pack_into('<4H', table, HATCH_BASES['normal'] * 8, 10, 10, 10, 10)
        with self.assertRaisesRegex(ValueError, 'non-common GBA tile'):
            render_hatch(bytes(table), graphics, palette, 'normal', False)
        with self.assertRaises(ValueError):
            png_rgba(16, 64, b'')

    def test_source_clipdata_identifies_exact_native_hatch_type(self):
        width, height = 40, 16
        clip = [0] * (width * height)
        clip[6 * width + 3] = 54  # CLIPDATA_REGULAR_DOOR, one block right.
        clip[5 * width + 30] = 64  # CLIPDATA_MISSILE_DOOR, one block left.
        self.assertEqual(
            mzm_hatch_from_clipdata((width, height, clip), 2, 6),
            ('normal', 'right', 3))
        self.assertEqual(
            mzm_hatch_from_clipdata((width, height, clip), 31, 5),
            ('missile', 'left', 30))
        self.assertIsNone(mzm_hatch_from_clipdata((width, height, clip), 12, 8))
        self.assertIsNone(mzm_hatch_from_clipdata((width, height, clip), 0, 20))


if __name__ == '__main__':
    unittest.main()
