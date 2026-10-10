# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_runtime_sidecars_0167 import COUNTS

class Runtime0167Tests(unittest.TestCase):
    def test_left_counts(self):
        self.assertEqual(len(COUNTS), 6)
        self.assertEqual(sum(COUNTS.values()), 29)
