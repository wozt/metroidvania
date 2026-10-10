# SPDX-License-Identifier: GPL-3.0-only
import unittest

from scripts import mzm_samus_compositions_0160 as core
from scripts.mzm_samus_compositions_0166 import SEQUENCES
from scripts.mzm_samus_compositions_0164 import ADDITIONAL, extend_sequences


class LeftCompositionTests(unittest.TestCase):
    def test_unique_sequences(self):
        merged = extend_sequences(extend_sequences(core.SEQUENCES, ADDITIONAL), SEQUENCES)
        self.assertEqual(len(merged), len(core.SEQUENCES) + len(ADDITIONAL) + len(SEQUENCES))

    def test_native_symbol_families(self):
        for name, symbols in SEQUENCES.items():
            with self.subTest(name=name):
                self.assertEqual(len(symbols), 4)
                self.assertTrue(symbols[0].startswith("sSamusAnim_PowerSuit_Left_"))
                self.assertTrue(symbols[1].startswith("sArmCannonAnim_Suit_Left_"))
                self.assertTrue(all(x.startswith("sArmCannonGfx_") for x in symbols[2:]))

    def test_duplicate_rejected(self):
        with self.assertRaises(ValueError):
            extend_sequences(core.SEQUENCES, {"midair_forward_right": ("a",)})
