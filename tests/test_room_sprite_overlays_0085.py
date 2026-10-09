# SPDX-License-Identifier: GPL-3.0-only
"""Patch 0085: ROM-free sprite/legend and private preview protocol regression."""
import contextlib
import io
import tempfile
import unittest
from pathlib import Path
from scripts import project_room_entities as pe
from scripts import native_enemy_sprite_cache as sprites


class SpriteOverlay0085(unittest.TestCase):
    def test_project_preview_has_exact_aria_item_id(self):
        doc = pe._new('aria', '0', 4, 320, 160)
        # Fixture uses unassigned type because the real native catalog is ROM-based.
        pe.create(doc, 'ENEMY', 16, 32, 'Original enemy')
        out = io.StringIO()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pe.save(root, doc)
            args = ['--root', str(root), '--world', 'aria', '--area', '0',
                    '--room', '4', '--width', '320', '--height', '160', 'list-previews']
            with contextlib.redirect_stdout(out):
                self.assertEqual(pe.main(args), 0)
        self.assertEqual(out.getvalue().splitlines(),
                         ['1\tENEMY\t16\t32\tOriginal enemy\tunassigned\t-1'])

    def test_png_validation_and_safe_native_ids(self):
        from scripts.native_sprite_thumbnails import _png_rgba
        rgba = bytes([0, 0, 0, 0] * 16 * 16)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = root / 'sprite.png'
            candidate.write_bytes(_png_rgba(16, 16, rgba))
            dest = sprites.install('aria', 'enemy:07', candidate, root / 'cache')
            self.assertTrue(dest.is_file())
            self.assertEqual(dest, sprites.install('aria', 'enemy:07', candidate, root / 'cache'))
            for invalid in ('enemy:71', 'enemy:../../', 'enemy:GG', 'pickup:03'):
                with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                    sprites.destination('aria', invalid, root / 'cache')
            candidate.write_bytes(b'not a png')
            with self.assertRaises(ValueError):
                sprites.install('aria', 'enemy:07', candidate, root / 'cache')
            self.assertTrue(dest.is_file())

    def test_original_assets_are_not_misclassified(self):
        with self.assertRaises(ValueError):
            sprites.destination('mzm', 'PSPRITE_MISSILE_TANK')
        self.assertEqual(sprites.destination('mzm', 'PSPRITE_ZOOMER_RED', Path('test')),
                         Path('test/mzm/PSPRITE_ZOOMER_RED.png'))

    def test_source_contract_overlay_and_legend(self):
        src = Path('editor/native_workspace.c').read_text()
        self.assertIn('PATCH_0085_ROOM_SPRITE_OVERLAYS', src)
        self.assertIn('if (item->kind >= OVERLAY_COUNT || !doc->overlays[item->kind]) continue;', src)
        self.assertIn('room_draw_sprite(doc, cr, item, x, y, width, height);', src)
        self.assertIn('gtk_box_append(GTK_BOX(page), room_color_legend());', src)
        self.assertIn('"Enemies", "Items", "Objects", "Doors", "Events"', src)
        self.assertIn('cairo_stroke(cr);\n        /* Render authentic sprite', src)
        self.assertIn('g_hash_table_destroy(doc->entity_sprite_cache);', src)


if __name__ == '__main__':
    unittest.main()
