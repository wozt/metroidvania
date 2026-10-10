# SPDX-License-Identifier: GPL-3.0-only
"""Suit-specific palette routing remains explicit and fail-closed."""
import unittest
from scripts.mzm_samus_export_0150 import suit_group
from scripts.mzm_samus_body import load_palette_banks

class PaletteRouting0154(unittest.TestCase):
    def test_groups(self):
        self.assertEqual(suit_group("sSamusAnim_PowerSuit_Right_Running"), "PowerSuit")
        self.assertEqual(suit_group("sSamusAnim_FullSuit_Left_Crouching"), "FullSuit")
        self.assertEqual(suit_group("sSamusAnim_Suitless_Right_Running"), "Suitless")
        self.assertIsNone(suit_group("sSamusAnim_Unknown"))

    def test_invalid_palette_is_rejected(self):
        with self.assertRaises(ValueError):
            load_palette_banks(bytes(128), 120, 2, 0)
