# SPDX-License-Identifier: GPL-3.0-only
"""Regression digests of the native Aria importer on the local ROM.

The fixture records the SHA-256 of every inspection command's complete
standard output, the native world-catalog files and every file in the room
export. It was recorded after those outputs had been proven identical to the
former Python room exporter and reference decoders. The fixture contains only
digests, paths and summaries, never ROM-derived bytes.

Regenerate after an intended format change with
``AOS_GOLDEN_UPDATE=1 python3 -m unittest tests.test_fusion_assets_aos_golden``
and review the diff.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/fusion_importer"
ROM = ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba"
FIXTURE = ROOT / "tests/fixtures/aos_native_golden.tsv"
SCHEMA = "metroidvania-aos-native-golden-v1"
COMMANDS = (
    "--aria-directory",
    "--aria-map",
    "--aria-rooms",
    "--aria-transitions",
    "--aria-backgrounds",
    "--aria-background-tiles",
    "--aria-background-renders",
    "--aria-background-composites",
    "--aria-collisions",
    "--aria-runtime-rooms",
    "--aria-loads",
    "--aria-video-memory",
    "--aria-entities",
    "--aria-bosses",
    "--aria-summary",
)


def _run(*arguments: str) -> bytes:
    result = subprocess.run([str(BINARY), *arguments], cwd=ROOT,
                            capture_output=True, check=False)
    if result.returncode:
        raise AssertionError(f"{arguments[0]} failed: {result.stderr.decode()}")
    return result.stdout


def _digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _current() -> list[str]:
    lines = [f"schema\t{SCHEMA}"]
    for command in COMMANDS:
        output = _run(command, str(ROM))
        summary = output.decode("utf-8").rstrip("\n").split("\n")[-1]
        lines.append(f"command\t{command}\t{hashlib.sha256(output).hexdigest()}\t"
                     + summary.replace("\t", " "))
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        world = root / "world.json"
        rooms = root / "rooms.tsv"
        console = _run("--aria-catalog", str(ROM), str(world), str(rooms))
        normalized = console.replace(str(root).encode(), b"<OUTPUT>")
        lines.append(f"catalog-console\tstdout\t"
                     f"{hashlib.sha256(normalized).hexdigest()}")
        lines.append(f"catalog-file\tworld.json\t{_digest(world)}")
        lines.append(f"catalog-file\trooms.tsv\t{_digest(rooms)}")

        output = root / "runtime"
        console = _run("--aria-export-rooms", str(ROM), str(output))
        lines.append(f"export-console\tstdout\t{hashlib.sha256(console).hexdigest()}")
        for path in sorted(output.rglob("*")):
            if path.is_file():
                lines.append(f"export-file\t{path.relative_to(output).as_posix()}\t"
                             f"{_digest(path)}")
    return lines


@unittest.skipUnless(BINARY.exists() and ROM.exists(),
                     "native importer or local Aria ROM is unavailable")
class NativeGoldenTests(unittest.TestCase):
    def test_native_outputs_match_the_recorded_digests(self):
        current = _current()
        if os.environ.get("AOS_GOLDEN_UPDATE"):
            FIXTURE.write_text("\n".join(current) + "\n", encoding="utf-8")
        expected = FIXTURE.read_text(encoding="utf-8").splitlines()
        self.assertEqual(current[0], expected[0])
        recorded = {tuple(line.split("\t")[:2]): line for line in expected[1:]}
        produced = {tuple(line.split("\t")[:2]): line for line in current[1:]}
        self.assertEqual(sorted(produced), sorted(recorded))
        for key, line in produced.items():
            self.assertEqual(line, recorded[key], key)


if __name__ == "__main__":
    unittest.main()
