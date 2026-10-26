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
ARIA_ROM = ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba"
DATABASES = (ROOT / "third_party/mzm/database.json", ROOT / "third_party/cvaos/database.json")
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


def _raw_current() -> list[str]:
    """The raw import of both ROMs: console, manifest, room descriptors and
    one digest over every raw block's path and bytes."""
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        console = _run("--raw-import", str(ROM), str(ARIA_ROM), str(ELF),
                       *map(str, DATABASES), str(root))
        lines = [f"raw-console\tstdout\t{hashlib.sha256(console).hexdigest()}"]
        for relative in ("shared/manifests/import_catalog.json", "rooms/metroid/rooms.tsv"):
            lines.append(f"raw-file\t{relative}\t"
                         f"{hashlib.sha256((root / relative).read_bytes()).hexdigest()}")
        blocks = hashlib.sha256()
        files = sorted(path for world in ("metroid", "aria")
                       for path in (root / world / "raw").rglob("*") if path.is_file())
        for path in files:
            blocks.update(path.relative_to(root).as_posix().encode() + b"\0")
            blocks.update(hashlib.sha256(path.read_bytes()).digest())
        lines.append(f"raw-blocks\t{len(files)}\t{blocks.hexdigest()}")
    return lines


def _compare(test: unittest.TestCase, current: list[str], expected: list[str]) -> None:
    recorded = {tuple(line.split("\t")[:2]): line for line in expected}
    produced = {tuple(line.split("\t")[:2]): line for line in current}
    test.assertEqual(sorted(produced), sorted(recorded))
    for key, line in produced.items():
        test.assertEqual(line, recorded[key], key)


def _fixture(raw: bool) -> list[str]:
    lines = FIXTURE.read_text(encoding="utf-8").splitlines()
    return [lines[0]] + [line for line in lines[1:] if line.startswith("raw-") == raw]


def _update(lines: list[str], raw: bool) -> None:
    kept = _fixture(not raw)[1:] if FIXTURE.exists() else []
    header = [f"schema\t{SCHEMA}"]
    body = (lines if raw else lines[1:]) + kept
    FIXTURE.write_text("\n".join(header + sorted(body, key=lambda line: line.startswith(
        "raw-"))) + "\n", encoding="utf-8")


@unittest.skipUnless(BINARY.exists() and ROM.exists() and ELF.exists(),
                     "native importer, local Zero Mission ROM or ELF is unavailable")
class NativeGoldenTests(unittest.TestCase):
    def test_native_outputs_match_the_recorded_digests(self):
        current = _current()
        if os.environ.get("MZM_GOLDEN_UPDATE"):
            _update(current, raw=False)
        expected = _fixture(raw=False)
        self.assertEqual(current[0], expected[0])
        _compare(self, current[1:], expected[1:])

    @unittest.skipUnless(ARIA_ROM.exists() and all(path.exists() for path in DATABASES),
                         "local Aria ROM or decompilation databases unavailable")
    def test_raw_import_matches_the_recorded_digests(self):
        current = _raw_current()
        if os.environ.get("MZM_GOLDEN_UPDATE"):
            _update(current, raw=True)
        _compare(self, current, _fixture(raw=True)[1:])

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
