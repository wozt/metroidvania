# SPDX-License-Identifier: GPL-3.0-only
"""0106: native project-door global-map picker integration contracts."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MapDoorTargetPicker0106(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.atlas = (ROOT / 'editor/world_atlas.c').read_text()
        cls.workspace = (ROOT / 'editor/native_workspace.c').read_text()
        cls.main = (ROOT / 'editor/main.c').read_text()

    def test_original_cells_only_and_no_implicit_room_open(self):
        s = self.atlas.split('static void grid_clicked(', 1)[1].split(
            'static gboolean has_image(', 1)[0]
        self.assertIn('c.provenance == 3 || c.room == 999', s)
        self.assertIn('c.provenance == 4', s)
        self.assertIn('world_atlas_pick_finish_0106(w, TRUE, c.area, c.room);', s)
        self.assertLess(s.index('world_atlas_pick_finish_0106(w, TRUE'),
                        s.index('if (presses >= 2) selected_open(w);'))

    def test_one_shot_picker_is_cancelable(self):
        self.assertIn('w->pick_callback = NULL;', self.atlas)
        self.assertIn('w->pick_data = NULL;', self.atlas)
        self.assertIn('world_atlas_abandon_room_pick(GtkWidget *page, gpointer userdata)', self.atlas)
        self.assertIn('world_atlas_pick_cancel_clicked_0106', self.atlas)
        self.assertIn('"Cancel picking"', self.atlas)
        self.assertIn('if (w->pick_callback)\n        world_atlas_pick_finish_0106', self.atlas)
        self.assertIn('w->pick_data != userdata', self.atlas)

    def test_form_does_not_save_on_pick(self):
        start = self.workspace.index('static void project_door_map_picked_0106(')
        end = self.workspace.index('static void project_door_editor_open_0102(', start)
        body = self.workspace[start:end]
        self.assertNotIn('project_command(', body)
        self.assertNotIn('project_reload(', body)
        self.assertIn('gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->target_door), 0)', body)
        self.assertIn('gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->spawn_x), 0)', body)
        self.assertIn('"Pick room on global map"', self.workspace)
        self.assertIn('world_atlas_abandon_room_pick(form->atlas_page, form);', self.workspace)

    def test_navigation_and_weak_lifetimes(self):
        self.assertIn('native_workspace_set_world_atlas(editor->native_workspace, editor->world_map_page)',
                      self.main)
        self.assertIn('gtk_notebook_set_current_page(GTK_NOTEBOOK(ancestor)', self.atlas)
        self.assertIn('g_object_add_weak_pointer(G_OBJECT(form->atlas_page)', self.workspace)
        self.assertIn('g_object_remove_weak_pointer(G_OBJECT(form->return_page)', self.workspace)


if __name__ == '__main__':
    unittest.main()
