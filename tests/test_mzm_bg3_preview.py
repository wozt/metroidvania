# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_bg3_preview import preview, analyze_references


def litlz(data):
    out = bytearray([0x10, len(data)&255, (len(data)>>8)&255, 0])
    for i in range(0,len(data),8):
        out.append(0)
        out.extend(data[i:i+8])
    return bytes(out)

class BG3PreviewTests(unittest.TestCase):
    def test_text_map_and_gfx(self):
        gfx = bytes([0x11]*32)
        base = (0xfde0 - len(gfx) - 0xc000)//32
        word = (0x3000|base).to_bytes(2,'little')
        bg = bytes([0,0,0,0]) + litlz(word*1024)
        pal = bytearray(14*32)
        pal[16*2+2:16*2+4] = (31).to_bytes(2,'little')
        image,report=preview(bg,litlz(gfx),bytes(pal))
        self.assertEqual(image[:2],b'BM')
        self.assertEqual(report['visible_pixels'],256*256)
        self.assertEqual(report['unresolved_tiles'],0)
    def test_reference_diagnostics(self):
        data = (0x0001).to_bytes(2, 'little') * 512 + (0x300a).to_bytes(2, 'little') * 512
        report = analyze_references(data, 16, 0)
        self.assertEqual(report['total_cells'], 1024)
        self.assertEqual(report['referenced_graphics_cells'], 1024)
        self.assertEqual(report['common_palette_cells'], 512)
        self.assertEqual(report['tileset_palette_cells'], 512)
        self.assertEqual(report['palette_banks'], [0, 3])

    def test_missing_palette_not_fabricated(self):
        gfx = bytes([0x11] * 32)
        base = (0xfde0 - len(gfx) - 0xc000) // 32
        bg = bytes(4) + litlz(base.to_bytes(2, 'little') * 1024)
        with self.assertRaisesRegex(ValueError, 'palette-missing pixels=65536'):
            preview(bg, litlz(gfx), b'')

    def test_fail_closed(self):
        with self.assertRaises(ValueError): preview(b'bad',b'',b'')

if __name__ == '__main__': unittest.main()
