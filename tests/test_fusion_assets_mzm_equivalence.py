# SPDX-License-Identifier: GPL-3.0-only
"""Compare the native Zero Mission ROM-symbol reader with Python."""
from __future__ import annotations

from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.mzm_rom_bank import elf_symbols, produce, symbol_lines


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
