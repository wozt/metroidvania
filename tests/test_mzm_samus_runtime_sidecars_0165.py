# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_runtime_sidecars_0165 import NAMES, COUNTS, validate


class ExtensionTests(unittest.TestCase):
    def test_five_groups(self):
        self.assertEqual(len(NAMES), 5)
        self.assertEqual(COUNTS, (10, 10, 3, 5, 3))

    def test_wrong_manifest_rejected(self):
        with self.assertRaises(ValueError):
            validate({"schema": "unknown"}, None)

    def test_missing_sequence_rejected(self):
        with self.assertRaises(ValueError):
            validate({"schema": "metroidvania-mzm-samus-compositions-extension-v1",
                      "sequences": {}}, None)
