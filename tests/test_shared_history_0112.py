# SPDX-License-Identifier: GPL-3.0-only
"""0112: GTK chronological room history contracts (functional Xvfb test too)."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class SharedHistory0112(unittest.TestCase):
    def test_project_mutations_take_pre_command_snapshots(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        part = source.split('static gboolean project_command(', 1)[1].split(
            'static guint project_collision_type(', 1)[0]
        self.assertIn('room_history_capture_0112(doc, doc->map)', part)
        self.assertIn('if (success && mutation)', part)
        self.assertIn('room_history_push_0112(doc->undo', part)
        self.assertIn('history_clear(doc->redo', part)

    def test_snapshots_cover_tiles_json_and_native_positions(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for text in ('NativeMap *map;', 'GBytes *stage_json, *stage_ini',
                     'room_history_restore_0112(', 'room_history_equal_0112(',
                     'room_history_mark_saved_0112(doc);',
                     'history_step(doc, FALSE);', 'history_step(doc, TRUE);'):
            self.assertIn(text, source)
        self.assertNotIn('NativeMap **undo, **redo', source)

    def test_real_gtk_regression_is_registered(self):
        source = (ROOT / 'tests/test_native_workspace_gtk.c').read_text(encoding='utf-8')
        self.assertIn('/native-workspace/shared-tile-project-undo-redo', source)
        self.assertIn('native_workspace_test_shared_history(workspace, 0)', source)


if __name__ == '__main__':
    unittest.main()
