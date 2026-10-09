# SPDX-License-Identifier: GPL-3.0-only
"""0125: local native BG1/BG2 are optional and never part of exports."""
from pathlib import Path
import struct
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
VIEWER = ROOT / "src/tools/room_package_viewer.c"
BINARY = ROOT / "build/fusion_room_package_viewer"
FIXTURE = ROOT / "tests/fixtures/project_room_0124.preview.tsv"


def bmp24(width: int, height: int) -> bytes:
    pitch = (width * 3 + 3) & ~3
    pixels = bytes([35, 70, 100]) * width
    data = b"".join(pixels + bytes(pitch - width * 3) for _ in range(height))
    return (struct.pack("<2sIHHI", b"BM", 54 + len(data), 0, 0, 54)
            + struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24,
                          0, len(data), 0, 0, 0, 0) + data)


class LocalBackground0125(unittest.TestCase):
    def test_viewer_keeps_native_assets_outside_package(self):
        source = VIEWER.read_text(encoding="utf-8")
        for term in ("PATCH_0125_LOCAL_NATIVE_BG", "local_mzm_background",
                     "rooms/metroid/previews/%s_%03d_bg%d.bmp",
                     "surface->w != preview->width || surface->h != preview->height",
                     "SDL_CreateTextureFromSurface", "SDL_SCALEMODE_NEAREST",
                     "SDL_RenderTexture(renderer, textures[active - 1], NULL, &image)",
                     "SDLK_c", "SDLK_m", "SDLK_1", "SDLK_2", "--no-auto-bg"):
            self.assertIn(term, source)
        self.assertNotIn('SDL_RenderTexture(renderer, textures[0], NULL, &image);', source)
        exporter = (ROOT / "scripts/project_room_package.py").read_text()
        self.assertIn('"native_assets_included": False', exporter)

    def test_explicit_local_bmp_dimension_contract_headless(self):
        if not BINARY.is_file():
            self.skipTest("build/fusion_room_package_viewer not built yet")
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as directory:
            matching = Path(directory) / "bg1.bmp"
            mismatch = Path(directory) / "wrong.bmp"
            matching.write_bytes(bmp24(128, 64))
            mismatch.write_bytes(bmp24(160, 64))
            base = [str(BINARY), "--check", "--no-auto-bg", "--bg1"]
            good = subprocess.run([*base, str(matching), str(FIXTURE)],
                                  cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(good.returncode, 0, good.stderr)
            self.assertIn("local BG1=matching", good.stdout)
            bad = subprocess.run([*base, str(mismatch), str(FIXTURE)],
                                 cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(bad.returncode, 2)
            self.assertIn("BMP dimensions do not match", bad.stderr)
            old = subprocess.run([str(BINARY), "--check", str(FIXTURE)],
                                 cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(old.returncode, 0, old.stderr)


if __name__ == "__main__":
    unittest.main()
