# SPDX-License-Identifier: GPL-3.0-only
"""Native editor project-room commands against the Python backend (transitional).

The same command sequence runs on two copies of a fixture root, one through
each backend; every output and the final file trees must be identical.
"""
import filecmp
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
C = str(ROOT / "build/fusion_editor_cli")


def _run(args, root, python, extra=None):
    env = dict(os.environ)
    env.pop("MV_EDITOR_ROOM_STAGE", None)
    if extra:
        env.update(extra)
    if python:
        env["FUSION_EDITOR_PYTHON_BACKEND"] = "1"
    result = subprocess.run([C, *args, f"--root={root}"], capture_output=True, text=True,
                            env=env, cwd=root)
    return (result.returncode, result.stdout.replace(str(root), "<ROOT>"),
            result.stderr.replace(str(root), "<ROOT>"))


def _fixture(base):
    base.mkdir(parents=True)
    shutil.copytree(ROOT / "data/story", base / "data/story")
    shutil.copytree(ROOT / "data/cutscenes", base / "data/cutscenes")
    annotations = base / "assets/extracted/rooms/metroid/annotations"
    annotations.mkdir(parents=True)
    (annotations / "brinstar_005.tsv").write_text(
        "DOOR|3|0|32|16|48|hatch|blue|x|y\nDOOR|4|224|32|16|48|hatch|red|x|y\n"
        "ITEM|1|0|0|1|1|a|b|c|d\n")
    (base / "story_in.toml").write_text(
        (ROOT / "data/cutscenes/interzone_first_meeting.toml").read_text())
    (base / "bad.toml").write_text(
        'schema = "metroid-vania-cutscene"\nversion = 1\nid = "BAD"\n')
    (base / "broken.toml").write_text('schema = "unterminated\n')
    overview = base / "assets/extracted/world_overview"
    overview.mkdir(parents=True)
    (overview / "mzm.tsv").write_text("# area|room|x|y|save|warp\n0|1|10|10|0|0|1\n0|2|11|10|1|0|3|4\n")
    (overview / "aria.tsv").write_text("# cells\n2|5|30|20|0|1|2\n")
    entities = base / "assets/extracted/overrides/aria/entities"
    entities.mkdir(parents=True)
    (entities / "area_01_room_003.json").write_text(json.dumps({
        "schema": "metroidvania.project-room-entities", "version": 1, "world": "aria",
        "area": "1", "room": 3, "width_px": 320, "height_px": 160, "next_id": 2,
        "entities": [{"id": 1, "kind": "ITEM", "x": 16, "y": 24, "label": "Potion",
                      "native_type": "pickup:02",
                      "settings": {"item_id": 3, "parameter_0": 0, "parameter_1": 1,
                                   "flags": 0}}]}, indent=1))
    return base


