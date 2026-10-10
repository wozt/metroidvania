# SPDX-License-Identifier: GPL-3.0-only
"""Check symbol-driven native Samus suit palette selection without a ROM."""
import unittest
from scripts.mzm_samus_export_0150 import (PALETTE_SYMBOLS,
    PALETTE_EXPECTED_ADDRESSES, resolve_suit_palettes, suit_group)

class AutoPalette0155Tests(unittest.TestCase):
    def setUp(self):
        self.nm = "\n".join(
            f"{PALETTE_EXPECTED_ADDRESSES[suit]:08x} R {symbol}"
            for suit, symbol in PALETTE_SYMBOLS.items())
        self.rom = bytes(0x400000)

    def test_all_verified_palettes(self):
        offsets = resolve_suit_palettes(self.nm, self.rom)
        self.assertEqual(len(offsets), 5)
        self.assertEqual(offsets["FullSuit"], 0x237FA8)
        self.assertEqual(offsets["Suitless"], 0x2387E8)

    def test_missing_symbol_rejected(self):
        with self.assertRaisesRegex(ValueError, "missing palette"):
            resolve_suit_palettes(self.nm.splitlines()[1] + "\n", self.rom)

    def test_mismatched_elf_rejected(self):
        with self.assertRaisesRegex(ValueError, "unexpected ROM location"):
            resolve_suit_palettes(self.nm.replace("08237fa8", "08237fb8"), self.rom)

    def test_all_suit_routing(self):
        for suit in PALETTE_SYMBOLS:
            self.assertEqual(suit_group(f"sSamusAnim_{suit}_Left_Running"), suit)
