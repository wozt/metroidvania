# SPDX-License-Identifier: GPL-3.0-only
"""0113: collision authoring and fixed icon strips (source + backend contract)."""
import unittest
from pathlib import Path
from scripts import project_room_entities as entities

ROOT = Path(__file__).resolve().parents[1]


class CollisionToolbar0113(unittest.TestCase):
    def test_every_collision_brush_is_available_in_both_worlds(self):
        for world, area, resolution in [('mzm', 'Brinstar', 16), ('aria', '3', 8)]:
            with self.subTest(world=world):
                doc = entities._new(world, area, 5, 256, 256)
                self.assertEqual(doc['collision']['resolution_px'], resolution)
                for index, collision_type in enumerate(entities.COLLISION_TYPES):
                    self.assertEqual(
                        entities.collision_fill(doc, index, 1, 1, 1, collision_type), 1)
                    self.assertEqual(entities.collision_get(doc, index, 1), collision_type)
                # Removing the project overlay differs from explicit air.
                air_x = entities.COLLISION_TYPES.index('air')
                self.assertEqual(entities.collision_clear(doc, air_x, 1, 1, 1), 1)
                self.assertEqual(entities.collision_get(doc, air_x, 1), 'empty')
                entities.validate(doc, world, area, 5, 256, 256)

    def test_native_reference_matrix_covers_both_worlds(self):
        for world, resolution in [('mzm', 16), ('aria', 8)]:
            with self.subTest(world=world):
                capabilities = entities.collision_capabilities(world)
                self.assertEqual(capabilities['resolution_px'], resolution)
                self.assertEqual(capabilities['native_encoder'], 'unavailable')
                self.assertEqual(
                    [entry['type'] for entry in capabilities['types']],
                    list(entities.COLLISION_TYPES))
                self.assertTrue(all(
                    entry['native_reference_status'].startswith('verified_')
                    for entry in capabilities['types']))

    def test_freehand_stroke_replaces_existing_cells_atomically(self):
        doc = entities._new('mzm', 'Brinstar', 5, 256, 256)
        entities.collision_fill(doc, 1, 1, 3, 2, 'solid')
        points = [(1, 1), (2, 1), (2, 2), (2, 2)]
        self.assertEqual(entities.collision_stroke(doc, points, 'water'), 3)
        self.assertEqual(entities.collision_get(doc, 1, 1), 'water')
        self.assertEqual(entities.collision_get(doc, 2, 2), 'water')
        self.assertEqual(entities.collision_get(doc, 3, 2), 'solid')
        self.assertEqual(entities.collision_stroke(doc, points, 'air'), 3)
        self.assertEqual(entities.collision_get(doc, 1, 1), 'air')
        self.assertEqual(entities.collision_stroke(doc, [(3, 2)], 'one_way'), 1)
        self.assertEqual(entities.collision_get(doc, 3, 2), 'one_way')

    def test_fixed_toolbar_and_one_backend_mutation_per_stroke(self):
        c = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        for snippet in (
            'TOOL_WALL, TOOL_PLATFORM, TOOL_HAZARD, TOOL_SLOPE_UP',
            'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2)',
            'gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tool_scroll), tools)',
            'gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(visibility_scroll)',
            'collision_icon_toggle_0113(',
            'collision_commit_0113(',
            'collision_stroke_line_0117(',
            'project_command(doc, "collision-stroke", args, NULL)',
            'doc->collision_dragging = FALSE;',
            'case GDK_KEY_u: doc->tool_id = TOOL_WATER;',
            'case GDK_KEY_p: doc->tool_id = TOOL_PLATFORM;',
            'case GDK_KEY_d: doc->tool_id = TOOL_HAZARD;',
            'case GDK_KEY_r: doc->tool_id = TOOL_SLOPE_UP;',
            'case GDK_KEY_t: doc->tool_id = TOOL_SLOPE_DOWN;',
            'doc->tool_id == TOOL_PLATFORM ? "one_way"',
            'doc->tool_id == TOOL_HAZARD ? "hazard"',
            'doc->tool_id == TOOL_SLOPE_UP ? "slope_up"',
            'doc->tool_id == TOOL_SLOPE_DOWN ? "slope_down"',
            '"mv-room-tool-name"',
            'if (!strcmp(name, "water")) return 6;',
            'if (!strcmp(name, "air")) return 7;',
            'if (cell->type == 7) continue;',
            'else cairo_set_source_rgba(cr, 1.0, 0.12, 0.12, 0.48);',
        ):
            self.assertIn(snippet, c)
        # No wrapping GtkFlowBox as main room toolbar.
        self.assertNotIn('GtkWidget *tools = gtk_flow_box_new();', c)
        self.assertNotIn('gtk_flow_box_insert(GTK_FLOW_BOX(tools)', c)
        self.assertNotIn('gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(tools)', c)


if __name__ == '__main__':
    unittest.main()
