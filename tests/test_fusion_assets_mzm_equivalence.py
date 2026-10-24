# SPDX-License-Identifier: GPL-3.0-only
"""Compare the native Zero Mission ROM-symbol reader with Python."""
from __future__ import annotations

from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.mzm_c_tables import enum_values, evaluate
from scripts.mzm_rom_bank import elf_symbols, produce, symbol_lines
from scripts.mzm_room_render import source_rooms


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/fusion_importer"
ROM = ROOT / "roms/Metroid - Zero Mission (USA).gba"
ELF = ROOT / "third_party/mzm/mzm_us.elf"


@unittest.skipUnless(BINARY.exists() and ROM.exists() and ELF.exists(),
                     "native importer, local Zero Mission ROM or ELF is unavailable")
class FusionAssetsMzmEquivalenceTests(unittest.TestCase):
    def test_rom_symbols_match_python_reference(self):
        symbols = elf_symbols(ELF.read_bytes())
        expected, duplicates = symbol_lines(symbols, ROM.stat().st_size)
        result = subprocess.run(
            [str(BINARY), "--mzm-symbols", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(lines[:-1], expected)
        self.assertEqual(lines[-1],
                         f"summary\t{len(expected)}\t{len(duplicates)}")

    def test_serialized_bank_and_manifest_match_python_byte_for_byte(self):
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            python_root = temporary / "python"
            native_output = temporary / "native"
            python_root.mkdir()
            native_output.mkdir()
            result = produce(python_root, ROM, ELF.parent)
            native = subprocess.run(
                [str(BINARY), "--mzm-rom-bank", str(ROM), str(ELF),
                 str(native_output)],
                cwd=ROOT, text=True, capture_output=True, check=False,
            )
            self.assertEqual(native.returncode, 0, native.stderr)
            for name in ("rom_bank.bin", "rom_symbols.tsv"):
                self.assertEqual((native_output / name).read_bytes(),
                                 (result["folder"] / name).read_bytes())
                self.assertFalse((native_output / f"{name}.tmp").exists())

    def test_room_entries_match_all_source_descriptors(self):
        symbol_rows, _ = symbol_lines(elf_symbols(ELF.read_bytes()),
                                      ROM.stat().st_size)
        symbols = {parts[1]: int(parts[2])
                   for row in symbol_rows
                   if (parts := row.split("\t"))[0] == "symbol"}
        names = enum_values()
        scalar_fields = (
            "tileset", "bg0Prop", "bg1Prop", "bg2Prop", "bg3Prop",
        )
        tail_fields = (
            "bg3Scrolling", "transparency", "defaultSpriteset",
            "firstSpritesetEvent", "firstSpriteset", "secondSpritesetEvent",
            "secondSpriteset", "mapX", "mapY", "effect", "effectY",
            "musicTrack",
        )
        pointer_fields = (
            "pBg0Data", "pBg1Data", "pBg2Data", "pClipData", "pBg3Data",
            "pDefaultSpriteData", "pFirstSpriteData", "pSecondSpriteData",
        )
        expected = []
        for room in source_rooms():
            fields = room["fields"]
            table = symbols[f"s{room['area']}RoomEntries"]
            scalars = [evaluate(fields.get(field, "0"), names)
                       for field in scalar_fields]
            pointers = [symbols[fields[field]] for field in pointer_fields]
            tails = [evaluate(fields.get(field, "0"), names)
                     for field in tail_fields]
            values = [table + room["index"] * 60, *scalars,
                      *pointers[:5], *tails[:2], pointers[5],
                      tails[2], tails[3], pointers[6], tails[4], tails[5],
                      pointers[7], *tails[6:]]
            expected.append((room["area"], room["index"], values))

        result = subprocess.run(
            [str(BINARY), "--mzm-rooms", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        area_rows = []
        for line in lines:
            parts = line.split("\t")
            if parts[0] == "area":
                area_rows.append((parts[2], int(parts[3]), int(parts[4])))
            elif parts[0] == "room":
                actual.append((parts[1], int(parts[2]),
                               [int(value) for value in parts[3:]]))
        self.assertEqual(actual, expected)
        grouped = []
        for area, _, _ in expected:
            if not grouped or grouped[-1] != area:
                grouped.append(area)
        expected_areas = []
        for area in grouped:
            rooms = [room for room in expected if room[0] == area]
            expected_areas.append((area, len(rooms),
                                   symbols[f"s{area}RoomEntries"]))
        self.assertEqual(area_rows, expected_areas)
        self.assertEqual(lines[-1], f"summary\t{len(expected)}")
