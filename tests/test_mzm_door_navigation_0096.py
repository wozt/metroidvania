# SPDX-License-Identifier: GPL-3.0-only
"""Regression tests for native MZM door targets and GTK navigation (0096)."""
import unittest
from pathlib import Path
from unittest.mock import patch
from scripts import world_overview as overview


class DoorTransitions0096(unittest.TestCase):
    def test_verified_intra_area_targets_only(self):
        table = [
            {'index': 0, 'type': 'DOOR_TYPE_CLOSED_HATCH | DOOR_TYPE_NORMAL', 'sourceRoom': 2,
             'destinationDoor': 1, 'xStart': 2, 'yStart': 2},
            {'index': 1, 'type': 'DOOR_TYPE_NORMAL', 'sourceRoom': 4,
             'destinationDoor': 0, 'xStart': 2, 'yStart': 2},
            {'index': 2, 'type': 'DOOR_TYPE_AREA_CONNECTION', 'sourceRoom': 2,
             'destinationDoor': 1, 'xStart': 10, 'yStart': 2},
            {'index': 3, 'type': 'DOOR_TYPE_NONE', 'sourceRoom': 0,
             'destinationDoor': 0, 'xStart': 0, 'yStart': 0},
            {'index': 4, 'type': 'DOOR_TYPE_NORMAL', 'sourceRoom': 2,
             'destinationDoor': 3, 'xStart': 20, 'yStart': 2},
        ]
        tables = {name: [] for name in overview.MZM_AREAS}
        tables['Brinstar'] = table
        anchors = [(0, 2, 5, 4, 0, 0), (0, 4, 9, 4, 0, 0)]
        with patch.object(overview, 'ROOM_SOURCE') as room_source, \
             patch.object(overview, 'decode_room_descriptors', return_value=[]), \
             patch.object(overview, 'decode_door_tables', return_value=tables):
            room_source.read_text.return_value = ''
            data = overview.mzm_global_doors(anchors).decode('utf-8')
        self.assertIn('0|2|0|5|4|DOOR_TYPE_CLOSED_HATCH + DOOR_TYPE_NORMAL|4|1', data)
        self.assertIn('0|2|2|5|4|DOOR_TYPE_AREA_CONNECTION|-|-', data)
        self.assertIn('0|2|4|6|4|DOOR_TYPE_NORMAL|-|-', data)
        self.assertNotIn('|DOOR_TYPE_NONE|', data)
        self.assertNotIn('DOOR_TYPE_AREA_CONNECTION|4|1', data)
        self.assertTrue(all(len(row.split('|')) == 8 for row in data.splitlines()
                            if row and not row.startswith('#')))

    def test_gtk_panel_is_native_and_read_only(self):
        source = (Path(__file__).resolve().parents[1] /
                  'editor/world_atlas.c').read_text(encoding='utf-8')
        self.assertIn('gtk_expander_new("Native doors / select a Zero Mission room")', source)
        self.assertIn('selected_room_doors_refresh(w);', source)
        self.assertIn('native_workspace_import_async(w->workspace, mzm_areas[a - 1], r - 1);', source)
        self.assertIn('door->target_room != G_MAXUINT', source)
        self.assertIn('n == 6 || n == 8', source)
        self.assertIn('gtk_widget_set_visible(w->door_expander, w->world == 0)', source)


if __name__ == '__main__':
    unittest.main()
