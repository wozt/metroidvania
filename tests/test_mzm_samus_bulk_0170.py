# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_bulk_0170 import CANDIDATES, collect
from scripts import mzm_samus_compositions_0160 as core


class Bulk0170Tests(unittest.TestCase):
    def test_catalogue_size(self):
        self.assertEqual(len(CANDIDATES), 48)

    def test_four_native_names(self):
        for name, symbols in CANDIDATES.items():
            with self.subTest(name=name):
                self.assertEqual(len(symbols), 4)
                self.assertTrue(symbols[0].startswith("sSamusAnim_PowerSuit_"))
                self.assertTrue(symbols[1].startswith("sArmCannonAnim_Suit_"))

    def test_runtime_unchanged(self):
        self.assertEqual(len(core.SEQUENCES), 3)
