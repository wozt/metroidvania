# SPDX-License-Identifier: GPL-3.0-only
"""0108: Aria BG3 editor preview preserves native palette transparency."""
import struct
import unittest
import zlib
from pathlib import Path
from scripts.aos_native_workspace import png_background_rgba

ROOT = Path(__file__).resolve().parents[1]


class AriaBG3Preview0108(unittest.TestCase):
    def test_png_with_transparent_and_opaque_pixels(self):
        rgba = bytes((0, 0, 0, 0, 255, 0, 12, 255))
        data = png_background_rgba(2, 1, rgba)
        self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(struct.unpack_from('>II', data, 16), (2, 1))
        offset = 8
        compressed = []
        while offset < len(data):
            length = struct.unpack_from('>I', data, offset)[0]
            tag = data[offset + 4: offset + 8]
            if tag == b'IDAT':
                compressed.append(data[offset + 8:offset + 8 + length])
            offset += 12 + length
        self.assertEqual(zlib.decompress(b''.join(compressed)), b'\0' + rgba)
        for width, height, raw in ((0, 1, b''), (4097, 1, b''),
                                   (16, 16, bytes(16))):
            with self.assertRaises(ValueError):
                png_background_rgba(width, height, raw)

    def test_static_private_bg3_is_wired_into_room_editor(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        importer = (ROOT / 'scripts/aos_native_workspace.py').read_text(encoding='utf-8')
        self.assertIn('area_%02u_room_%03u_bg3.png', source)
        self.assertIn("bg3_name = f'rooms/aria/previews/{basename}_bg3.png'", importer)
        self.assertIn("bg3['status'] == 'DECODED_TEXT_BACKGROUND'", importer)
        self.assertIn('renderer.render_background(bg3, vram, palette)', importer)


if __name__ == '__main__':
    unittest.main()
