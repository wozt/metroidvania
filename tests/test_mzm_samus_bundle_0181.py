# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_bundle_0181 import SUITS

class SamusBundle0181Tests(unittest.TestCase):
    def test_suits(self):
        self.assertEqual(len(SUITS), 5)
        self.assertEqual(SUITS[0], "PowerSuit")
