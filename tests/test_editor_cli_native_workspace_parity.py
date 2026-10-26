"""The native workroom commands equal the Python backend, read-only.

Native workrooms live in the repository's private extracted assets, so the
cases only read them or run as dry runs; nothing under assets/ is modified.
"""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
C = str(ROOT / "build/fusion_editor_cli")
MZM = ROOT / "assets/extracted/rooms/metroid/workrooms/brinstar_004.mvnative"
ARIA = ROOT / "assets/extracted/rooms/aria/workrooms/area_00_room_001.mvnative"


def _run(args, python, root=ROOT):
    env = dict(os.environ)
    if python:
        env["FUSION_EDITOR_PYTHON_BACKEND"] = "1"
    result = subprocess.run([C, *args, f"--root={root}"], capture_output=True, text=True,
                            env=env, cwd=ROOT)
    return result.returncode, result.stdout, result.stderr


Z = ["--world=zero_mission", "--area=Brinstar", "--room=4"]
O = ["--world=zero_mission", "--area=0", "--room=7"]
A = ["--world=aria", "--area=0", "--room=1"]
CASES = [
    ["--command=layer-list", *Z], ["--command=layer-list", *Z, "--format=json"],
    ["--command=layer-list", *A, "--format=tsv"], ["--command=layer-list", *O],
    ["--command=tile-get", *O, "--layer=bg1", "--x=2", "--y=2"],
    ["--command=layer-list", "--world=aria", "--area=1", "--room=998"],
    ["--command=layer-list", "--world=moon", "--area=1", "--room=3"],
    ["--command=tile-get", *Z, "--layer=bg1", "--x=0", "--y=0"],
    ["--command=tile-get", *Z, "--layer=bg2", "--x=3", "--y=2", "--format=json"],
    ["--command=tile-get", *A, "--layer=bg1", "--x=5", "--y=5"],
    ["--command=tile-get", *Z, "--layer=bg3", "--x=0", "--y=0"],
    ["--command=tile-get", *Z, "--layer=bg1", "--x=254", "--y=0"],
    ["--command=tile-get", *Z, "--layer=bg1", "--x=255", "--y=0"],
    ["--command=tile-set", *Z, "--layer=bg1", "--x=1", "--y=1", "--tile-id=0", "--dry-run"],
    ["--command=tile-set", *Z, "--layer=bg1", "--x=1", "--y=1", "--tile-id=1023",
     "--dry-run"],
    ["--command=tile-set", *Z, "--layer=bg1", "--x=1", "--y=1", "--tile-id=1024",
     "--dry-run"],
    ["--command=tile-fill", *A, "--layer=bg2", "--x=2", "--y=2", "--tile-id=1", "--dry-run"],
    ["--command=tile-fill", *A, "--layer=bg2", "--x=2", "--y=2", "--dry-run"],
    ["--command=room-render", *Z, "--dry-run", "--format=json"],
    ["--command=room-render", "--world=zero_mission", "--area=Brinstar", "--room=0",
     "--dry-run"],
    ["--command=room-render", "--world=zero_mission", "--area=Kraid", "--room=999",
     "--dry-run"],
    ["--command=room-render", "--world=zero_mission", "--room=4", "--dry-run"],
    ["--command=room-open", *Z, "--dry-run"],
    ["--command=room-open", *Z, "--dry-run", "--format=json"],
    ["--command=room-open", "--world=zero_mission", "--area=Kraid", "--room=999",
     "--dry-run"],
]


@unittest.skipUnless(Path(C).is_file() and MZM.is_file() and ARIA.is_file(),
                     "needs the native build and the private extracted workrooms")
class NativeWorkspaceParity(unittest.TestCase):
    def test_commands_match_python(self):
        for arguments in CASES:
            with self.subTest(arguments=arguments):
                self.assertEqual(_run(arguments, False), _run(arguments, True))

    def test_other_roots_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            arguments = ["--command=layer-list", *Z]
            native = _run(arguments, False, directory)
            self.assertEqual(native, _run(arguments, True, directory))
            self.assertIn("require the repository root", native[2])
            arguments = ["--command=room-open", *Z]
            native = _run(arguments, False, directory)
            self.assertEqual(native, _run(arguments, True, directory))
            self.assertIn("require the repository root", native[2])


if __name__ == "__main__":
    unittest.main()
