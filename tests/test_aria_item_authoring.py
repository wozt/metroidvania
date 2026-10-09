# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free regression for 0081 native Aria pickup family/typed settings."""
import tempfile
import unittest
from pathlib import Path
from scripts import project_room_entities as pe
from scripts.native_sprite_thumbnails import _png_rgba, _decode_icon, _page


class AriaItemAuthoringTests(unittest.TestCase):
    def test_subtypes_and_family_metadata(self):
        records = pe.catalog_options('aria', 'ITEM', records=[])
        choices = {r['native_type'] for r in records}
        for family in ('pickup', 'hard-mode-pickup', 'all-souls-reward'):
            for code in (1, 2, 3, 4, 5, 6, 7, 8):
                self.assertIn(f'{family}:{code:02X}', choices)
        self.assertNotIn('pickup:03', {r['native_type'] for r in
                         pe.catalog_options('aria', 'ENEMY', records=[])})

    def test_settings_roundtrip_and_old_documents(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            d = pe.load(root, 'aria', '0', 2, 320, 160)
            e = pe.create(d, 'ITEM', 16, 32, 'Gold')
            pe.save(root, d)
            self.assertNotIn('settings', pe.load(root, 'aria', '0', 2, 320, 160)['entities'][0])
            settings = pe.item_settings('pickup:03', 58, 513, 999, 255)
            pe.assign(d, e['id'], 'pickup:03', records=[{
                'native_type': 'pickup:03', 'name': 'Weapon pickup',
                'category': 'Item / soul pickup'}], settings=settings)
            pe.save(root, d)
            restored = pe.load(root, 'aria', '0', 2, 320, 160)
            self.assertEqual(restored['entities'][0]['settings'], settings)
            self.assertEqual(restored['entities'][0]['native_type'], 'pickup:03')

    def test_fail_closed_and_no_partial_edit(self):
        for args in [('pickup:03', 59, 0, 0, 0), ('pickup:08', 6, 0, 0, 0),
                     ('enemy:02', 1, 0, 0, 0), ('pickup:04', 0, 65536, 0, 0)]:
            with self.subTest(args=args), self.assertRaises(ValueError):
                pe.item_settings(*args)
        d = pe._new('mzm', 'Brinstar', 1, 320, 160)
        e = pe.create(d, 'ITEM', 0, 0, 'Test')
        e['settings'] = {'item_id': 0, 'parameter_0': 0,
                         'parameter_1': 0, 'flags': 0}
        with self.assertRaises(ValueError):
            pe.validate(d, 'mzm', 'Brinstar', 1, 320, 160)

    def test_png_generator_validity(self):
        import struct
        # Three synthetic explicit-size native pages plus a ROM palette.
        from scripts.native_sprite_thumbnails import GFX_BANKS, PALETTE, GBA_ROM_BASE
        rom = bytearray(max(*GFX_BANKS, PALETTE) - GBA_ROM_BASE + 0x4000)
        for address in GFX_BANKS:
            p = address - GBA_ROM_BASE
            rom[p:p+4] = (0x2000).to_bytes(4, 'little')
            rom[p+4:p+4+0x2000] = b'\x11' * 0x2000
        p = PALETTE - GBA_ROM_BASE
        rom[p+2:p+4] = (0x001f).to_bytes(2, 'little')
        pages = [_page(rom, addr) for addr in GFX_BANKS]
        img = _decode_icon(rom, pages, 0)
        self.assertTrue(img.startswith(b'\x89PNG\r\n\x1a\n'))
        self.assertGreater(len(img), 60)
        self.assertTrue(_png_rgba(1, 1, b'\x00'*4).startswith(b'\x89PNG'))


if __name__ == '__main__':
    unittest.main()
