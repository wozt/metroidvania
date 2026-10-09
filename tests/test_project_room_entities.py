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
        self.assertIn('"scripts/editor_cli.py"', source)
        self.assertIn('"entity-create"', source)
        self.assertIn('"collision-set"', source)
        self.assertIn('"door-create"', source)
        self.assertIn("draw_project_collision", source)
        self.assertIn("doc->tool_id == TOOL_SELECT", source)

    def test_legacy_entity_document_migrates_to_unified_room_data(self):
        path = pe.path_for(self.root, "mzm", "Brinstar", 2, 64, 32)
        path.parent.mkdir(parents=True)
        path.write_text(json.dumps({
            "schema": pe.LEGACY_SCHEMA, "version": 1, "world": "mzm",
            "area": "Brinstar", "room": 2, "width_px": 64, "height_px": 32,
            "next_id": 1, "entities": [],
        }), encoding="utf-8")
        document = pe.load(self.root, "mzm", "Brinstar", 2, 64, 32)
        self.assertEqual((document["schema"], document["version"]), (pe.SCHEMA, 2))
        self.assertEqual(document["collision"], {"resolution_px": 16, "cells": []})
        self.assertEqual(document["doors"], [])
        self.assertEqual(document["transitions"], [])
        self.assertIn(pe.LEGACY_SCHEMA, path.read_text())

    def test_collision_door_transition_round_trip_and_dependencies(self):
        document = pe.load(self.root, "aria", "0", 8, 64, 32)
        self.assertEqual(document["collision"]["resolution_px"], 8)
        self.assertEqual(pe.collision_fill(document, 0, 0, 3, 2, "solid"), 6)
        self.assertEqual(pe.collision_get(document, 2, 1), "solid")
        self.assertEqual(pe.collision_fill(document, 1, 0, 1, 1, "hazard"), 1)
        self.assertEqual(pe.collision_clear(document, 0, 1, 2, 1), 2)
        door = pe.door_create(document, 0, 0, 8, 16, "Project gate", "portal", "left")
        transition = pe.transition_create(
            document, door["id"], "mzm", "Brinstar", 3, 0, 16, 16)
        with self.assertRaisesRegex(ValueError, "linked transition"):
            pe.door_delete(document, door["id"])
        changed = pe.transition_update(document, transition["id"], spawn_x=24)
        self.assertEqual(changed["spawn_x"], 24)
        path = pe.save(self.root, document)
        restored = pe.load(self.root, "aria", "0", 8, 64, 32)
        self.assertEqual(len(restored["collision"]["cells"]), 4)
        self.assertEqual(restored["doors"], [door])
        self.assertEqual(restored["transitions"][0]["spawn_x"], 24)
        self.assertIn(pe.SCHEMA, path.read_text())
        pe.transition_delete(restored, transition["id"])
        pe.door_delete(restored, door["id"])
        self.assertEqual((restored["doors"], restored["transitions"]), ([], []))

    def test_geometry_rejects_invalid_bounds_types_and_duplicate_links(self):
        document = pe.load(self.root, "mzm", "Brinstar", 1, 64, 32)
        with self.assertRaises(ValueError):
            pe.collision_fill(document, 4, 0, 1, 1, "solid")
        with self.assertRaises(ValueError):
            pe.collision_fill(document, 0, 0, 1, 1, "invented")
        with self.assertRaises(ValueError):
            pe.door_create(document, 8, 0, 16, 16, "Bad", "normal", "left")
        door = pe.door_create(document, 0, 0, 16, 16, "Door", "normal", "left")
        pe.transition_create(document, door["id"], "aria", "0", 1, 0, 0, 0)
        with self.assertRaisesRegex(ValueError, "already linked"):
            pe.transition_create(document, door["id"], "aria", "0", 2, 0, 0, 0)


if __name__ == "__main__":
    unittest.main()
