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
        self.assertEqual((document["schema"], document["version"]),
                         (pe.SCHEMA, pe.VERSION))
        self.assertEqual(document["collision"], {"resolution_px": 16, "cells": []})
        self.assertEqual(document["doors"], [])
        self.assertEqual(document["transitions"], [])
        self.assertEqual(document["events"], [])
        self.assertIn(pe.LEGACY_SCHEMA, path.read_text())

    def test_version_two_room_document_migrates_events_in_memory(self):
        path = pe.path_for(self.root, "aria", "2", 4, 64, 32)
        path.parent.mkdir(parents=True)
        document = pe._new("aria", "2", 4, 64, 32)
        document["version"] = 2
        document.pop("next_event_id")
        document.pop("events")
        path.write_text(json.dumps(document), encoding="utf-8")
        migrated = pe.load(self.root, "aria", "2", 4, 64, 32)
        self.assertEqual(migrated["version"], pe.VERSION)
        self.assertEqual(migrated["next_event_id"], 1)
        self.assertEqual(migrated["events"], [])
        self.assertEqual(json.loads(path.read_text())["version"], 2)

    def test_version_three_events_migrate_conditions_in_memory(self):
        path = pe.path_for(self.root, "mzm", "Brinstar", 4, 64, 32)
        path.parent.mkdir(parents=True)
        document = pe._new("mzm", "Brinstar", 4, 64, 32)
        pe.event_create(
            document, "EVENT", 0, 0, 16, 16, "Legacy event", "enter",
            "checkpoint", "checkpoint:legacy", False)
        document["version"] = 3
        for event in document["events"]:
            event.pop("condition_mode")
            event.pop("conditions")
        path.write_text(json.dumps(document), encoding="utf-8")
        migrated = pe.load(self.root, "mzm", "Brinstar", 4, 64, 32)
        self.assertEqual(migrated["version"], pe.VERSION)
        self.assertEqual(migrated["events"][0]["condition_mode"], "all")
        self.assertEqual(migrated["events"][0]["conditions"], [])
        self.assertEqual(json.loads(path.read_text())["version"], 3)

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

    def test_event_regions_round_trip_in_both_worlds(self):
        for world, area, step in (("mzm", "Brinstar", 16), ("aria", "3", 8)):
            with self.subTest(world=world):
                document = pe.load(self.root, world, area, 9, 64, 64)
                event = pe.event_create(
                    document, "EVENT", step, step, step * 2, step,
                    "Start encounter", "enter", "spawn", "encounter.alpha", True)
                trigger = pe.event_create(
                    document, "TRIGGER", 0, 0, step, step,
                    "Save checkpoint", "interact", "checkpoint", "save.room9", False)
                changed = pe.event_update(
                    document, event["id"], label="Start boss encounter",
                    action_ref="encounter.boss")
                self.assertEqual(changed["label"], "Start boss encounter")
                pe.save(self.root, document)
                restored = pe.load(self.root, world, area, 9, 64, 64)
                self.assertEqual(len(restored["events"]), 2)
                self.assertEqual(restored["events"][0]["action_ref"], "encounter.boss")
                pe.event_delete(restored, trigger["id"])
                self.assertEqual([item["id"] for item in restored["events"]],
                                 [event["id"]])
                original = json.loads(json.dumps(restored))
                with self.assertRaises(ValueError):
                    pe.event_update(restored, event["id"], x=step // 2)
                self.assertEqual(restored, original)

    def test_event_conditions_protect_referenced_room_records(self):
        document = pe.load(self.root, "mzm", "Brinstar", 6, 64, 64)
        entity = pe.create(document, "ENEMY", 0, 0, "Condition entity")
        door = pe.door_create(
            document, 0, 0, 16, 16, "Condition door", "normal", "left")
        transition = pe.transition_create(
            document, door["id"], "aria", "0", 1, 0, 0, 0)
        prerequisite = pe.event_create(
            document, "EVENT", 0, 0, 16, 16, "Prerequisite", "enter",
            "checkpoint", "checkpoint:prerequisite", False)
        guarded = pe.event_create(
            document, "TRIGGER", 16, 16, 16, 16, "Guarded", "interact",
            "checkpoint", "checkpoint:guarded", True, "any", [
                {"type": "entity_present", "ref": f"entity:{entity['id']}",
                 "negated": False},
                {"type": "event_complete", "ref": f"event:{prerequisite['id']}",
                 "negated": True},
                {"type": "transition_ready",
                 "ref": f"transition:{transition['id']}", "negated": False},
            ])
        self.assertEqual((guarded["condition_mode"], len(guarded["conditions"])),
                         ("any", 3))
        with self.assertRaisesRegex(ValueError, "event condition"):
            pe.delete(document, entity["id"])
        with self.assertRaisesRegex(ValueError, "event condition"):
            pe.transition_delete(document, transition["id"])
        with self.assertRaisesRegex(ValueError, "event condition"):
            pe.event_delete(document, prerequisite["id"])
        snapshot = json.loads(json.dumps(document))
        with self.assertRaises(ValueError):
            pe.event_update(document, guarded["id"], condition_mode="none")
        self.assertEqual(document, snapshot)


if __name__ == "__main__":
    unittest.main()
