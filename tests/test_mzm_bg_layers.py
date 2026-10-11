# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests of the Zero Mission background export helpers."""
from pathlib import Path
import struct
import unittest

from scripts import mzm_bg_layers

DECOMP = Path(__file__).resolve().parents[1] / "third_party/mzm"


class MzmBgLayerTests(unittest.TestCase):
    def test_block_values_index_the_sheet(self):
        # Tileset metatiles first, sCommonTilemap entries after them.
        self.assertEqual(mzm_bg_layers.sheet_block(5), 5)
        self.assertEqual(mzm_bg_layers.sheet_block(0x400), 1024)
        self.assertEqual(mzm_bg_layers.sheet_block(0x400 + 207), 1024 + 207)
        self.assertEqual(mzm_bg_layers.sheet_block(0x400 + 208), 0)

    def test_draw_tile_uses_flips_banks_and_transparent_zero(self):
        vram = bytearray(0x10000)
        vram[0x4000 + 32] = 0x21        # tile 1: pixel (0,0) = 1, (1,0) = 2
        colors = [None] * 256
        colors[3 * 16 + 1] = (10, 20, 30)
        colors[3 * 16 + 2] = (40, 50, 60)
        rgba = bytearray(8 * 8 * 4)
        mzm_bg_layers.draw_tile(rgba, 8, 0, 0, 0x3000 | 1, bytes(vram), colors, 0x4000)
        self.assertEqual(bytes(rgba[0:8]), bytes((10, 20, 30, 255, 40, 50, 60, 255)))
        self.assertEqual(rgba[8 + 3], 0)        # index 0: transparent
        flipped = bytearray(8 * 8 * 4)
        mzm_bg_layers.draw_tile(flipped, 8, 0, 0, 0x3400 | 1, bytes(vram), colors, 0x4000)
        self.assertEqual(bytes(flipped[7 * 4:8 * 4]), bytes((10, 20, 30, 255)))

    def test_bmp32_is_a_top_down_alpha_bitmap(self):
        data = mzm_bg_layers.bmp32(2, 1, bytes((1, 2, 3, 4, 5, 6, 7, 8)))
        self.assertEqual(data[:2], b"BM")
        width, height, planes, bits, compression = struct.unpack_from("<iiHHI", data, 18)
        self.assertEqual((width, height, bits, compression), (2, -1, 32, 3))
        self.assertEqual(data[-8:], bytes((3, 2, 1, 4, 7, 6, 5, 8)))

    @unittest.skipUnless((DECOMP / "include/constants/room.h").is_file(),
                         "pinned MZM decompilation missing")
    def test_bg_properties_follow_the_enum(self):
        props = mzm_bg_layers.bg_properties(DECOMP)
        self.assertEqual(props["BG_PROP_RLE_COMPRESSED"], 0x10)
        self.assertEqual(props["BG_PROP_LZ77_COMPRESSED"], 0x40)
        self.assertEqual(props["BG_PROP_DISABLE_TRANSPARENCY"], 0x12)
        self.assertEqual(props["BG_PROP_STARTS_FROM_BOTTOM"], 0x46)

    @unittest.skipUnless((DECOMP / "include/constants/room.h").is_file(),
                         "pinned MZM decompilation missing")
    def test_room_lines_hold_block_layers(self):
        fields = {"bg0Prop": "BG_PROP_NONE", "bg2Prop": "BG_PROP_RLE_COMPRESSED",
                  "bg3Prop": "BG_PROP_NONE", "transparency": "24", "bg3Scrolling": "3"}
        lines, images = mzm_bg_layers.room_background_lines(
            fields, {"Bg1": (2, 1, (3, 0x401)), "Bg2": (2, 1, (0, 1))}, 8)
        self.assertEqual(lines[0], "L\t8\t24\t0\t16\t0\t3\tEFFECT_NONE")
        self.assertIn("G\t1\t2\t1", lines)
        self.assertIn("R\t1\t0\t3,1025", lines)
        self.assertEqual(images, {})


if __name__ == "__main__":
    unittest.main()
