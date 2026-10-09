# SPDX-License-Identifier: GPL-3.0-only
"""Actual create/move/delete and persistence contract, no commercial ROM required."""
import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from scripts import project_room_entities as pe


class ProjectEntityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_create_move_delete_two_worlds_and_persistence(self):
        for world, area in (("mzm", "Brinstar"), ("aria", "0")):
            document = pe.load(self.root, world, area, 23, 480, 320)
            for i, kind in enumerate(pe.KINDS):
                self.assertEqual(pe.create(document, kind, 16 * i, 16,
                                           f"Custom {kind.lower()}")["id"], i + 1)
            path = pe.save(self.root, document)
            self.assertIn("assets/extracted/overrides/", str(path))
            fresh = pe.load(self.root, world, area, 23, 480, 320)
            self.assertEqual(len(fresh["entities"]), 3)
            pe.move(fresh, 1, 48, 96)
            pe.delete(fresh, 2)
            pe.save(self.root, fresh)
            self.assertEqual([(e["id"], e["x"], e["y"])
                              for e in pe.load(self.root, world, area, 23, 480, 320)["entities"]],
                             [(1, 48, 96), (3, 32, 16)])

    def test_strict_geometry_ids_categories_and_evil_text(self):
        doc = pe.load(self.root, "mzm", "Brinstar", 0, 240, 160)
        for kind, x, y, label, token in (("DOOR", 0, 0, "No", "unassigned"),
                                           ("ENEMY", 1, 0, "No", "unassigned"),
                                           ("ITEM", 240, 0, "No", "unassigned"),
                                           ("OBJECT", 0, 0, "bad\nline", "unassigned"),
                                           ("OBJECT", 0, 0, "Name", "../../rom")):
            with self.subTest(kind=kind, x=x), self.assertRaises(ValueError):
                pe.create(doc, kind, x, y, label, token)
        self.assertEqual(doc["entities"], [])
        pe.create(doc, "ENEMY", 0, 0, "Enemy")
        pe.save(self.root, doc)
        with self.assertRaises(ValueError):
            pe.load(self.root, "mzm", "Brinstar", 0, 320, 160)
        path = pe.path_for(self.root, "mzm", "Brinstar", 0, 240, 160)
        tampered = json.loads(path.read_text())
        tampered["entities"][0]["id"] = 9
        path.write_text(json.dumps(tampered))
        with self.assertRaises(ValueError):
            pe.load(self.root, "mzm", "Brinstar", 0, 240, 160)

    def test_cli_list_and_symlink_refusal(self):
        base = ["--root", str(self.root), "--world", "aria", "--area", "1",
                "--room", "3", "--width", "320", "--height", "160"]
        with contextlib.redirect_stdout(io.StringIO()) as stdout:
            self.assertEqual(pe.main(base + ["create", "--kind", "ITEM", "--x", "16", "--y", "32", "--label", "Custom Potion"]), 0)
        self.assertIn("CREATED 1", stdout.getvalue())
        with contextlib.redirect_stdout(io.StringIO()) as stdout:
            self.assertEqual(pe.main(base + ["list"]), 0)
        self.assertEqual(stdout.getvalue(), "1\tITEM\t16\t32\tCustom Potion\tunassigned\n")
        path = pe.path_for(self.root, "aria", "1", 3, 320, 160)
        path.unlink()
        path.symlink_to(self.root / "fake.json")
        with self.assertRaises(ValueError):
            pe.load(self.root, "aria", "1", 3, 320, 160)

    def test_native_records_are_not_project_edited(self):
        source = (Path(__file__).resolve().parents[1] / "editor/native_workspace.c").read_text()
        self.assertIn("item->project_owned", source)
        self.assertIn("Project entity", source)
        self.assertIn('"scripts.project_room_entities"', source)
        self.assertIn("doc->tool_id == TOOL_SELECT", source)


if __name__ == "__main__":
    unittest.main()
