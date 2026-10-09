# SPDX-License-Identifier: GPL-3.0-only
"""Source contract for native room annotations and the shared object catalog."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ObjectCatalogUiContract(unittest.TestCase):
    def test_catalog_is_permanent_and_separates_both_worlds(self):
        main = (ROOT / 'editor/main.c').read_text(encoding='utf-8')
        catalog = (ROOT / 'editor/object_catalog.c').read_text(encoding='utf-8')
        self.assertIn('editor->object_page = object_catalog_build(center);', main)
        self.assertIn('Zero Mission objects', catalog)
        self.assertIn('Aria of Sorrow objects', catalog)
        self.assertIn('image-missing-symbolic', catalog)
        self.assertIn('gtk_widget_set_sensitive(create, FALSE);', catalog)
        self.assertIn('gtk_widget_set_sensitive(clone, FALSE);', catalog)

    def test_room_editor_exposes_truthful_native_data_overlays(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for label in ('"Walls", "Objects", "Doors", "Events", "Triggers"',
                      'gtk_label_new("Room data")',
                      'load_annotations(doc, annotations_path)',
                      'Show decoded trigger regions (not available yet)'):
            self.assertIn(label, source)
        self.assertIn(
            'gtk_widget_set_sensitive(overlay_buttons[OVERLAY_TRIGGERS], FALSE);',
            source)


if __name__ == '__main__':
    unittest.main()
