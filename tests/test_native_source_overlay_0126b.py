# SPDX-License-Identifier: GPL-3.0-only
"""0126b: faithful source-only overlay sidecar, strict SDL identity and colors."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.native_source_overlay import encode_overlay

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'build/fusion_room_package_viewer'
FIXTURE = ROOT / 'tests/fixtures/project_room_0124.preview.tsv'

class NativeOverlay0126b(unittest.TestCase):
    def test_clipdata_and_native_annotation_keep_source_identity(self):
        data, counts = encode_overlay('Brinstar', 7, 128, 64,
            (8, 4, tuple([0, 1, 6, 33] + [0] * 28)), [
                ('ENEMY', 0, 16, 16, 16, 16, 'default', 'native', 'Actor', 'info'),
                ('DOOR', 1, 112, 0, 16, 32, 'default', 'native', 'Exit', 'info'),
                ('ITEM', 2, 999, 0, 16, 16, 'default', 'native', 'Offscreen', 'info'),
            ])
        self.assertEqual(counts, {'collision': 3, 'markers': 2,
                                  'skipped_markers': 1})
        self.assertIn(b'N\t16\t0\t16\t16\t1\n', data)
        self.assertIn(b'N\t32\t0\t16\t16\t6\n', data)
        self.assertIn(b'N\t48\t0\t16\t16\t33\n', data)
        self.assertIn(b'A\t16\t16\t16\t16\t1\n', data)
        self.assertIn(b'A\t112\t0\t16\t32\t4\n', data)
        self.assertNotIn(b'999', data)
        with self.assertRaisesRegex(ValueError, 'geometry differs'):
            encode_overlay('Brinstar', 7, 128, 64, (7, 4, tuple([1] * 28)), [])

    def test_sdl_source_identity_and_malformed_file_fail_closed(self):
        if not BINARY.is_file():
            self.skipTest('SDL viewer not built')
        blob, counts = encode_overlay('Brinstar', 7, 128, 64,
            (8, 4, tuple([1] + [0] * 31)),
            [('ENEMY', 0, 0, 0, 16, 16, 'default', 'native', 'Actor', 'info')])
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder) / 'source.tsv'
            file.write_bytes(blob)
            args = [str(BINARY), '--check', '--no-auto-bg', '--native-source', str(file), str(FIXTURE)]
            good = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(good.returncode, 0, good.stderr)
            self.assertIn('Native source overlay records: 2', good.stdout)
            for broken in (blob.replace(b'Brinstar', b'Kraid'),
                           blob.replace(b'N\t0\t0\t16\t16\t1',
                                        b'N\t0\t0\t16\t16\t-1'),
                           blob + b'extra\n'):
                file.write_bytes(broken)
                failure = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
                self.assertEqual(failure.returncode, 2)
                self.assertIn('Invalid native source overlay', failure.stderr)

    def test_source_remains_separate_and_colors_match_gtk(self):
        viewer = (ROOT / 'src/tools/room_package_viewer.c').read_text()
        gtk = (ROOT / 'editor/native_workspace.c').read_text()
        self.assertIn('PATCH_0126B_NATIVE_SOURCE', viewer)
        self.assertIn('mark->kind == \'N\'', viewer)
        self.assertIn('SDL_RenderFillRect(renderer, rect);', viewer)
        self.assertIn('mark->kind == \'N\' ? 148 : mark->kind == \'C\' ? 117 : 71', viewer)
        self.assertIn('0.2; green = 1.0; blue = 0.45', gtk)
        self.assertIn('red = 0.1; green = 0.56; blue = 1.0', gtk)
        self.assertIn('case 2: *r = 51; *g = 255; *b = 115;', viewer)
        self.assertIn('case 6: *r = 26; *g = 143; *b = 255;', viewer)
        exporter = (ROOT / 'scripts/project_room_package.py').read_text()
        self.assertNotIn('native_source_overlay', exporter)

if __name__ == '__main__':
    unittest.main()
