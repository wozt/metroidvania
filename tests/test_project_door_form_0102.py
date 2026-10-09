# SPDX-License-Identifier: GPL-3.0-only
"""Patch 0102 GTK door editor must use the validated private backend."""
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / 'editor/native_workspace.c'


class ProjectDoorEditor0102(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding='utf-8')
        cls.block = cls.source.split(
            '/* PATCH_0102_SHARED_PROJECT_DOOR_PROPERTIES:', 1)[1].split(
            'static void annotation_window_open(', 1)[0]

    def test_edit_form_has_validated_backend_operations(self):
        for command in ('door-update', 'door-link', 'transition-update',
                        'transition-list', 'transition-delete'):
            with self.subTest(command=command):
                self.assertIn('"' + command + '"', self.block)
                self.assertIn('!strcmp(action, "' + command + '")',
                              self.source)

    def test_source_doors_not_edited(self):
        self.assertIn('!item->project_owned || item->kind != OVERLAY_DOORS',
                      self.block)
        self.assertIn('if (editor && item->project_owned && item->kind == OVERLAY_DOORS)',
                      self.source)
        self.assertIn('original rom', self.source.lower())

    def test_dialog_has_shared_destination_controls(self):
        for field in ('Target world', 'Target area', 'Target room ID',
                      'Target project door ID', 'Spawn X', 'Spawn Y',
                      'Save door properties', 'Save destination', 'Unlink'):
            self.assertIn(field, self.block)
        self.assertIn('project_door_fetch_link_0102(form)', self.block)


if __name__ == '__main__':
    unittest.main()
