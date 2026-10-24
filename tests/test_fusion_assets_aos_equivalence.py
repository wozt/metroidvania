# SPDX-License-Identifier: GPL-3.0-only
"""Compare the native Aria area directory with the Python reference."""
from __future__ import annotations

from pathlib import Path
import subprocess
import unittest

from scripts.import_aos_world import decode_area_directory


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
