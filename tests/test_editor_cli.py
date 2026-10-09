# SPDX-License-Identifier: GPL-3.0-only
"""Functional contracts for the display-independent editor CLI."""
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from scripts.editor_cli import CliUsageError, _safe_output
from scripts.editor_backend import _event_validation, execute
from scripts import project_room_entities as project_rooms

ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "scripts/editor_cli.py"


class EditorCliTests(unittest.TestCase):
    def run_cli(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment.pop("DISPLAY", None)
        environment.pop("WAYLAND_DISPLAY", None)
        return subprocess.run(
            [sys.executable, str(CLI), *arguments], cwd=ROOT,
            env=environment, text=True, capture_output=True, check=False)

    def test_project_info_is_clean_json_without_display(self):
        completed = self.run_cli("--command=project-info", "--format=json")
        self.assertEqual(completed.returncode, 0, completed.stderr)
        payload = json.loads(completed.stdout)
        self.assertTrue(payload["success"])
        self.assertEqual(payload["data"]["gameplay_status"], "not_implemented")

    def test_unknown_command_option_is_a_usage_error(self):
        completed = self.run_cli(
            "--command=project-info", "--world=aria", "--format=json")
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(json.loads(completed.stdout)["error"]["type"], "usage")
        self.assertIn("unknown option", completed.stderr)

    def test_unavailable_command_is_explicit(self):
        completed = self.run_cli("--command=audio-list", "--format=json")
        self.assertEqual(completed.returncode, 3)
        payload = json.loads(completed.stdout)
        self.assertEqual(payload["error"]["type"], "unavailable")
        self.assertIn("pending", payload["error"]["message"])

    def test_atomic_dry_run_batch_does_not_create_a_room(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            batch = root / "batch.json"
            batch.write_text(json.dumps({"operations": [{
                "command": "room-create", "world": "aria", "area": 0,
                "slug": "dry_room", "name": "Dry room",
                "width_screens": 1, "height_screens": 1,
            }]}), encoding="utf-8")
            completed = self.run_cli(
                f"--batch={batch}", "--atomic=true", "--dry-run=true",
                f"--root={root}", "--format=json")
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertFalse((root / "assets").exists())

    def test_room_draft_and_placement_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            create = self.run_cli(
                "--command=room-create", "--world=zero_mission", "--area=0",
                "--slug=cli_room", "--name=CLI Room", "--width-screens=1",
                "--height-screens=1", f"--root={root}", "--format=json")
            self.assertEqual(create.returncode, 0, create.stderr)
            overview = root / "assets/extracted/world_overview"
            overview.mkdir(parents=True)
            (overview / "mzm.tsv").write_text(
                "0|0|31|31|0|0\n", encoding="utf-8")
            placed = self.run_cli(
                "--command=room-place", "--room=zero_mission:00:cli_room",
                "--x=2", "--y=3", f"--root={root}", "--format=json")
            self.assertEqual(placed.returncode, 0, placed.stderr)
            listed = self.run_cli(
                "--command=placement-list", "--world=zero_mission",
                f"--root={root}", "--format=tsv")
            self.assertEqual(
                listed.stdout,
                "zero_mission:00:cli_room\tCLI Room\tzero_mission\t0\t2\t3\t1\t1\n")
            refused = self.run_cli(
                "--command=room-unplace", "--room=zero_mission:00:cli_room",
                f"--root={root}", "--format=json")
            self.assertEqual(refused.returncode, 4)
            removed = self.run_cli(
                "--command=room-unplace", "--room=zero_mission:00:cli_room",
                "--confirm=true", f"--root={root}", "--format=json")
            self.assertEqual(removed.returncode, 0, removed.stderr)

    def test_project_entity_round_trip_uses_tsv_and_confirmation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scope = (
                "--world=zero_mission", "--area=Brinstar", "--room=7",
                "--width=64", "--height=32", f"--root={root}",
            )
            created = self.run_cli(
                "--command=entity-create", *scope, "--kind=ENEMY",
                "--x=0", "--y=0", "--label=CLI Enemy", "--format=json")
            self.assertEqual(created.returncode, 0, created.stderr)
            moved = self.run_cli(
                "--command=entity-move", *scope, "--id=1", "--x=16", "--y=0",
                "--format=json")
            self.assertEqual(moved.returncode, 0, moved.stderr)
            updated = self.run_cli(
                "--command=entity-update", *scope, "--id=1", "--x=32", "--y=0",
                "--label=Edited CLI Enemy", "--native-type=unassigned",
                "--format=json")
            self.assertEqual(updated.returncode, 0, updated.stderr)
            listed = self.run_cli(
                "--command=entity-list", *scope, "--preview=true", "--format=tsv")
            self.assertEqual(
                listed.stdout,
                "1\tENEMY\t32\t0\tEdited CLI Enemy\tunassigned\t-1\n")
            refused = self.run_cli(
                "--command=entity-delete", *scope, "--id=1", "--format=json")
            self.assertEqual(refused.returncode, 4)
            deleted = self.run_cli(
                "--command=entity-delete", *scope, "--id=1", "--confirm=true",
                "--format=json")
            self.assertEqual(deleted.returncode, 0, deleted.stderr)

    def test_aria_entity_update_keeps_typed_item_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            scope = (
                "--world=aria", "--area=3", "--room=7",
                "--width=64", "--height=32", f"--root={directory}",
            )
            created = self.run_cli(
                "--command=entity-create", *scope, "--kind=ITEM", "--x=8", "--y=0",
                "--label=Aria Item", "--native-type=pickup:03", "--item-id=1",
                "--parameter-0=2", "--parameter-1=3", "--flags=4",
                "--format=json")
            self.assertEqual(created.returncode, 0, created.stderr)
            updated = self.run_cli(
                "--command=entity-update", *scope, "--id=1", "--x=16", "--y=8",
                "--label=Edited Aria Item", "--native-type=pickup:03",
                "--item-id=12", "--parameter-0=513", "--parameter-1=999",
                "--flags=7", "--format=json")
            self.assertEqual(updated.returncode, 0, updated.stderr)
            entity = json.loads(updated.stdout)["data"]["entity"]
            self.assertEqual((entity["x"], entity["y"]), (16, 8))
            self.assertEqual(entity["label"], "Edited Aria Item")
            self.assertEqual(entity["settings"], {
                "item_id": 12, "parameter_0": 513,
                "parameter_1": 999, "flags": 7,
            })

    def test_collision_door_and_transition_cli_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            scope = (
                "--world=zero_mission", "--area=Brinstar", "--room=7",
                "--width=64", "--height=32", f"--root={directory}",
            )
            collision = self.run_cli(
                "--command=collision-fill", *scope, "--x=0", "--y=0",
                "--fill-width=2", "--fill-height=1", "--type=solid",
                "--format=json")
            self.assertEqual(collision.returncode, 0, collision.stderr)
            stroke = self.run_cli(
                "--command=collision-stroke", *scope,
                "--points=0,0;1,0;1,1;1,1", "--type=water", "--format=json")
            self.assertEqual(stroke.returncode, 0, stroke.stderr)
            self.assertEqual(json.loads(stroke.stdout)["data"]["points"], 3)
            listed_collision = self.run_cli(
                "--command=collision-list", *scope, "--format=tsv")
            self.assertEqual(
                listed_collision.stdout,
                "0\t0\twater\t16\n1\t0\twater\t16\n1\t1\twater\t16\n")
            door = self.run_cli(
                "--command=door-create", *scope, "--x=0", "--y=0",
                "--door-width=16", "--door-height=32", "--label=CLI Gate",
                "--door-type=portal", "--facing=left", "--format=json")
            self.assertEqual(door.returncode, 0, door.stderr)
            transition = self.run_cli(
                "--command=door-link", *scope, "--source-door-id=1",
                "--target-world=zero_mission", "--target-area=Brinstar",
                "--target-room=3", "--target-door-id=0", "--spawn-x=16",
                "--spawn-y=16", "--format=json")
            self.assertEqual(transition.returncode, 0, transition.stderr)
            listed_transition = self.run_cli(
                "--command=transition-list", *scope, "--format=tsv")
            self.assertEqual(
                listed_transition.stdout, "1\t1\tmzm\tBrinstar\t3\t0\t16\t16\n")
            refused = self.run_cli(
                "--command=door-delete", *scope, "--id=1", "--confirm=true",
                "--format=json")
            self.assertEqual(refused.returncode, 4)
            removed_transition = self.run_cli(
                "--command=transition-delete", *scope, "--id=1", "--confirm=true",
                "--format=json")
            self.assertEqual(removed_transition.returncode, 0, removed_transition.stderr)
            removed_door = self.run_cli(
                "--command=door-delete", *scope, "--id=1", "--confirm=true",
                "--format=json")
            self.assertEqual(removed_door.returncode, 0, removed_door.stderr)

    def test_collision_capabilities_report_both_native_formats(self):
        for world, resolution in (("zero_mission", 16), ("aria", 8)):
            with self.subTest(world=world):
                completed = self.run_cli(
                    "--command=collision-capabilities", f"--world={world}",
                    "--format=json")
                self.assertEqual(completed.returncode, 0, completed.stderr)
                data = json.loads(completed.stdout)["data"]
                self.assertEqual(data["world"], world)
                self.assertEqual(data["resolution_px"], resolution)
                self.assertEqual(data["native_encoder"], "unavailable")
                by_type = {entry["type"]: entry for entry in data["types"]}
                self.assertEqual(set(by_type), {
                    "solid", "one_way", "hazard", "slope_up", "slope_down",
                    "water", "air",
                })
                self.assertEqual(
                    by_type["one_way"]["native_reference_status"],
                    "verified_exact")

    def test_event_region_cli_round_trip_in_both_worlds(self):
        with tempfile.TemporaryDirectory() as directory:
            capabilities = self.run_cli(
                "--command=capabilities", f"--root={directory}", "--format=json")
            self.assertEqual(capabilities.returncode, 0, capabilities.stderr)
            commands = json.loads(capabilities.stdout)["data"]["commands"]
            for command in ("event-list", "event-inspect", "event-create",
                            "event-update", "event-delete", "event-validate"):
                self.assertEqual(commands[command],
                                 "available_project_room_data_only")
            for world, area, step in (("zero_mission", "Brinstar", 16),
                                      ("aria", "3", 8)):
                with self.subTest(world=world):
                    scope = (
                        f"--world={world}", f"--area={area}", "--room=5",
                        "--width=64", "--height=64", f"--root={directory}",
                    )
                    created = self.run_cli(
                        "--command=event-create", *scope, "--event-kind=TRIGGER",
                        f"--x={step}", f"--y={step}", f"--region-width={step * 2}",
                        f"--region-height={step}", "--label=Checkpoint trigger",
                        "--trigger-type=enter", "--action-type=checkpoint",
                        "--action-ref=checkpoint:room5", "--once=true",
                        "--condition-mode=any",
                        '--conditions=[{"type":"checkpoint_active",'
                        '"ref":"checkpoint:alpha","negated":false},'
                        '{"type":"checkpoint_active",'
                        '"ref":"checkpoint:beta","negated":true}]',
                        "--format=json")
                    self.assertEqual(created.returncode, 0, created.stderr)
                    updated = self.run_cli(
                        "--command=event-update", *scope, "--id=1",
                        "--event-kind=EVENT", "--x=0", "--y=0",
                        f"--region-width={step}", f"--region-height={step}",
                        "--label=Checkpoint event", "--trigger-type=interact",
                        "--action-type=checkpoint", "--action-ref=checkpoint:room5_return",
                        "--once=false", "--condition-mode=all",
                        '--conditions=[{"type":"checkpoint_active",'
                        '"ref":"checkpoint:alpha","negated":false}]',
                        "--format=json")
                    self.assertEqual(updated.returncode, 0, updated.stderr)
                    validation = self.run_cli(
                        "--command=event-validate", *scope, "--format=tsv")
                    self.assertEqual(
                        validation.stdout,
                        "1\tcheckpoint\tcheckpoint:room5_return\t1\t"
                        "valid_checkpoint_key\tall\t1\t1\t0:valid_checkpoint_key\n")
                    listed = self.run_cli(
                        "--command=event-list", *scope, "--format=tsv")
                    self.assertEqual(
                        listed.stdout,
                        f"1\tEVENT\t0\t0\t{step}\t{step}\tCheckpoint event\t"
                        "interact\tcheckpoint\tcheckpoint:room5_return\t0\tall\t"
                        "checkpoint_active,checkpoint:alpha,0\n")
                    rejected = self.run_cli(
                        "--command=event-update", *scope, "--id=1",
                        "--action-type=story", "--action-ref=timeline:missing",
                        "--format=json")
                    self.assertEqual(rejected.returncode, 4)
                    rejected_condition = self.run_cli(
                        "--command=event-update", *scope, "--id=1",
                        '--conditions=[{"type":"entity_present",'
                        '"ref":"entity:999","negated":false}]',
                        "--format=json")
                    self.assertEqual(rejected_condition.returncode, 4)
                    unchanged = self.run_cli(
                        "--command=event-list", *scope, "--format=tsv")
                    self.assertEqual(unchanged.stdout, listed.stdout)
                    deleted = self.run_cli(
                        "--command=event-delete", *scope, "--id=1",
                        "--confirm=true", "--format=json")
                    self.assertEqual(deleted.returncode, 0, deleted.stderr)

    def test_event_reference_validation_resolves_every_action_family(self):
        document = project_rooms._new("mzm", "Brinstar", 5, 64, 64)
        entity = project_rooms.create(
            document, "ENEMY", 0, 0, "Spawn target")
        door = project_rooms.door_create(
            document, 0, 0, 16, 16, "Exit", "normal", "left")
        transition = project_rooms.transition_create(
            document, door["id"], "aria", "0", 1, 0, 0, 0)
        first = project_rooms.event_create(
            document, "EVENT", 0, 0, 16, 16, "Checkpoint", "enter",
            "checkpoint", "checkpoint:test_room", False,
            conditions=[{
                "type": "story_flag", "ref": "flag:start_choice_locked",
                "negated": False,
            }])
        references = (
            ("story", "timeline:shared.start_choice"),
            ("story", "cutscene:interzone_first_meeting"),
            ("spawn", f"entity:{entity['id']}"),
            ("toggle", f"event:{first['id']}"),
            ("transition", f"transition:{transition['id']}"),
        )
        for index, (action, reference) in enumerate(references, 1):
            project_rooms.event_create(
                document, "TRIGGER", 16, 16, 16, 16,
                f"Reference {index}", "interact", action, reference, False)
        first["condition_mode"] = "any"
        first["conditions"] = [
            {"type": "story_flag", "ref": "flag:start_choice_locked",
             "negated": False},
            {"type": "event_complete", "ref": "event:2", "negated": False},
            {"type": "entity_present", "ref": f"entity:{entity['id']}",
             "negated": True},
            {"type": "transition_ready",
             "ref": f"transition:{transition['id']}", "negated": False},
            {"type": "checkpoint_active", "ref": "checkpoint:test_room",
             "negated": False},
        ]
        records = _event_validation(document, ROOT)
        self.assertTrue(all(record["valid"] for record in records), records)
        self.assertEqual(
            [condition["reference_status"]
             for condition in records[0]["conditions"]],
            ["verified_story_flag", "verified_project_event",
             "verified_project_entity", "verified_project_transition",
             "valid_checkpoint_key"])
        with self.assertRaisesRegex(ValueError, "referenced by a project event"):
            project_rooms.delete(document, entity["id"])
        with self.assertRaisesRegex(ValueError, "referenced by a project event"):
            project_rooms.transition_delete(document, transition["id"])
        with self.assertRaisesRegex(ValueError, "referenced by another project event"):
            project_rooms.event_delete(document, first["id"])
        document["events"][-1]["action_ref"] = "transition:999"
        self.assertEqual(
            _event_validation(document, ROOT)[-1]["reference_status"],
            "missing_project_transition")

    def test_story_save_validates_and_writes_through_backend(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "data/cutscenes").mkdir(parents=True)
            source = root / "candidate.toml"
            source.write_text(
                'schema = "metroid-vania-cutscene"\nversion = 1\n'
                'id = "cli_scene"\nworld = "interzone"\n'
                '[[actors]]\nid = "soma"\n'
                '[[steps]]\ntype = "dialogue"\nactor = "soma"\ntext = "Hello"\n',
                encoding="utf-8")
            saved = self.run_cli(
                "--command=story-save", "--kind=cutscene", f"--input={source}",
                "--target=data/cutscenes/cli_scene.toml", f"--root={root}",
                "--format=json")
            self.assertEqual(saved.returncode, 0, saved.stderr)
            self.assertEqual((root / "data/cutscenes/cli_scene.toml").read_text(),
                             source.read_text())
            traversal = self.run_cli(
                "--command=story-save", "--kind=cutscene", f"--input={source}",
                "--target=../escape.toml", f"--root={root}", "--format=json")
            self.assertEqual(traversal.returncode, 4)

    def test_room_open_dry_run_validates_without_private_output(self):
        with tempfile.TemporaryDirectory() as directory:
            completed = self.run_cli(
                "--command=room-open", "--world=zero_mission",
                "--area=Brinstar", "--room=3", "--dry-run=true",
                f"--root={directory}", "--format=json")
            self.assertEqual(completed.returncode, 0, completed.stderr)
            payload = json.loads(completed.stdout)
            self.assertFalse(payload["data"]["persisted"])
            self.assertEqual(payload["created_paths"], [])

    def test_tile_fill_uses_native_core_adapter_without_writing_in_dry_run(self):
        room = {"world": "zero_mission", "area": "Brinstar", "room": 3,
                "native_id": "mzm:brinstar:003"}
        base = ROOT / "assets/extracted/rooms/metroid/workrooms/test.mvnative"
        override = ROOT / "assets/extracted/overrides/metroid/test.mvnative"
        with (mock.patch("scripts.editor_backend._native_room", return_value=room),
              mock.patch("scripts.editor_backend._native_workspace_paths",
                         return_value=(base, override)),
              mock.patch("scripts.editor_backend._check_native_path"),
              mock.patch("scripts.editor_backend._native_map_tool",
                         return_value=["CHANGED", "1"]) as adapter):
            result = execute("tile-fill", {
                "world": "zero_mission", "area": "Brinstar", "room": 3,
                "layer": "bg2", "x": 4, "y": 5, "tile_id": 7,
            }, root=ROOT, dry_run=True)
        self.assertTrue(result["changed"])
        self.assertFalse(result["persisted"])
        arguments = adapter.call_args.args[1]
        self.assertIn("--command=fill", arguments)
        self.assertIn("--dry-run=true", arguments)
        self.assertFalse(any(value.startswith("--output=") for value in arguments))

    def test_gui_business_operations_reference_only_shared_cli(self):
        sources = [
            ROOT / "editor/room_browser.c", ROOT / "editor/native_workspace.c",
            ROOT / "editor/world_atlas.c", ROOT / "editor/object_catalog.c",
            ROOT / "editor/story_workspace.c",
        ]
        combined = "\n".join(path.read_text(encoding="utf-8") for path in sources)
        self.assertIn("scripts/editor_cli.py", combined)
        for legacy in (
                '"scripts.authored_rooms"', '"scripts.map_placements"',
                '"scripts.project_room_entities"', '"scripts.object_catalog"',
                '"scripts.validate_story_assets"'):
            self.assertNotIn(legacy, combined)

    def test_output_refuses_symlink_parent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "target"
            target.mkdir()
            link = root / "link"
            link.symlink_to(target, target_is_directory=True)
            with self.assertRaises(CliUsageError):
                _safe_output(link / "result.json", "{}\n")

    def test_batch_refuses_symlink_parent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "target"
            target.mkdir()
            batch = target / "batch.json"
            batch.write_text('{"operations":[{"command":"project-info"}]}',
                             encoding="utf-8")
            link = root / "link"
            link.symlink_to(target, target_is_directory=True)
            completed = self.run_cli(f"--batch={link / 'batch.json'}", "--format=json")
            self.assertEqual(completed.returncode, 2)


if __name__ == "__main__":
    unittest.main()
