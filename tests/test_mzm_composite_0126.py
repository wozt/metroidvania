# SPDX-License-Identifier: GPL-3.0-only
"""0126: native indexed-color transparency and bounded partial BG1/BG2 viewer."""
import struct
import subprocess
import tempfile
from pathlib import Path
import unittest

from scripts import mzm_room_render as renderer

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/fusion_room_package_viewer"
FIXTURE = ROOT / "tests/fixtures/project_room_0124.preview.tsv"


def bmp24(width, height):
    pitch = (width * 3 + 3) & ~3
    data = b"".join(b"\x00" * pitch for _ in range(height))
    return (struct.pack("<2sIHHI", b"BM", 54 + len(data), 0, 0, 54)
            + struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24,
                          0, len(data), 0, 0, 0, 0) + data)


class NativePartialComposite0126(unittest.TestCase):
    def test_gba_4bpp_palette_index_zero_is_transparent_only_in_composite(self):
        # First 8x8 cell is transparent (tile 0), second is opaque (tile 1).
        # Other cells repeat that pair, while the independent RGB remains valid.
        gfx = bytes(32) + bytes([0x11] * 32)
        palette = bytearray(512)
        struct.pack_into('<H', palette, (16 + 1) * 2, 0x001f)
        table = [(0x3000, 0x3001, 0x3000, 0x3001)]
        coverage = bytearray(16 * 16)
        rgb, _, _ = renderer.render_layer(1, 1, (0,), table, gfx, palette, 0,
                                          visibility=coverage)
        self.assertEqual((coverage[0], coverage[7], coverage[8], coverage[15]),
                         (0, 0, 1, 1))
        self.assertEqual((coverage[8 * 16], coverage[8 * 16 + 8]), (0, 1))
        # Previous callers get unchanged RGB instead of a breaking API change.
        previous, _, _ = renderer.render_layer(1, 1, (0,), table, gfx, palette, 0)
        self.assertEqual(previous, rgb)
        backdrop = bytearray([0, 255, 0] * (16 * 16))
        merged, counts = renderer.compose_partial_bg12(rgb, coverage, backdrop,
                                                        bytes([1] * (16 * 16)))
        self.assertEqual(counts, {"bg1_visible_pixels": 128,
                                  "bg2_visible_pixels": 128, "unresolved_pixels": 0})
        self.assertEqual(merged[:3], bytes([0, 255, 0]))
        self.assertNotEqual(merged[8 * 3:9 * 3], bytes([0, 255, 0]))

    def test_unknown_source_pixels_are_not_invented(self):
        out, counts = renderer.compose_partial_bg12(
            bytes([12, 13, 14] * 4), bytes([1, 0, 0, 0]),
            bytes([40, 41, 42] * 4), bytes([0, 1, 0, 0]))
        self.assertEqual(out[:6], bytes([12, 13, 14, 40, 41, 42]))
        self.assertEqual(out[6:], bytes(6))
        self.assertEqual(counts["unresolved_pixels"], 2)
        with self.assertRaises(ValueError):
            renderer.compose_partial_bg12(bytes(3), bytes(1), bytes(3), bytes(2))

    def test_viewer_opt_in_composite_and_local_images_not_packaged(self):
        source = (ROOT / "src/tools/room_package_viewer.c").read_text()
        for token in ('--composite', 'bg12_composite.bmp', 'SDLK_3',
                      'SDL_DestroyTexture(textures[2]);',
                      'partial BG1-over-BG2 (order not verified)'):
            self.assertIn(token, source)
        self.assertIn('"native_assets_included": False',
            (ROOT / 'scripts/project_room_package.py').read_text())
        if not BINARY.is_file():
            self.skipTest('SDL viewer has not been built yet')
        with tempfile.TemporaryDirectory() as tmp:
            good = Path(tmp) / 'partial.bmp'
            bad = Path(tmp) / 'mismatched.bmp'
            good.write_bytes(bmp24(128, 64))
            bad.write_bytes(bmp24(144, 64))
            args = [str(BINARY), '--check', '--no-auto-bg', '--composite']
            result = subprocess.run([*args, str(good), str(FIXTURE)],
                cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('BG12=matching', result.stdout)
            result = subprocess.run([*args, str(bad), str(FIXTURE)],
                cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('BMP dimensions do not match', result.stderr)


if __name__ == '__main__':
    unittest.main()
