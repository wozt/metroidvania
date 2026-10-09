# SPDX-License-Identifier: GPL-3.0-only
"""Source contract for the GTK4 two-level workbench and map pan surface."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class EditorLayerContract(unittest.TestCase):
    def test_editor_documents_have_separate_notebook(self):
        source = (ROOT / 'editor/main.c').read_text(encoding='utf-8')
        self.assertIn('build_editor_workbench(editor, application);', source)
        self.assertIn('editor->editing_dock, editor->palette_dock);', source)
        self.assertIn('"metroidvania-ephemeral-document-docks"', source)
        self.assertIn('build_palette_workbench(editor, application, right);', source)

    def test_native_focus_propagates_through_both_notebooks(self):
        source = (ROOT / 'editor/native_workspace.c').read_text(encoding='utf-8')
        # The GTK4 notebook can interpose internal containers between its
        # registered page and the actual notebook widget. Check the new
        # ancestry-based navigation rather than the removed parent/child loop.
        focus = source.split('static void focus_page(GtkWidget *page)', 1)[1].split(
            'static gboolean key_pressed(', 1)[0]
        self.assertIn('gtk_widget_is_ancestor(page, candidate)', focus)
        self.assertIn('gtk_notebook_get_nth_page(notebook, (gint)i)', focus)
        self.assertIn('gtk_notebook_set_current_page(notebook, (gint)i);', focus)

    def test_pan_uses_stationary_scroller_not_moving_grid(self):
        source = (ROOT / 'editor/world_atlas.c').read_text(encoding='utf-8')
        self.assertIn('gtk_widget_add_controller(scroller, GTK_EVENT_CONTROLLER(pan));', source)
        self.assertIn('gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller),pan_surface);', source)
        self.assertNotIn('gtk_widget_add_controller(grid, GTK_EVENT_CONTROLLER(pan));', source)
        self.assertNotIn('gtk_widget_add_controller(pan_surface, GTK_EVENT_CONTROLLER(pan));', source)
        self.assertIn('gtk_widget_add_tick_callback(w->scroller, map_pan_frame, w, NULL)', source)
        self.assertIn('gtk_overlay_set_child(GTK_OVERLAY(map_overlay), w->map_background);', source)
        self.assertIn('gtk_overlay_add_overlay(GTK_OVERLAY(map_overlay), grid);', source)
        self.assertIn('gtk_box_append(GTK_BOX(canvas_row),map_overlay);', source)
        self.assertIn('gtk_widget_set_can_target(w->connection_canvas, FALSE);', source)

    def test_global_map_uses_one_empty_background_and_wrapping_controls(self):
        source = (ROOT / 'editor/world_atlas.c').read_text(encoding='utf-8')
        grid = source.split('static void grid_rebuild(WorldGrid *w)\n{', 1)[1].split(
            '\nstatic void begin_generation(WorldGrid *w);', 1)[0]
        self.assertIn('map_background_draw_0117', source)
        self.assertIn('GtkWidget *grid_extent = gtk_box_new', grid)
        self.assertNotIn('GtkWidget *empty = gtk_box_new', grid)
        self.assertIn('GtkWidget *bar=gtk_flow_box_new();', source)
        self.assertNotIn('gtk_widget_set_size_request(right_space,720,-1);', source)
        self.assertNotIn('gtk_widget_set_size_request(bottom_space,-1,520);', source)


if __name__ == '__main__':
    unittest.main()
