# SPDX-License-Identifier: GPL-3.0-only
"""Compare the native GBA header extractor with a ROM-free Python fixture."""
from __future__ import annotations

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/fusion_importer"


def synthetic_header() -> bytes:
    rom = bytearray(0xC0)
    rom[0xA0:0xAC] = b"SYNTHETIC TE"
    rom[0xAC:0xB0] = b"ABCD"
    rom[0xB0:0xB2] = b"01"
    rom[0xB2] = 0x96
    rom[0xB3] = 0x01
    rom[0xB4] = 0x02
    rom[0xBC] = 0x03
    rom[0xBD] = (-(sum(rom[0xA0:0xBD]) + 0x19)) & 0xFF
    return bytes(rom)


@unittest.skipUnless(BINARY.exists(), "fusion_importer has not been built")
class FusionImporterHeaderTests(unittest.TestCase):
    def test_native_header_matches_python_fixture(self):
        with tempfile.NamedTemporaryFile() as fixture:
            fixture.write(synthetic_header())
            fixture.flush()
            result = subprocess.run(
                [str(BINARY), "--header", fixture.name],
                cwd=ROOT, text=True, capture_output=True, check=False,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        fields = dict(line.split("=", 1) for line in result.stdout.splitlines())
        self.assertEqual(fields["title"], "SYNTHETIC TE")
        self.assertEqual(fields["game_code"], "ABCD")
        self.assertEqual(fields["maker_code"], "01")
        self.assertEqual(fields["main_unit_code"], "0x01")
        self.assertEqual(fields["device_type"], "0x02")
        self.assertEqual(fields["software_version"], "3")
        self.assertEqual(fields["checksum"], "valid")
