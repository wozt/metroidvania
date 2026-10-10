# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free checks of cannon symbol boundaries and private output paths."""
import tempfile
import unittest
from pathlib import Path
from scripts.mzm_samus_cannon_0156 import parse_cannon_symbols, validate_private_output


class Cannon0156Tests(unittest.TestCase):
    def test_symbol_records(self):
        result = parse_cannon_symbols(
            '082337ec 00000040 R sArmCannonGfx_Upper_Forward_Left_Default\n'
            '082338ac 00000040 R sArmCannonGfx_Lower_Forward_Left_Default',
            0x800000)
        self.assertEqual(len(result), 2)
        self.assertEqual(result['sArmCannonGfx_Upper_Forward_Left_Default']['size'], 64)

    def test_rejects_overflow(self):
        with self.assertRaises(ValueError):
            parse_cannon_symbols('087ffff0 00000040 R sArmCannonGfx_Test', 0x800000)

    def test_private_output_path(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'assets/extracted').mkdir(parents=True)
            validate_private_output(root, root / 'assets/extracted/cannon')
            with self.assertRaises(ValueError):
                validate_private_output(root, root / 'other/cannon')
