# SPDX-License-Identifier: GPL-3.0-only
"""0124: no-ROM, deterministic export and saved-versus-staged isolation."""
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from scripts import editor_backend as backend
from scripts import project_room_entities as rooms
from scripts import project_room_package as package


class RoomExport0124(unittest.TestCase):
    def make_room(self, root):
        doc = rooms._new("mzm", "Brinstar", 7, 128, 64)
        rooms.collision_fill(doc, 0, 1, 2, 1, "solid")
        rooms.create(doc, "ENEMY", 16, 16, "Test enemy")
        rooms.door_create(doc, 64, 0, 16, 32, "Exit", "normal", "left")
        rooms.event_create(doc, "EVENT", 16, 16, 16, 16, "Checkpoint",
                           "enter", "checkpoint", "checkpoint:room_007", False)
        rooms.save(root, doc)
        return doc

    def test_export_is_deterministic_and_covers_authored_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            doc = self.make_room(root)
            scope = {"world": "zero_mission", "area": "Brinstar", "room": 7}
            first = backend.execute("room-export", scope, root=root)
            second = backend.execute("room-export", scope, root=root)
            self.assertEqual(first["sha256"], second["sha256"])
            self.assertEqual(first["package_dir"], second["package_dir"])
            folder = Path(first["package_dir"])
            data = json.loads((folder / "room.json").read_text())
            self.assertEqual(data["room"], doc)
            self.assertEqual(data["engine_adapter"], "unavailable")
            self.assertFalse(data["native_assets_included"])
            preview = (folder / "preview.tsv").read_text()
            self.assertIn("MVROOM-PREVIEW\t1\tmzm\tBrinstar\t7\t128\t64\t16", preview)
            self.assertIn("C\t0\t16\t16\t16\t1\n", preview)
            self.assertIn("D\t64\t0\t16\t32\t0\n", preview)
            self.assertIn("E\t16\t16\t16\t16\t0\n", preview)
            self.assertIn("V\t16\t16\t16\t16\t0\n", preview)
            self.assertTrue(preview.endswith("END\n"))

    def test_dry_run_never_writes_and_stage_is_never_exported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            doc = self.make_room(root)
            scope = {"world": "zero_mission", "area": "Brinstar", "room": 7}
            stage = "00000000-0000-4000-8000-000000000124"
            with patch.dict(os.environ, {"MV_EDITOR_ROOM_STAGE": stage}):
                rooms.collision_fill(doc, 2, 2, 1, 1, "hazard")
                rooms.save(root, doc)  # Staged data, NOT a diskette save.
                result = backend.execute("room-export", scope, root=root, dry_run=True)
                self.assertFalse(result["persisted"])
                self.assertFalse((root / "assets/extracted/exports").exists())
                saved = backend.execute("room-export", scope, root=root)
                data = json.loads((Path(saved["package_dir"]) / "room.json").read_text())
                self.assertEqual(len(data["room"]["collision"]["cells"]), 2)
                self.assertEqual(len(doc["collision"]["cells"]), 3)

    def test_rejects_missing_file_and_does_not_mutate_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scope = {"world": "zero_mission", "area": "Brinstar", "room": 7}
            with self.assertRaisesRegex(ValueError, "saved project room"):
                backend.execute("room-export", scope, root=root)
            doc = self.make_room(root)
            saved_path = rooms.path_for(root, "mzm", "Brinstar", 7, 128, 64)
            before = saved_path.read_bytes()
            backend.execute("room-export", scope, root=root)
            self.assertEqual(saved_path.read_bytes(), before)
            self.assertEqual(package.read_saved(root, "mzm", "Brinstar", 7), doc)


if __name__ == "__main__":
    unittest.main()
