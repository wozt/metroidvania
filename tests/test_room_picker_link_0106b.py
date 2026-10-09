# SPDX-License-Identifier: GPL-3.0-only
"""CMake link contract for the real GTK atlas picker (0106b)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class AtlasPickerLinkContract(unittest.TestCase):
    def test_both_gtk_targets_link_world_atlas(self):
        content = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        for target in ("fusion_native_workspace_gtk_tests",
                       "fusion_room_browser_gtk_tests"):
            with self.subTest(target=target):
                section = content.split(f"add_executable({target}", 1)[1].split(
                    "\n        )", 1)[0]
                self.assertIn("editor/native_workspace.c", section)
                self.assertIn("editor/world_atlas.c", section)

    def test_linked_functions_are_real(self):
        source = (ROOT / "editor/world_atlas.c").read_text(encoding="utf-8")
        self.assertIn("gboolean world_atlas_begin_room_pick(", source)
        self.assertIn("void world_atlas_abandon_room_pick(", source)

if __name__ == "__main__":
    unittest.main()
