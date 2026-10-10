# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free checks for ELF-size based Samus animation extraction."""
import unittest
from scripts.mzm_samus_export_0150 import parse_nm_sizes, frame_metadata


class SamusExport0150Tests(unittest.TestCase):
    def test_accepts_exact_sized_symbols(self):
        entries = parse_nm_sizes(
            "08248744 00000040 R sSamusAnim_PowerSuit_Right_Standing\n"
            "08248034 000000a0 R sSamusAnim_PowerSuit_Right_Running\n")
        self.assertEqual(entries["sSamusAnim_PowerSuit_Right_Standing"], (0x08248744, 64))
        self.assertEqual(entries["sSamusAnim_PowerSuit_Right_Running"][1] // 16, 10)

    def test_rejects_bad_size(self):
        self.assertFalse(parse_nm_sizes(
            "08248744 00000031 R sSamusAnim_PowerSuit_Right_Standing"))

    def test_rejects_unaligned_frame_array(self):
        with self.assertRaises(ValueError):
            frame_metadata(b"", 0x08248744, 31)
