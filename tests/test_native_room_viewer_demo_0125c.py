# SPDX-License-Identifier: GPL-3.0-only
"""0125c: the native-sized demo initializes its isolated directory without ROMs."""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from scripts import native_room_viewer_demo as demo


class NativeRoomViewerDemo0125c(unittest.TestCase):
    def test_fresh_private_root_can_save_and_export(self):
        with tempfile.TemporaryDirectory() as directory:
            temporary_root = Path(directory)
            sandbox = temporary_root / "assets/extracted/native_demo_0125"
            self.assertFalse(sandbox.exists())
            fake_source = {
                "layers": {"Bg1": {
                    "status": "DECODED_METATILES",
                    "width_blocks": 8, "height_blocks": 6,
                    "path": "rooms/metroid/previews/brinstar_033_bg1.bmp",
                }}
            }
            output = io.StringIO()
            with (patch.object(demo, "ROOT", temporary_root),
                  patch.object(demo.native, "decode_room", return_value=fake_source),
                  patch.object(sys, "argv", ["native_room_viewer_demo",
                                              "--area", "Brinstar", "--room", "33"]),
                  contextlib.redirect_stdout(output)):
                self.assertEqual(demo.main(), 0)
                self.assertEqual(demo.main(), 0)  # Stable reruns.
            self.assertTrue(sandbox.is_dir())
            saved = sandbox / "assets/extracted/overrides/metroid/entities/brinstar_033.json"
            self.assertTrue(saved.is_file())
            package_dirs = list((sandbox / "assets/extracted/exports/mzm").glob("brinstar_033_*"))
            self.assertEqual(len(package_dirs), 1)
            exported = json.loads((package_dirs[0] / "room.json").read_text(encoding="utf-8"))
            self.assertEqual(exported["room"]["width_px"], 128)
            self.assertEqual(exported["room"]["height_px"], 96)
            self.assertFalse(exported["native_assets_included"])
            self.assertTrue((package_dirs[0] / "preview.tsv").is_file())
            self.assertIn("fusion_room_package_viewer", output.getvalue())


if __name__ == "__main__":
    unittest.main()
