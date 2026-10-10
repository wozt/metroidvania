# SPDX-License-Identifier: GPL-3.0-only
"""Ensure unrelated duplicate ELF locals do not block native Samus export."""
import unittest
from scripts.mzm_samus_compositions_0160 import parse_sized_symbols


class SamusSymbolFilter0161(unittest.TestCase):
    def test_unrelated_duplicate_local_symbols_ignored(self):
        raw = ("08001000 00000004 t _fpadd_parts\n"
               "08002000 00000008 t _fpadd_parts\n"
               "08248034 000000a0 R sSamusAnim_PowerSuit_Right_Running\n"
               "082342b0 00000050 R sArmCannonAnim_Suit_Right_DiagonalUp_Running\n"
               "08233000 00000040 R sArmCannonGfx_Upper_DiagonalUp_Right_Default\n")
        parsed = parse_sized_symbols(raw)
        self.assertNotIn("_fpadd_parts", parsed)
        self.assertEqual(len(parsed), 3)

    def test_conflicting_target_symbol_still_rejected(self):
        raw = ("08248034 000000a0 R sSamusAnim_PowerSuit_Right_Running\n"
               "08248044 000000a0 R sSamusAnim_PowerSuit_Right_Running\n")
        with self.assertRaises(ValueError):
            parse_sized_symbols(raw)
