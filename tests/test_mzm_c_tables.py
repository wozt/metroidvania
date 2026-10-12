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


@unittest.skipUnless((DECOMP / "src/data/block_data.c").is_file(),
                     "pinned MZM decompilation missing")
class BlockTableTests(unittest.TestCase):
    def test_block_tables(self):
        names = mzm_c_tables.enum_values(DECOMP)
        self.assertEqual(names["CLIP_BEHAVIOR_BLOCK_START"], 16)
        self.assertEqual(names["CAA_BOMB_CHAIN"], 15)
        damage = mzm_c_tables.table("block_data.c", "sClipdataAffectingActionDamageTypes",
                                    DECOMP)
        self.assertEqual(damage[names["CAA_SCREW_ATTACK"]], 1 << 7)
        timers = mzm_c_tables.table("block_data.c", "sBrokenBlocksTimers", DECOMP)
        self.assertEqual(len(timers[names["BLOCK_TYPE_SHOT_BLOCK_REFORM"]]), 13)
        chains = mzm_c_tables.table("block_data.c", "sBombChainReverseData", DECOMP)
        self.assertEqual(chains[4]["behavior"], names["CLIP_BEHAVIOR_HORIZONTAL_BOMB_CHAIN1"])



@unittest.skipUnless((DECOMP / "src/data/haze_data.c").is_file(),
                     "pinned MZM decompilation missing")
class PowerBombWindowTests(unittest.TestCase):
    def test_window_rows(self):
        lines = mzm_c_tables.power_bomb_window_lines(DECOMP)
        self.assertEqual(lines[0], "schema\tmetroidvania-mzm-power-bomb-v1")
        windows = {int(line.split("\t")[1]): [int(v) for v in line.split("\t")[2].split(",")]
                   for line in lines[1:]}
        self.assertEqual(sorted(windows), list(range(161)))
        # (right, left) pairs over 2 * axis + 1 rows; axes below 4 reuse 0.
        for axis, values in windows.items():
            self.assertEqual(len(values), 2 * (2 * max(axis, 4) + 1))
        self.assertEqual(windows[0][:6], [0, 0, 2, -2, 4, -4])


if __name__ == "__main__":
    unittest.main()
