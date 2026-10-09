# SPDX-License-Identifier: GPL-3.0-only
"""0113: collision authoring and fixed icon strips (source + backend contract)."""
import unittest
from pathlib import Path
from scripts import project_room_entities as entities

ROOT = Path(__file__).resolve().parents[1]


class CollisionToolbar0113(unittest.TestCase):
    def test_water_and_air_are_explicit_private_overrides(self):
        for world, area, resolution in [('mzm', 'Brinstar', 16), ('aria', '3', 8)]:
            with self.subTest(world=world):
                doc = entities._new(world, area, 5, 256, 256)
                self.assertEqual(doc['collision']['resolution_px'], resolution)
                self.assertEqual(entities.collision_fill(doc, 1, 1, 3, 2, 'solid'), 6)
                self.assertEqual(entities.collision_fill(doc, 1, 1, 2, 1, 'water'), 2)
                self.assertEqual(entities.collision_fill(doc, 2, 2, 1, 1, 'air'), 1)
                self.assertEqual(entities.collision_get(doc, 1, 1), 'water')
                self.assertEqual(entities.collision_get(doc, 2, 2), 'air')
                # Removing the project overlay differs from explicit air.
                self.assertEqual(entities.collision_clear(doc, 2, 2, 1, 1), 1)
                self.assertEqual(entities.collision_get(doc, 2, 2), 'empty')
                entities.validate(doc, world, area, 5, 256, 256)

    def test_fixed_toolbar_and_one_backend_mutation_per_rectangle(self):
        c = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for snippet in (
            'TOOL_WALL, TOOL_WATER, TOOL_AIR',
            'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2)',
            'gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tool_scroll), tools)',
            'gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(visibility_scroll)',
            'collision_icon_toggle_0113(',
            'collision_commit_0113(',
            'project_command(doc, "collision-fill", args, NULL)',
            'doc->collision_dragging = FALSE;',
            'case GDK_KEY_u: doc->tool_id = TOOL_WATER;',
            'if (!strcmp(name, "water")) return 6;',
            'if (!strcmp(name, "air")) return 7;',
        ):
            self.assertIn(snippet, c)
        # No wrapping GtkFlowBox as main room toolbar.
        self.assertNotIn('GtkWidget *tools = gtk_flow_box_new();', c)
        self.assertNotIn('gtk_flow_box_insert(GTK_FLOW_BOX(tools)', c)
        self.assertNotIn('gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(tools)', c)


if __name__ == '__main__':
    unittest.main()
