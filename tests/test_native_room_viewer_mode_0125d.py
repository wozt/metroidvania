# SPDX-License-Identifier: GPL-3.0-only
"""0125d: authentic-background mode must never invent game objects or walls."""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from scripts import native_room_viewer_demo as demo


class NativeRoomViewerMode0125d(unittest.TestCase):
    def _run(self, root: Path, *, fake: bool = False):
        response = {
            "layers": {"Bg1": {
                "status": "DECODED_METATILES", "width_blocks": 8,
                "height_blocks": 6,
                "path": "rooms/metroid/previews/brinstar_033_bg1.bmp",
            }}
        }
        output = io.StringIO()
        argv = ["native_room_viewer_demo", "--area", "Brinstar", "--room", "33"]
        if fake:
            argv.append("--demo-overlays")
        with (patch.object(demo, "ROOT", root),
              patch.object(demo.native, "decode_room", return_value=response),
              patch.object(sys, "argv", argv),
              contextlib.redirect_stdout(output)):
            self.assertEqual(demo.main(), 0)
        parent = root / "assets/extracted/native_demo_0125/assets/extracted/exports/mzm"
        packages = list(parent.glob("brinstar_033_*/room.json"))
        self.assertEqual(len(packages), 1)
        exported = json.loads(packages[0].read_text(encoding="utf-8"))
        return exported["room"], output.getvalue(), packages[0].parent / "preview.tsv"

    def test_default_is_authentic_source_without_synthetic_project_markers(self):
        with tempfile.TemporaryDirectory() as temp:
            room, stdout, preview = self._run(Path(temp))
            self.assertEqual(room["width_px"], 128)
            self.assertEqual(room["height_px"], 96)
            for field in ("entities", "doors", "transitions", "events"):
                self.assertEqual(room[field], [], field)
            self.assertEqual(room["collision"]["cells"], [])
            self.assertIn("no invented collision", stdout)
            text = preview.read_text(encoding="ascii")
            self.assertIn("MVROOM-PREVIEW\t1\tmzm\tBrinstar\t33\t128\t96\t16", text)
            self.assertTrue(text.endswith("END\n"))
            self.assertNotIn("\nC\t", text)
            self.assertNotIn("\nD\t", text)
            self.assertNotIn("\nE\t", text)

    def test_synthetic_geometry_requires_explicit_demo_switch(self):
        with tempfile.TemporaryDirectory() as temp:
            room, stdout, _ = self._run(Path(temp), fake=True)
            self.assertTrue(room["collision"]["cells"])
            self.assertTrue(room["entities"])
            self.assertTrue(room["doors"])
            self.assertIn("synthetic project", stdout)


if __name__ == "__main__":
    unittest.main()
