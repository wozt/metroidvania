# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_special_0171 import CANDIDATES, SPECIAL


class SpecialMotionCatalogueTests(unittest.TestCase):
    def test_candidate_count(self):
        self.assertEqual(len(CANDIDATES), 2 * len(SPECIAL))

    def test_required_spins_in_catalogue(self):
        for motion in ("spinning", "spacejumping", "screwattacking"):
            self.assertIn(motion + "_right", CANDIDATES)
            self.assertIn(motion + "_left", CANDIDATES)

    def test_symbol_families(self):
        for item in CANDIDATES.values():
            self.assertEqual(len(item), 4)
            self.assertTrue(item[0].startswith("sSamusAnim_PowerSuit_"))
            self.assertTrue(item[1].startswith("sArmCannonAnim_Suit_"))
