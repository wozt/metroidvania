# SPDX-License-Identifier: GPL-3.0-only
import unittest

from scripts import mzm_samus_compositions_0160 as base
from scripts.mzm_samus_compositions_0164 import ADDITIONAL, extend_sequences


class SamusCompositions0164Tests(unittest.TestCase):
    def test_extension_keeps_original_sequences(self):
        merged = extend_sequences(base.SEQUENCES, ADDITIONAL)
        self.assertEqual(len(merged), len(base.SEQUENCES) + len(ADDITIONAL))
        self.assertEqual({key: merged[key] for key in base.SEQUENCES}, base.SEQUENCES)

    def test_duplicate_key_refused(self):
        with self.assertRaises(ValueError):
            extend_sequences(base.SEQUENCES, {"midair_forward_right": ("x",)})

    def test_sequences_have_four_symbols(self):
        for key, symbols in ADDITIONAL.items():
            with self.subTest(key=key):
                self.assertEqual(len(symbols), 4)
                self.assertTrue(symbols[0].startswith("sSamusAnim_PowerSuit_"))
                self.assertTrue(symbols[1].startswith("sArmCannonAnim_Suit_"))
