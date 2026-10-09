# SPDX-License-Identifier: GPL-3.0-only
"""0087: vertical workspace list and selected-page routing contract."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class VerticalWorkspace0087(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "editor/main.c").read_text(encoding="utf-8")

    def test_sidebar_replaces_horizontal_permanent_tabs(self):
        s = self.source
        self.assertIn("PATCH_0087_VERTICAL_WORKSPACE_NAV", s)
        self.assertIn("gtk_notebook_set_show_tabs(GTK_NOTEBOOK(center), FALSE);", s)
        self.assertIn("GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);", s)
        self.assertIn("GtkWidget *navigation = gtk_list_box_new();", s)
        self.assertIn("gtk_list_box_append(GTK_LIST_BOX(list), row);", s)
        self.assertIn("gtk_notebook_get_tab_label_text(center, page)", s)
        self.assertNotIn('g_signal_connect(left, "switch-page",', s)

    def test_all_real_pages_are_central_and_open_editors_last(self):
        s = self.source
        zero = s.index("room_browser_build(center, editor->native_workspace, ROOM_WORLD_ZERO)")
        aria = s.index("room_browser_build(center, editor->native_workspace, ROOM_WORLD_ARIA)")
        self.assertLess(zero, aria)
        self.assertLess(aria, s.index("world_atlas_build(center,"))
        self.assertLess(s.index("story_workspace_build(center,"),
                        s.index("build_editor_workbench(editor, application);", aria))
        self.assertIn('gtk_label_new("Open editors")', s)
        self.assertIn('g_object_set_data(G_OBJECT(row), "mv-workspace-page", page);', s)

    def test_selection_and_opened_rooms_route_to_center(self):
        s = self.source
        self.assertIn('G_CALLBACK(navigation_row_selected)', s)
        self.assertIn('navigation_select_for_page(editor, page);', s)
        self.assertIn('gtk_notebook_page_num(center, page)', s)
        self.assertIn('gtk_notebook_set_current_page(center, target);', s)
        self.assertIn('gtk_notebook_page_num(GTK_NOTEBOOK(editor->center_dock),', s)
        self.assertIn('editor->editing_page);', s)
        self.assertIn('gtk_notebook_set_current_page(GTK_NOTEBOOK(editor->center_dock), workbench);', s)

    def test_rom_visuals_ui_removed_without_touching_sprite_pipeline(self):
        s = self.source
        self.assertNotIn('build_assets_tab(', s)
        self.assertNotIn('"ROM visuals"', s)
        self.assertNotIn('asset_update_preview(', s)
        self.assertIn('build_palette_workbench(editor, application, right);', s)
        self.assertIn('build_inspector(right);', s)
        self.assertIn('native_workspace_build(editor->native_workspace,', s)

if __name__ == "__main__":
    unittest.main()
