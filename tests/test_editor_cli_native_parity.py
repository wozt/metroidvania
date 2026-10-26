# SPDX-License-Identifier: GPL-3.0-only
"""Native editor backend parity with the Python backend (transitional)."""
import unittest
import json, os, subprocess, sys, tempfile, shutil
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import authored_rooms
C = str(ROOT / "build/fusion_editor_cli")
def run(args, py):
    env = dict(os.environ)
    if py: env["FUSION_EDITOR_PYTHON_BACKEND"] = "1"
    r = subprocess.run([C, *args], capture_output=True, text=True, env=env, cwd=ROOT)
    return r.returncode, r.stdout, r.stderr
def fixture(base):
    root = Path(base); root.mkdir(parents=True)
    for world, area, slug, name, w, h in [("zero_mission", 0, "alpha", "Alpha room", 2, 1),
                                          ("aria", 3, "beta-2", "Bêta – salle", 1, 3),
                                          ("zero_mission", 5, "gamma_x", "Gamma", 1, 1)]:
        authored_rooms.save_room(authored_rooms.new_room(world=world, area=area, slug=slug, name=name,
                                 width_screens=w, height_screens=h), root)
    (root / "assets/extracted/authored_rooms/placements.json").write_text(json.dumps(
        {"schema": "metroidvania.map-placements", "version": 1, "placements": [
            {"id": "zero_mission:00:alpha", "x": 3, "y": 4}, {"y": 2, "x": 7, "id": "aria:03:beta-2"}]}))
    return root
cases = []
base_cmds = [
    ["--command=capabilities"], ["--command=capabilities", "--format=json"],
    ["--command=project-info", "--format=json"], ["--command=list-worlds"],
    ["--command=list-areas", "--world=aria"], ["--command=list-areas", "--world=bad"],
    ["--command=list-areas"], ["--command=list-areas", "--world=aria", "--format=tsv"],
    ["--command=room-list", "--world=zero_mission", "--format=json"],
    ["--command=room-list", "--world=zero_mission", "--area=brinstar"],
    ["--command=room-list", "--world=zero_mission", "--area=3"],
    ["--command=room-list", "--world=zero_mission", "--area=9"],
    ["--command=room-list", "--world=zero_mission", "--area=Nope"],
    ["--command=room-list", "--world=aria", "--area=2", "--format=json"],
    ["--command=list-rooms", "--world=aria"],
    ["--command=room-list", "--world=aria", "--format=tsv"],
    ["--command=room-list", "--source=draft", "--format=tsv"],
    ["--command=room-list", "--source=draft", "--world=aria"],
    ["--command=room-list", "--source=draft", "--area=05", "--format=json"],
    ["--command=room-list", "--source=draft", "--area=5", "--format=tsv"],
    ["--command=room-list", "--source=bogus", "--world=aria"],
    ["--command=room-inspect", "--world=zero_mission", "--area=Kraid", "--room=4"],
    ["--command=room-inspect", "--world=zero_mission", "--room=4"],
    ["--command=room-inspect", "--world=aria", "--area=0", "--room=7", "--format=json"],
    ["--command=room-inspect", "--source=draft", "--room=aria:03:beta-2"],
    ["--command=room-inspect", "--source=draft", "--room=missing"],
    ["--command=room-validate", "--room=zero_mission:00:alpha"],
    ["--command=room-validate"],
    ["--command=placement-list", "--format=tsv"], ["--command=placement-list", "--world=aria"],
    ["--command=placement-list", "--format=json"],
    ["--command=list-worlds", "--area=1"], ["--command=list-worlds", "extra"],
    ["--wor=aria", "--command=list-areas"], ["--command"], ["--format=x", "--command=list-worlds"],
    ["--command=list-worlds", "--atomic=maybe"], ["--version"], ["--command=list-worlds", "--batch=x"],
    ["--command=list-worlds", "--root=/nonexistent"], ["--command=list-worlds", "--root=../x"],
    ["--command=list-worlds", "--dry-run=true", "--verbose=false", "--format=json"],
    ["--command=room-list", "--world", "aria", "--area", "1", "--format", "tsv"],
    ["--command=room-list", "--world=zero_mission", "--area=-1"],
    ["--command=room-list", "--source=draft", "--area=+1"],
]
def _parity():
  with tempfile.TemporaryDirectory() as tmp:
    good = fixture(Path(tmp) / "good")
    bad1 = fixture(Path(tmp) / "badjson")
    (bad1 / "assets/extracted/authored_rooms/zero_mission/00/broken.json").write_text("{")
    bad2 = fixture(Path(tmp) / "mismatch")
    p = bad2 / "assets/extracted/authored_rooms/aria/03/beta-2.json"
    d = json.loads(p.read_text()); d["name"] = " lead"; p.write_text(json.dumps(d))
    bad3 = fixture(Path(tmp) / "overlap")
    (bad3 / "assets/extracted/authored_rooms/placements.json").write_text(json.dumps(
        {"schema": "metroidvania.map-placements", "version": 1, "placements": [
            {"id": "zero_mission:00:alpha", "x": 3, "y": 4}, {"id": "zero_mission:05:gamma_x", "x": 4, "y": 4},
            {"id": "zero_mission:00:alpha", "x": 9, "y": 9}]}))
    bad4 = fixture(Path(tmp) / "wrongdir")
    shutil.move(str(bad4 / "assets/extracted/authored_rooms/aria/03/beta-2.json"),
                str(bad4 / "assets/extracted/authored_rooms/aria/03/other.json"))
    link = Path(tmp) / "link"; link.symlink_to(good)
    roots = [None, good, bad1, bad2, bad3, bad4, link, Path(tmp) / "empty"]
    (Path(tmp) / "empty").mkdir()
    total = bad = 0
    for root in roots:
        for args in base_cmds:
            full = args + ([f"--root={root}"] if root else [])
            a, b = run(full, True), run(full, False)
            total += 1
            if a != b:
                bad += 1
                if False:
                    print("DIFF", root.name if root else "repo", full[:3])
                    for x, y, n in zip(a, b, ("code", "out", "err")):
                        if x != y: print(f"  {n}: py={str(x)[:300]!r}\n      c ={str(y)[:300]!r}")
    return total, bad


@unittest.skipUnless(Path(C).exists(), "native editor CLI not built")
class NativeParityTests(unittest.TestCase):
    def test_ported_commands_match_the_python_backend(self):
        total, bad = _parity()
        self.assertEqual(bad, 0, f"{bad}/{total} differ")


if __name__ == "__main__":
    unittest.main()
