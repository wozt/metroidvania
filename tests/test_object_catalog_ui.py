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
        for label in ('"Walls", "Enemies", "Items", "Objects", "Doors", "Events", "Triggers", "Other"',
                      'gtk_label_new("Room data")',
                      'load_annotations(doc, annotations_path)',
                      'Show project trigger regions; native trigger decoding is not available yet'):
            self.assertIn(label, source)
        self.assertNotIn(
            'gtk_widget_set_sensitive(overlay_buttons[OVERLAY_TRIGGERS], FALSE);',
            source)

    def test_project_event_regions_share_one_editor_in_both_worlds(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for contract in (
                '"event-list"',
                '"event-create"',
                '"event-update"',
                '"event-delete"',
                '"Create project event region here..."',
                '"Create project trigger region here..."',
                '"Apply event changes"',
                'project_event_move',
                'const guint step = doc->project_aria ? 8u : 16u;'):
            self.assertIn(contract, source)

    def test_room_records_have_context_menu_and_typed_editor_shell(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for contract in (
                'annotation_canvas_context_pressed',
                'annotation_list_context_pressed',
                '"Inspect full record"',
                '"Locate in Room data"',
                '"Open %s editor…"',
                '"Apply project override"'):
            self.assertIn(contract, source)
        self.assertIn('gtk_widget_set_sensitive(apply, FALSE);', source)

    def test_project_entity_editor_updates_both_world_grids_atomically(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for contract in (
                'command = "entity-update"',
                'project_command(doc, "update", options, NULL)',
                '"Apply entity changes"',
                '"Placement grid: 8px (Aria)"',
                '"Placement grid: 16px (Zero Mission)"',
                'gtk_spin_button_set_snap_to_ticks',
                'item->index, item->native_type, item->label'):
            self.assertIn(contract, source)


if __name__ == '__main__':
    unittest.main()