Z = ["--world=zero_mission", "--area=Brinstar", "--room=5", "--width=256", "--height=160"]
Z2 = ["--world=zero_mission", "--area=0", "--room=6", "--width=240", "--height=160"]
A = ["--world=aria", "--area=1", "--room=3", "--width=320", "--height=160"]
EVENT = ["--x=16", "--y=16", "--region-width=32", "--region-height=16"]
SEQUENCE = [
    ["--command=collision-capabilities", "--world=aria", "--format=json"],
    ["--command=collision-capabilities", "--world=zero_mission"],
    ["--command=collision-list", *Z], ["--command=collision-list", *Z, "--format=tsv"],
    ["--command=collision-set", *Z, "--x=2", "--y=3", "--type=solid"],
    ["--command=collision-fill", *Z, "--x=0", "--y=0", "--fill-width=4", "--fill-height=2",
     "--type=water"],
    ["--command=collision-fill", *Z, "--x=0", "--y=0", "--type=water"],
    ["--command=collision-fill", *Z, "--x=15", "--y=0", "--fill-width=4", "--fill-height=2",
     "--type=water"],
    ["--command=collision-stroke", *Z, "--points=1,1;2,1;1,1;5,5", "--type=hazard",
     "--format=json"],
    ["--command=collision-stroke", *Z, "--points=1,1;99,1", "--type=hazard"],
    ["--command=collision-stroke", *Z, "--points=1;2", "--type=hazard"],
    ["--command=collision-get", *Z, "--x=1", "--y=1"],
    ["--command=collision-get", *Z, "--x=9", "--y=9"],
    ["--command=collision-clear", *Z, "--x=0", "--y=0", "--fill-width=2", "--fill-height=2"],
    ["--command=collision-clear", *Z, "--x=0", "--y=0", "--fill-width=2", "--fill-height=2",
     "--confirm=true"],
    ["--command=collision-validate", *Z], ["--command=collision-list", *Z, "--format=tsv"],
    ["--command=collision-set", *Z, "--x=2", "--y=3", "--type=lava"],
    ["--command=door-create", *Z, "--x=0", "--y=32", "--door-width=16", "--door-height=48",
     "--label=West", "--door-type=normal", "--facing=left"],
    ["--command=door-create", *Z, "--x=5", "--y=32", "--door-width=16", "--door-height=48",
     "--label=Bad", "--door-type=normal", "--facing=left"],
    ["--command=door-create", *Z, "--x=240", "--y=32", "--door-width=16", "--door-height=48",
     "--label=East", "--door-type=boss", "--facing=right", "--format=json"],
    ["--command=door-adopt", *Z, "--native-index=3", "--native-variant=hatch",
     "--native-type=blue"],
    ["--command=door-adopt", *Z, "--native-index=3", "--native-variant=hatch",
     "--native-type=blue"],
    ["--command=door-adopt", *Z, "--native-index=9", "--native-variant=hatch",
     "--native-type=blue"],
    ["--command=door-list", *Z, "--format=tsv"], ["--command=door-list", *Z, "--format=json"],
    ["--command=door-update", *Z, "--id=2", "--x=240", "--y=16", "--door-width=16",
     "--door-height=48", "--label=East2", "--door-type=locked", "--facing=right"],
    ["--command=door-update", *Z, "--id=9", "--x=240", "--y=16", "--door-width=16",
     "--door-height=48", "--label=E", "--door-type=locked", "--facing=right"],
    ["--command=door-inspect", *Z, "--id=1"], ["--command=door-inspect", *Z, "--id=7"],
    ["--command=door-create", *Z2, "--x=0", "--y=32", "--door-width=16", "--door-height=48",
     "--label=Back", "--door-type=normal", "--facing=left"],
    ["--command=door-target-list", "--target-world=zero_mission", "--target-area=Brinstar",
     "--target-room=6", "--format=tsv"],
    ["--command=door-target-list", "--target-world=zero_mission", "--target-area=brinstar",
     "--target-room=6", "--target-door-id=5"],
    ["--command=door-target-list", "--target-world=aria", "--target-area=1",
     "--target-room=3", "--format=json"],
    ["--command=door-target-list", "--target-world=zero_mission", "--target-area=0",
     "--target-room=900"],
    ["--command=transition-create", *Z, "--source-door-id=1", "--target-world=zero_mission",
     "--target-area=Brinstar", "--target-room=6", "--target-door-id=1", "--spawn-x=16",
     "--spawn-y=48"],
    ["--command=transition-create", *Z, "--source-door-id=1", "--target-world=zero_mission",
     "--target-area=Brinstar", "--target-room=6"],
    ["--command=door-link", *Z, "--source-door-id=2", "--target-world=aria",
     "--target-area=1", "--target-room=3"],
    ["--command=transition-list", *Z, "--format=tsv"],
    ["--command=transition-validate", *Z, "--format=json"],
    ["--command=transition-update", *Z, "--id=2", "--source-door-id=2", "--target-world=aria",
     "--target-area=1", "--target-room=4"],
    ["--command=door-return-plan", "--world=zero_mission", "--area=Brinstar", "--room=5",
     "--source-door-id=1", "--format=tsv"],
    ["--command=door-return-plan", "--world=zero_mission", "--area=Brinstar", "--room=5",
     "--source-door-id=2"],
    ["--command=connection-list", "--world=zero_mission", "--format=tsv"],
    ["--command=connection-list", "--world=zero_mission", "--area=0", "--room=5",
     "--format=json"],
    ["--command=connection-list", "--world=zero_mission", "--room=5"],
    ["--command=connection-list", "--world=aria"],
    ["--command=door-delete", *Z, "--id=1", "--confirm=true"],
    ["--command=event-create", *Z, "--event-kind=TRIGGER", *EVENT, "--label=Enter",
     "--trigger-type=enter", "--action-type=story",
     "--action-ref=timeline:shared.start_choice", "--once=true"],
    ["--command=event-create", *Z, "--event-kind=EVENT", *EVENT, "--label=Bad",
     "--trigger-type=enter", "--action-type=story", "--action-ref=timeline:nope",
     "--once=true"],
    ["--command=event-create", *Z, "--event-kind=EVENT", *EVENT, "--label=Cut",
     "--trigger-type=touch", "--action-type=story",
     "--action-ref=cutscene:interzone_first_meeting", "--once=false", "--condition-mode=any",
     '--conditions=[{"type":"story_flag","ref":"flag:start_choice_locked","negated":false},'
     '{"type":"event_complete","ref":"event:1","negated":true}]'],
    ["--command=event-create", *Z, "--event-kind=EVENT", *EVENT, "--label=C2",
     "--trigger-type=touch", "--action-type=toggle", "--action-ref=event:1", "--once=false",
     '--conditions=[{"type":"story_flag","ref":"flag:missing","negated":false}]'],
    ["--command=event-create", *Z, "--event-kind=EVENT", *EVENT, "--label=C3",
     "--trigger-type=touch", "--action-type=checkpoint", "--action-ref=checkpoint:a.b",
     "--once=false", "--conditions=bad"],
    ["--command=event-create", *Z, "--event-kind=EVENT", *EVENT, "--label=C4",
     "--trigger-type=touch", "--action-type=checkpoint", "--action-ref=checkpoint:ok",
     "--once=false"],
    ["--command=event-list", *Z, "--format=tsv"],
    ["--command=event-validate", *Z, "--format=tsv"],
    ["--command=event-validate", *Z, "--format=json"],
    ["--command=event-inspect", *Z, "--id=2"],
    ["--command=event-update", *Z, "--id=3", "--label=Renamed", "--x=32"],
    ["--command=event-update", *Z, "--id=3"],
    ["--command=event-delete", *Z, "--id=1", "--confirm=true"],
    ["--command=event-delete", *Z, "--id=3", "--confirm=true"],
    ["--command=entity-list", *A, "--format=tsv"],
    ["--command=entity-list", *A, "--preview=true", "--format=json"],
    ["--command=entity-list", *A, "--preview=true", "--format=tsv"],
    ["--command=entity-inspect", *A, "--id=1"],
    ["--command=entity-item-settings", *A, "--id=1", "--format=tsv"],
    ["--command=entity-move", *A, "--id=1", "--x=8", "--y=8"],
    ["--command=entity-delete", *A, "--id=1"],
    ["--command=entity-create", *Z, "--kind=ENEMY", "--x=16", "--y=32", "--label=Worm",
     "--native-type=PSPRITE_ACID_WORM"],
    ["--command=entity-create", *Z, "--kind=ENEMY", "--x=32", "--y=32", "--label=Marker"],
    ["--command=entity-create", *Z, "--kind=ITEM", "--x=16", "--y=32", "--label=Bad",
     "--native-type=PSPRITE_ACID_WORM"],
    ["--command=entity-create", *Z, "--kind=DOOR", "--x=16", "--y=32", "--label=Bad",
     "--native-type=PSPRITE_ACID_WORM"],
    ["--command=entity-create", *Z, "--kind=ENEMY", "--x=16", "--y=32", "--label=Bad",
     "--native-type=bad token"],
    ["--command=entity-create", *Z, "--kind=ENEMY", "--x=17", "--y=32", "--label=Off"],
    ["--command=entity-create", *Z, "--kind=ENEMY", "--x=16", "--y=32", "--label=Bad",
     "--item-id=1"],
    ["--command=entity-update", *Z, "--id=1", "--x=48", "--y=48", "--label=Renamed",
     "--native-type=PSPRITE_ATOMIC"],
    ["--command=entity-update", *Z, "--id=2", "--x=48", "--y=48", "--label=Kept",
     "--native-type=unassigned"],
    ["--command=entity-update", *Z, "--id=9", "--x=48", "--y=48", "--label=Missing",
     "--native-type=unassigned"],
    ["--command=entity-assign", *Z, "--id=2", "--native-type=PSPRITE_BARISTUTE"],
    ["--command=entity-assign", *Z, "--id=2", "--native-type=unassigned", "--dry-run"],
    ["--command=entity-assign", *Z, "--id=2"],
    ["--command=entity-create", *A, "--kind=ITEM", "--x=16", "--y=16", "--label=Potion",
     "--native-type=pickup:02", "--item-id=1", "--parameter-0=0", "--parameter-1=0",
     "--flags=0"],
    ["--command=entity-create", *A, "--kind=ITEM", "--x=16", "--y=16", "--label=Over",
     "--native-type=pickup:02", "--item-id=99", "--parameter-0=0", "--parameter-1=0",
     "--flags=0"],
    ["--command=entity-create", *A, "--kind=ENEMY", "--x=24", "--y=16", "--label=Enemy",
     "--native-type=enemy:5A", "--format=tsv"],
    ["--command=entity-create", *A, "--kind=OBJECT", "--x=24", "--y=16", "--label=Object",
     "--native-type=special-object:26"],
    ["--command=entity-assign", *A, "--id=2", "--native-type=pickup:03", "--item-id=4",
     "--parameter-0=1", "--parameter-1=2", "--flags=3"],
    ["--command=entity-assign", *A, "--id=2", "--native-type=pickup:05", "--item-id=7",
     "--parameter-0=1", "--parameter-1=2", "--flags=3"],
    ["--command=entity-assign", *A, "--id=2", "--native-type=pickup:04"],
    ["--command=entity-update", *A, "--id=3", "--x=32", "--y=24", "--label=Moved",
     "--native-type=enemy:2C"],
    ["--command=entity-update", *A, "--id=3", "--x=32", "--y=24", "--label=Moved",
     "--native-type=pickup:02"],
    ["--command=entity-inspect", *A, "--id=2"],
    ["--command=entity-catalog", "--world=zero_mission", "--kind=OBJECT", "--format=tsv"],
    ["--command=entity-catalog", "--world=aria", "--kind=ENEMY"],
    ["--command=entity-catalog", "--world=aria", "--kind=BOSS"],
    ["--command=list-assets", "--world=aria", "--format=tsv"],
    ["--command=list-assets", "--world=nope"],
    ["--command=entity-list", "--world=zero_mission", "--area=Nope", "--room=1", "--width=16",
     "--height=16"],
    ["--command=entity-list", "--world=zero_mission", "--room=1", "--width=16",
     "--height=16"],
    ["--command=entity-list", "--world=aria", "--area=1", "--room=1", "--width=17",
     "--height=16"],
    ["--command=story-validate", "--kind=cutscene", "--input=story_in.toml"],
    ["--command=story-validate", "--kind=cutscene", "--input=bad.toml"],
    ["--command=story-validate", "--kind=other", "--input=story_in.toml"],
    ["--command=story-validate", "--kind=timeline", "--input=missing.toml"],
    ["--command=story-validate", "--kind=timeline", "--input=data/story/timeline.toml",
     "--format=json"],
    ["--command=story-save", "--kind=cutscene", "--input=story_in.toml",
     "--target=data/cutscenes/copied_scene.toml", "--dry-run=true"],
    ["--command=story-save", "--kind=cutscene", "--input=story_in.toml",
     "--target=data/cutscenes/copied_scene.toml", "--format=json"],
    ["--command=story-save", "--kind=cutscene", "--input=story_in.toml",
     "--target=data/cutscenes/../x.toml"],
    ["--command=story-save", "--kind=timeline", "--input=story_in.toml",
     "--target=data/story/other.toml"],
    ["--command=project-validate", "--format=json"],
    ["--command=room-create", "--world=zero_mission", "--area=0", "--slug=hall", "--name=Hall",
     "--width-screens=2", "--format=json"],
    ["--command=room-create", "--world=zero_mission", "--area=0", "--slug=hall", "--name=Again"],
    ["--command=room-create", "--world=aria", "--area=4", "--slug=tower-1", "--name=Tour Été",
     "--height-screens=3"],
    ["--command=room-create", "--world=aria", "--area=4", "--slug=dry", "--name=Dry",
     "--dry-run=true"],
    ["--command=room-create", "--world=aria", "--area=12", "--slug=x", "--name=X"],
    ["--command=room-create", "--world=aria", "--area=1", "--slug=Bad", "--name=X"],
    ["--command=room-create", "--world=aria", "--area=1", "--slug=ok", "--name= lead"],
    ["--command=room-create", "--world=aria", "--area=1", "--slug=ok", "--name=X",
     "--width-screens=9"],
    ["--command=room-list", "--source=draft", "--format=tsv"],
    ["--command=room-place", "--room=zero_mission:00:hall", "--x=9", "--y=10"],
    ["--command=room-place", "--room=zero_mission:00:hall", "--x=31", "--y=0"],
    ["--command=room-place", "--room=zero_mission:00:hall", "--x=2", "--y=3", "--dry-run=true"],
    ["--command=room-place", "--room=zero_mission:00:hall", "--x=2", "--y=3", "--format=json"],
    ["--command=room-place", "--room=aria:04:tower-1", "--x=30", "--y=18"],
    ["--command=room-place", "--room=aria:04:tower-1", "--x=5", "--y=5"],
    ["--command=room-place", "--room=missing", "--x=5", "--y=5"],
    ["--command=room-place", "--x=5", "--y=5"],
    ["--command=room-move", "--room=zero_mission:00:hall", "--x=4", "--y=3"],
    ["--command=placement-list", "--format=tsv"],
    ["--command=room-unplace", "--room=aria:04:tower-1"],
    ["--command=room-unplace", "--room=aria:04:tower-1", "--confirm=true", "--dry-run=true"],
    ["--command=room-unplace", "--room=aria:04:tower-1", "--confirm=true"],
    ["--command=room-unplace", "--room=aria:04:tower-1", "--confirm=true"],
    ["--command=room-unplace", "--room=nobody", "--confirm=true"],
    ["--command=placement-list", "--format=json"],
]
STAGED = [
    ["--command=collision-set", *Z, "--x=4", "--y=4", "--type=air"],
    ["--command=collision-list", *Z, "--format=json"],
]


def _tree_differences(comparison, prefix=""):
    found = [prefix + name for name in
             comparison.diff_files + comparison.left_only + comparison.right_only]
    for name, sub in comparison.subdirs.items():
        found += _tree_differences(sub, prefix + name + "/")
    return found


@unittest.skipUnless(Path(C).exists(), "native editor CLI not built")
class NativeRoomParityTests(unittest.TestCase):
    def test_project_room_commands_match_the_python_backend(self):
        with tempfile.TemporaryDirectory() as directory:
            python = _fixture(Path(directory) / "python")
            native = _fixture(Path(directory) / "native")
            for arguments in SEQUENCE:
                with self.subTest(arguments=arguments[:2]):
                    self.assertEqual(_run(arguments, native, False),
                                     _run(arguments, python, True))
            for token in ("12345678-1234-1234-1234-123456789abc", "bad-token"):
                for arguments in STAGED:
                    with self.subTest(token=token, arguments=arguments[:1]):
                        extra = {"MV_EDITOR_ROOM_STAGE": token}
                        self.assertEqual(_run(arguments, native, False, extra),
                                         _run(arguments, python, True, extra))
            self.assertEqual(_tree_differences(filecmp.dircmp(python, native)), [])


if __name__ == "__main__":
    unittest.main()
