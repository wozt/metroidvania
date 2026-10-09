# SPDX-License-Identifier: GPL-3.0-only
"""0107 Grab: verified source placement persistence and per-world grid."""
import unittest
from pathlib import Path
from scripts import project_room_entities as entities

ROOT = Path(__file__).resolve().parents[1]


class GrabTool0107(unittest.TestCase):
    def test_aria_project_marker_on_half_tile(self):
        doc = entities._new('aria', '3', 7, 512, 512)
        marker = entities.create(doc, 'ITEM', 8, 24, 'Fine grid')
        entities.move(doc, marker['id'], 40, 56)
        self.assertEqual(doc['entities'][0]['x'], 40)
        self.assertEqual(doc['entities'][0]['y'], 56)
        with self.assertRaises(ValueError):
            entities.move(doc, marker['id'], 41, 56)

    def test_mzm_project_marker_requires_full_tile(self):
        doc = entities._new('mzm', 'Brinstar', 10, 512, 512)
        marker = entities.create(doc, 'ENEMY', 16, 32, 'Full grid')
        with self.assertRaises(ValueError):
            entities.move(doc, marker['id'], 24, 32)
        self.assertEqual(doc['entities'][0]['x'], 16)

    def test_c_has_dedicated_grab_and_safe_source_override(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for part in ('TOOL_GRAB', 'case GDK_KEY_m:', 'grab_hit_0107(',
                     'native_positions_apply_0107(doc);',
                     'native_position_save_0107(doc, item, x, y)',
                     'G_KEY_FILE_NONE', '"door-update"',
                     'doc->project_aria ? 8 : 16', 'item->native_overridden'):
            self.assertIn(part, source)


if __name__ == '__main__':
    unittest.main()
