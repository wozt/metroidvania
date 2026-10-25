# SPDX-License-Identifier: GPL-3.0-only
"""Regression digests of the native Zero Mission importer on the local ROM.

The fixture records, for each inspection command, the SHA-256 of its whole
standard output and its summary line, the ROM bank and symbol manifest, every
file of the Samus and projectile sprite libraries, the enemy sprite index,
bounds and tables with the content-addressed object names, and every file the
native room export writes. They were recorded once the native
outputs had been proven byte-identical to the former Python exporters, which
this file replaces as the reference. The fixture holds digests and counts
only, no ROM-derived bytes.

Regenerate after an intended format change with
``MZM_GOLDEN_UPDATE=1 python3 -m unittest tests.test_fusion_assets_mzm_golden``
and review the diff.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/fusion_importer"
ROM = ROOT / "roms/Metroid - Zero Mission (USA).gba"
ELF = ROOT / "third_party/mzm/mzm_us.elf"
FIXTURE = ROOT / "tests/fixtures/mzm_native_golden.tsv"
SCHEMA = "metroidvania-mzm-native-golden-v1"
COMMANDS = (
    "--mzm-symbols", "--mzm-rooms", "--mzm-clipdata",
    "--mzm-runtime-room-cores", "--mzm-runtime-room-doors",
    "--mzm-runtime-room-hatches", "--mzm-runtime-room-metadata",
    "--mzm-runtime-room-sprites", "--mzm-backgrounds", "--mzm-tilesets",
    "--mzm-metatiles", "--mzm-background-renders",
    "--mzm-background-composites", "--mzm-bitmaps", "--mzm-runtime-tables",
    "--mzm-export-backgrounds",
)


def _run(*arguments: str) -> bytes:
    result = subprocess.run([str(BINARY), *arguments], cwd=ROOT,
                            capture_output=True, check=False)
    if result.returncode:
        raise AssertionError(f"{arguments[0]} failed: {result.stderr.decode()}")
    return result.stdout


def _current() -> list[str]:
    lines = [f"schema\t{SCHEMA}"]
    for command in COMMANDS:
        output = _run(command, str(ROM), str(ELF))
        text = output.decode("utf-8").rstrip("\n").split("\n")
        lines.append(f"command\t{command}\t{hashlib.sha256(output).hexdigest()}\t"
                     + text[-1].replace("\t", " "))
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        bank = root / "rom"
        bank.mkdir()
        console = _run("--mzm-rom-bank", str(ROM), str(ELF), str(bank))
        normalized = console.replace(str(root).encode(), b"<OUTPUT>")
        lines.append(f"bank-console\tstdout\t{hashlib.sha256(normalized).hexdigest()}")
        for path in sorted(bank.iterdir()):
            if path.is_file():
                lines.append(f"bank-file\t{path.name}\t"
                             f"{hashlib.sha256(path.read_bytes()).hexdigest()}")

        samus, projectiles = root / "samus", root / "projectiles"
        console = _run("--mzm-export-samus", str(ROM), str(ELF), str(samus),
                       str(projectiles))
        lines.append(f"samus-console\tstdout\t{hashlib.sha256(console).hexdigest()}")
        for library in (samus, projectiles):
            for path in sorted(library.rglob("*")):
                if path.is_file():
                    lines.append(f"{library.name}-file\t"
                                 f"{path.relative_to(library).as_posix()}\t"
                                 f"{hashlib.sha256(path.read_bytes()).hexdigest()}")

        sprites, metadata = root / "sprites", root / "metadata"
        console = _run("--mzm-export-sprites", str(ROM), str(ELF), str(sprites),
                       str(metadata))
        lines.append(f"sprites-console\tstdout\t{hashlib.sha256(console).hexdigest()}")
        for path in (metadata / "sprites.tsv", sprites / "runtime_index.tsv",
                     sprites / "oam_bounds.tsv"):
            lines.append(f"sprites-file\t{path.name}\t"
                         f"{hashlib.sha256(path.read_bytes()).hexdigest()}")
        # The objects are content-addressed: each name must be its digest,
        # and the sorted names are recorded once.
        objects = sorted((sprites / "objects").iterdir())
        for path in objects:
            if hashlib.sha256(path.read_bytes()).hexdigest() + ".bmp" != path.name:
                raise AssertionError(f"object digest mismatch: {path.name}")
        names = "\n".join(path.name for path in objects).encode()
        lines.append(f"sprites-objects\t{len(objects)}\t{hashlib.sha256(names).hexdigest()}")

        output = root / "runtime"
        console = _run("--mzm-export-rooms", str(ROM), str(ELF), str(output))
        lines.append(f"console\t{hashlib.sha256(console).hexdigest()}")
        for path in sorted(output.rglob("*")):
            if path.is_file():
                lines.append(f"file\t{path.relative_to(output).as_posix()}\t"
                             f"{hashlib.sha256(path.read_bytes()).hexdigest()}")
    return lines


@unittest.skipUnless(BINARY.exists() and ROM.exists() and ELF.exists(),
                     "native importer, local Zero Mission ROM or ELF is unavailable")
class NativeGoldenTests(unittest.TestCase):
    def test_native_outputs_match_the_recorded_digests(self):
        current = _current()
        if os.environ.get("MZM_GOLDEN_UPDATE"):
            FIXTURE.write_text("\n".join(current) + "\n", encoding="utf-8")
        expected = FIXTURE.read_text(encoding="utf-8").splitlines()
        self.assertEqual(current[0], expected[0])
        recorded = {tuple(line.split("\t")[:2]): line for line in expected[1:]}
        produced = {tuple(line.split("\t")[:2]): line for line in current[1:]}
        self.assertEqual(sorted(produced), sorted(recorded))
        for key, line in produced.items():
            self.assertEqual(line, recorded[key], key)

    @unittest.skipIf(sys.platform == "win32", "POSIX symlinks")
    def test_room_export_refuses_symlinks_and_files(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "target"
            target.mkdir()
            link = Path(directory) / "link"
            link.symlink_to(target, target_is_directory=True)
            blocked = Path(directory) / "file"
            blocked.write_text("x", encoding="utf-8")
            for output in (link, blocked):
                result = subprocess.run(
                    [str(BINARY), "--mzm-export-rooms", str(ROM), str(ELF),
                     str(output)], cwd=ROOT, text=True, capture_output=True,
                     check=False)
                self.assertNotEqual(result.returncode, 0)
            self.assertFalse(list(target.iterdir()))


if __name__ == "__main__":
    unittest.main()
