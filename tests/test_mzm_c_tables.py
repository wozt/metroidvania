# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests of the decompilation table evaluator."""
from pathlib import Path
import unittest

from scripts import mzm_c_tables

DECOMP = Path(__file__).resolve().parents[1] / "third_party/mzm"


class EvaluateTests(unittest.TestCase):
    def test_expressions_and_single_precision_seconds(self):
        names = {"A": 2, "B": 8}
        self.assertEqual(mzm_c_tables.evaluate("A | B", names), 10)
        self.assertEqual(mzm_c_tables.evaluate("1 << A", names), 4)
        # 0.35f is 0.34999999; its single-precision product by 60 rounds to
        # 21.0f, so (s32) gives 21 as on the GBA.
        self.assertEqual(mzm_c_tables.evaluate("CONVERT_SECONDS(.35f)", names), 21)
        self.assertEqual(mzm_c_tables.evaluate("CONVERT_SECONDS(.05f)", names), 3)
        self.assertEqual(mzm_c_tables.evaluate("CONVERT_SECONDS(1.f / 15)", names), 4)
        self.assertEqual(mzm_c_tables.evaluate("UCHAR_MAX", names), 255)


if __name__ == "__main__":
    unittest.main()
