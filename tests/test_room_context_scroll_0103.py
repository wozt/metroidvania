# SPDX-License-Identifier: GPL-3.0-only
"""Source contracts for the GTK4 context anchor scroll fix."""
from pathlib import Path
import unittest

SRC = Path(__file__).resolve().parents[1] / "editor/native_workspace.c"


class CanvasContextAnchorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SRC.read_text()

    def test_popovers_parent_to_fixed_scroller(self):
        block = self.source.split("static void room_context_popover_place_0103(",1)[1].split(
            "static void project_context_empty(",1)[0]
        self.assertIn("gtk_widget_compute_point(relative, doc->scroller", block)
        self.assertIn("anchor = doc->scroller;", block)
        self.assertIn("gtk_widget_set_parent(popover, anchor);", block)
        self.assertIn("gtk_popover_set_pointing_to", block)

    def test_both_contexts_share_fixed_anchor(self):
        self.assertEqual(self.source.count(
            "room_context_popover_place_0103(doc, popover,"), 2)
        self.assertIn("room_context_popover_place_0103(doc, popover, canvas, x, y)", self.source)
        self.assertIn("room_context_popover_place_0103(doc, popover, relative, x, y)", self.source)


if __name__ == "__main__":
    unittest.main()
