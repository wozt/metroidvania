# SPDX-License-Identifier: GPL-3.0-only
"""GTK4 split-pane allocation contract for permanent and ephemeral docks."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ResponsiveDockContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'editor/main.c').read_text(encoding='utf-8')

    def test_each_pane_has_a_bounded_viewport(self):
        src = self.source
        self.assertIn('static GtkWidget *dock_viewport(GtkWidget *dock)', src)
        self.assertIn('gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), dock);', src)
        self.assertIn('gtk_widget_set_overflow(scroll, GTK_OVERFLOW_HIDDEN);', src)
        for part in ('left', 'center', 'right'):
            self.assertIn(f'editor->{part}_viewport = dock_viewport({part});', src)
            self.assertIn(f'gtk_widget_set_visible(editor->{part}_viewport, {part});', src)
        self.assertNotIn('gtk_paned_set_start_child(GTK_PANED(outer_split), left);', src)

    def test_both_splitters_can_allocate_narrow_panes(self):
        src = self.source
        for pane in ('outer_split', 'inner_split'):
            for side in ('start', 'end'):
                self.assertIn(
                    f'gtk_paned_set_shrink_{side}_child(GTK_PANED({pane}), TRUE);', src)
        self.assertIn('g_signal_connect(outer_split, "notify::position",', src)
        self.assertIn('g_signal_connect(inner_split, "notify::position",', src)

    def test_responsive_mode_observes_remaining_space(self):
        src = self.source
        self.assertIn('gtk_widget_get_width(editor->inner_split)', src)
        self.assertIn('if (remaining > 0 && remaining < threshold) right = FALSE;', src)
        self.assertIn('editor->updating_responsive = TRUE;', src)
        self.assertIn('editor->updating_responsive = FALSE;', src)
        self.assertIn('build_editor_workbench(editor, application);', src)
        self.assertIn('editor->editing_dock, editor->palette_dock);', src)


if __name__ == '__main__':
    unittest.main()
