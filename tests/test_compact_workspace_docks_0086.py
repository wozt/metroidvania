# SPDX-License-Identifier: GPL-3.0-only
"""Source layout and compact legend regression, updated for 0087."""
# PATCH_0087_TEST_ADAPTED
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CompactWorkspaceDockTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main = (ROOT / "editor/main.c").read_text(encoding="utf-8")
        cls.native = (ROOT / "editor/native_workspace.c").read_text(encoding="utf-8")

    def test_explorer_shortcut_page_replaced_by_real_tabs(self):
        s = self.main
        self.assertIn('PATCH_0086_SOURCE_WORKSPACE_DOCK', s)
        self.assertNotIn('static void build_explorer(', s)
        self.assertNotIn('static void explorer_button(', s)
        self.assertNotIn('build_explorer(editor, left);', s)
        self.assertIn('make_responsive_button(editor, "Sources", 1)', s)
        self.assertIn('g_signal_connect(list, "row-selected", G_CALLBACK(navigation_row_selected)', s)
        self.assertIn('editor_show_world(editor, editor->native_page);', s)

    def test_two_room_browsers_adjacent_and_other_tools_in_sources(self):
        s = self.main
        zero = 'room_browser_build(center, editor->native_workspace, ROOM_WORLD_ZERO)'
        aria = 'room_browser_build(center, editor->native_workspace, ROOM_WORLD_ARIA)'
        self.assertIn(zero, s)
        self.assertIn(aria, s)
        self.assertLess(s.index(zero), s.index(aria))
        section = s[s.index(zero):s.index('build_editor_workbench(editor, application);', s.index(zero))]
        self.assertNotIn('world_atlas_build', section[:section.index(aria)])
        self.assertIn('world_atlas_build(center,', section)
        self.assertIn('object_catalog_build(center)', section)
        self.assertIn('story_workspace_build(center,', section)
        self.assertNotIn('room_browser_build(left,', s)

    def test_legacy_object_catalog_contract_tracks_left_dock(self):
        catalog_test = (ROOT / 'tests/test_object_catalog_ui.py').read_text(encoding='utf-8')
        self.assertIn("self.assertIn('editor->object_page = object_catalog_build(center);', main)",
                      catalog_test)
        self.assertNotIn("self.assertIn('editor->object_page = object_catalog_build(left);', main)",
                         catalog_test)

    def test_room_tabs_remain_in_center_and_tools_on_right(self):
        s = self.main
        self.assertIn('gtk_notebook_append_page(GTK_NOTEBOOK(editor->center_dock), page,', s)
        self.assertIn('gtk_label_new("Open editors")', s)
        self.assertIn('navigation_select_for_page(editor, page);', s)
        self.assertIn('native_workspace_build(editor->native_workspace,', s)
        self.assertIn('editor->editing_dock, editor->palette_dock);', s)
        self.assertIn('build_palette_workbench(editor, application, right)', s)
        self.assertIn('G_CALLBACK(editor_document_added)', s)
        self.assertIn('editor->small_focus = 0;', s)
        self.assertIn('gtk_paned_set_shrink_start_child(GTK_PANED(outer_split), TRUE)', s)

    def test_legend_compact_and_colored_bounds_preserved(self):
        s = self.native
        self.assertIn('PATCH_0086_COMPACT_ROOM_LEGEND', s)
        self.assertIn('gtk_widget_set_halign(legend, GTK_ALIGN_START);', s)
        self.assertIn('gtk_widget_set_hexpand(legend, FALSE);', s)
        self.assertIn('gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(legend), FALSE);', s)
        self.assertIn('gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(color), 11);', s)
        self.assertIn('gtk_box_append(GTK_BOX(page), room_color_legend());', s)
        self.assertIn('room_legend_draw,', s)
        # Original doors with a project override must not be painted twice.
        self.assertIn(
            'if (item->kind >= OVERLAY_COUNT || !doc->overlays[item->kind] ||\n'
            '            item->native_overridden) continue;', s)
        self.assertIn('room_draw_sprite(doc, cr, item, x, y, width, height);', s)

if __name__ == '__main__':
    unittest.main()
