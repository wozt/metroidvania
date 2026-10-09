# SPDX-License-Identifier: GPL-3.0-only
"""0127: BG3 stays a standalone diagnostic and never alters project export."""
from __future__ import annotations
import contextlib
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from scripts import native_room_viewer_demo as demo

ROOT = Path(__file__).resolve().parents[1]
VIEWER = ROOT / 'src/tools/room_package_viewer.c'
BINARY = ROOT / 'build/fusion_room_package_viewer'
FIXTURE = ROOT / 'tests/fixtures/project_room_0124.preview.tsv'


def bmp24(width: int, height: int) -> bytes:
    pitch = (width * 3 + 3) & ~3
    pixels = bytes(pitch * height)
    return (struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54) +
            struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0,
                        len(pixels), 0, 0, 0, 0) + pixels)


class StandaloneBg3Tests0127(unittest.TestCase):
    def test_source_mode_is_independent_and_never_blends_unverified_bg3(self):
        text = VIEWER.read_text(encoding='utf-8')
        for symbol in ('--bg3 path.bmp', 'SDLK_4',
                       'experimental BG3 (independent native tilemap)',
                       'active != 4 && i < preview.count',
                       'active == 4 ? surfaces[3]->w : preview.width',
                       'BG3 diagnostic dimensions invalid',
                       'SDL_DestroyTexture(textures[3]);'):
            self.assertIn(symbol, text)
        self.assertIn('"native_assets_included": False',
                      (ROOT / 'scripts/project_room_package.py').read_text())

    def test_independent_bmp_geometry_and_rejection_without_video(self):
        if not BINARY.is_file():
            self.skipTest('SDL3 room viewer not compiled yet')
        with tempfile.TemporaryDirectory() as tmp:
            valid = Path(tmp) / 'bg3.bmp'
            tall = Path(tmp) / 'tall.bmp'
            wrong = Path(tmp) / 'bad.bmp'
            valid.write_bytes(bmp24(256, 256))
            tall.write_bytes(bmp24(256, 512))
            wrong.write_bytes(bmp24(144, 64))
            base = [str(BINARY), '--check', '--no-auto-bg', '--bg3']
            for item in (valid, tall):
                run = subprocess.run([*base, str(item), str(FIXTURE)],
                    cwd=ROOT, text=True, capture_output=True, check=False)
                self.assertEqual(run.returncode, 0, run.stderr)
                self.assertIn('BG3=native-tilemap', run.stdout)
            run = subprocess.run([*base, str(wrong), str(FIXTURE)],
                cwd=ROOT, text=True, capture_output=True, check=False)
            self.assertEqual(run.returncode, 2)
            self.assertIn('BG3 diagnostic dimensions invalid', run.stderr)
            run = subprocess.run([str(BINARY), '--check', '--no-auto-bg',
                str(FIXTURE)], cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn('BG3=absent', run.stdout)

    def test_demo_export_remains_empty_and_reports_optional_bg3(self):
        with tempfile.TemporaryDirectory() as tmp:
            sandbox = Path(tmp)
            fixture = {'layers': {'Bg1': {'status': 'DECODED_METATILES',
                                         'width_blocks': 8, 'height_blocks': 6,
                                         'path': 'rooms/metroid/previews/brinstar_033_bg1.bmp'}}}
            info = {'status': 'EXPERIMENTAL_BG3_TEXT_MAP',
                    'path': 'rooms/metroid/previews/brinstar_033_bg3.bmp'}
            stdout = io.StringIO()
            with (patch.object(demo, 'ROOT', sandbox),
                  patch.object(demo.native, 'decode_room', return_value=fixture),
                  patch.object(demo.bg3, 'render_room', return_value=info),
                  patch.object(demo.native_source_overlay, 'write_overlay',
                               side_effect=ValueError('no native clipdata')),
                  patch.object(sys, 'argv', ['native_room_viewer_demo', '--decode-bg3']),
                  contextlib.redirect_stdout(stdout)):
                self.assertEqual(demo.main(), 0)
            packages = list((sandbox / 'assets/extracted/native_demo_0125/assets/extracted/exports/mzm')
                            .glob('brinstar_033_*/preview.tsv'))
            self.assertEqual(len(packages), 1)
            preview = packages[0].read_text(encoding='ascii')
            self.assertIn('MVROOM-PREVIEW\t1\tmzm\tBrinstar\t33\t128\t96', preview)
            self.assertNotIn('\nN\t', preview)
            self.assertNotIn('\nA\t', preview)
            self.assertNotIn('\nC\t', preview)
            self.assertIn('press 4 in SDL3', stdout.getvalue())


if __name__ == '__main__':
    unittest.main()
