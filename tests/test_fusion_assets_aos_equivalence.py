# SPDX-License-Identifier: GPL-3.0-only
"""Compare the native Aria area directory with the Python reference."""
from __future__ import annotations

from pathlib import Path
import subprocess
import unittest

from scripts.import_aos_world import decode_area_directory, decode_map_cells


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/fusion_importer"
ROM = ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba"


@unittest.skipUnless(BINARY.exists() and ROM.exists(),
                     "native importer or local Aria ROM is unavailable")
class FusionAssetsAosEquivalenceTests(unittest.TestCase):
    def test_area_room_counts_match_python_reference(self):
        _, expected_counts = decode_area_directory(ROM.read_bytes())
        result = subprocess.run(
            [str(BINARY), "--aria-directory", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        fields = dict(line.split("=", 1) for line in result.stdout.splitlines())
        actual_counts = [int(fields[f"area_{area:02d}"])
                         for area in range(len(expected_counts))]
        self.assertEqual(actual_counts, expected_counts)
        self.assertEqual(int(fields["total_rooms"]), sum(expected_counts))

    def test_global_map_cells_match_python_reference(self):
        rom = ROM.read_bytes()
        area_pointers, room_counts = decode_area_directory(rom)
        expected = decode_map_cells(rom, area_pointers, room_counts)
        result = subprocess.run(
            [str(BINARY), "--aria-map", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            kind, x, y, area, room, save, warp, pointer = line.split("\t")
            self.assertEqual(kind, "cell")
            actual.append((int(x), int(y), int(area), int(room),
                           int(save), int(warp), int(pointer, 16)))
        expected_values = [
            (cell["map_x"], cell["map_y"], cell["engine_area"], cell["room"],
             int(cell["save"]), int(cell["warp"]),
             int(cell["room_pointer"], 16))
            for cell in expected
        ]
        self.assertEqual(actual, expected_values)
        kind, count, saves, warps = lines[-1].split("\t")
        self.assertEqual(kind, "summary")
        self.assertEqual(int(count), len(expected))
        self.assertEqual(int(saves), sum(cell["save"] for cell in expected))
        self.assertEqual(int(warps), sum(cell["warp"] for cell in expected))
