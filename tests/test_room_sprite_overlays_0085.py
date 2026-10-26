# SPDX-License-Identifier: GPL-3.0-only
"""Patch 0085: ROM-free sprite/legend and private preview protocol regression."""
import contextlib
import io
import tempfile
import unittest
from pathlib import Path
from scripts import project_room_entities as pe


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

    def test_source_contract_overlay_and_legend(self):
        src = Path('editor/native_workspace.c').read_text()
        self.assertIn('PATCH_0085_ROOM_SPRITE_OVERLAYS', src)
        # The 0104 native override intentionally hides the original overlay.
        self.assertIn(
            'if (item->kind >= OVERLAY_COUNT || !doc->overlays[item->kind] ||\n'
            '            item->native_overridden) continue;', src)
        self.assertIn('room_draw_sprite(doc, cr, item, x, y, width, height);', src)
        self.assertIn('gtk_box_append(GTK_BOX(page), room_color_legend());', src)
        self.assertIn('"Enemies", "Items", "Objects", "Doors", "Events"', src)
        self.assertIn('cairo_stroke(cr);\n        /* Render authentic sprite', src)
        self.assertIn('g_hash_table_destroy(doc->entity_sprite_cache);', src)


if __name__ == '__main__':
    unittest.main()
