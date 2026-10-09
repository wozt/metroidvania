# SPDX-License-Identifier: GPL-3.0-only
"""0109: GTK defers project room writes, native moves, and metatile saves."""
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from scripts import project_room_entities as pe

ROOT = Path(__file__).resolve().parents[1]


class DeferredRoomSave0109(unittest.TestCase):
    def test_stage_does_not_write_live_project_document(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = pe._new('aria', '3', 7, 512, 512)
            pe.create(document, 'ENEMY', 8, 16, 'Saved')
            real = pe.save(root, document)
            old = real.read_bytes()
            with patch.dict(os.environ, {'MV_EDITOR_ROOM_STAGE': '00000000-0000-4000-8000-000000000109'}):
                staged = pe.load(root, 'aria', '3', 7, 512, 512)
                pe.move(staged, 1, 24, 32)
                staged_path = pe.save(root, staged)
                self.assertIn('.editor_staging', str(staged_path))
                self.assertEqual(pe.load(root, 'aria', '3', 7, 512, 512)['entities'][0]['x'], 24)
                self.assertEqual(real.read_bytes(), old)
            self.assertEqual(pe.load(root, 'aria', '3', 7, 512, 512)['entities'][0]['x'], 8)
            self.assertTrue(staged_path.exists())

    def test_invalid_stage_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.dict(os.environ, {'MV_EDITOR_ROOM_STAGE': '../outside'}):
                with self.assertRaisesRegex(ValueError, 'stage token'):
                    pe.load(Path(directory), 'mzm', 'Brinstar', 10, 512, 512)

    def test_overlay_stage_optional_for_metatile_only_save(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        block = source.split('static gboolean room_stage_commit_0109(NativeWorkspace *doc)', 1)[1]
        block = block.split('static gboolean save_override(NativeWorkspace *doc)', 1)[0]
        self.assertIn('PATCH_0110_OPTIONAL_ROOM_STAGING', block)
        self.assertIn('if (!has_json && !has_ini)', block)
        self.assertLess(block.index('if (!has_json && !has_ini)'),
                        block.index('g_strdup_printf("area_%02u_room_%03u.json"'))
        self.assertIn('if (!doc->project_area || !*doc->project_area)', block)
        self.assertIn('return FALSE;', block)
        # The real GTK Save -> Close regression is also exercised by CTest:
        # native-workspace/save-modified-then-close and unsaved-close-dialog.

    def test_gtk_uses_shared_diskette_and_grab_hand(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for part in ('hand-symbolic', 'room_stage_commit_0109(doc)',
                     'MV_EDITOR_ROOM_STAGE', 'room_stage_path_0109(doc, "ini")',
                     'if (success) mark_changed(doc);',
                     'doc->unsaved ? " *" : ""'):
            self.assertIn(part, source)
        self.assertNotIn('"input-mouse-symbolic"', source)


if __name__ == '__main__':
    unittest.main()
