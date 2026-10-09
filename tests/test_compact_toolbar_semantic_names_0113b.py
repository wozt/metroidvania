# SPDX-License-Identifier: GPL-3.0-only
"""0113b: icon-only GTK category toggles keep stable semantic identifiers."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class CompactToolbarSemanticNames0113b(unittest.TestCase):
    def test_category_and_hatch_controls_have_semantic_names(self):
        source = (ROOT / "editor/native_workspace.c").read_text(encoding="utf-8")
        self.assertIn("PATCH_0113B_SEMANTIC_TOOLBAR_NAMES", source)
        self.assertIn('"mv-room-toggle-name", (gpointer)overlay_labels[i]', source)
        self.assertIn('"mv-room-toggle-name", (gpointer)"Animate hatches"', source)

    def test_real_gtk_lifecycle_tests_use_semantic_lookup(self):
        source = (ROOT / "tests/test_native_workspace_gtk.c").read_text(encoding="utf-8")
        func = source.split("static GtkWidget *find_room_toggle(", 1)[1]
        func = func.split("static void test_native_door_bitflags_imported(", 1)[0]
        self.assertIn('g_object_get_data(G_OBJECT(root), "mv-room-toggle-name")', func)
        self.assertIn('g_object_get_data(G_OBJECT(root), "mv-room-tool-name")', func)
        self.assertIn("gtk_button_get_label(GTK_BUTTON(root))", func)
        for case in ("native-doors-default-visible", "native-door-bitflags-imported",
                     "hatch-preview-lifetime"):
            self.assertIn(case, source)


if __name__ == "__main__":
    unittest.main()
