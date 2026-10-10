# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free MZM animation table inventory tests."""
import unittest
from scripts.mzm_samus_catalog import collect


class SamusCatalogTests0148(unittest.TestCase):
    def test_pose_and_aim_selectors_remain_separate(self):
        constants = """MAKE_ENUM(u8, SamusPose) {
    SPOSE_RUNNING,
    SPOSE_MIDAIR,
    SPOSE_MORPH_BALL,
    SPOSE_COUNT,
    SPOSE_NONE = UCHAR_MAX
};"""
        pointers = """const struct SamusAnimationData* const sSamusAnimPointers_PowerSuit[SPOSE_COUNT][2] = {
[SPOSE_RUNNING] = {sSamusAnim_PowerSuit_Right_Running, sSamusAnim_PowerSuit_Left_Running},
[SPOSE_MIDAIR] = {sSamusAnim_PowerSuit_Right_MidAir, sSamusAnim_PowerSuit_Left_MidAir}
};
const struct SamusAnimationData* const sSamusAnimPointers_PowerSuit_Running[4][2] = {
[ACD_FORWARD] = {sSamusAnim_PowerSuit_Right_Forward_Running, sSamusAnim_PowerSuit_Left_Forward_Running},
[ACD_DIAGONALLY_UP] = {sSamusAnim_PowerSuit_Right_DiagonalUp_Running, sSamusAnim_PowerSuit_Left_DiagonalUp_Running}
};"""
        catalog = collect(pointers, constants)
        self.assertEqual(catalog["pose_count"], 3)
        self.assertEqual(catalog["table_count"], 2)
        self.assertEqual(catalog["variant_count"], 4)
        self.assertIn("SPOSE_MORPH_BALL", catalog["poses"])
        row = catalog["tables"]["sSamusAnimPointers_PowerSuit_Running"][1]
        self.assertEqual(row["selector"], "ACD_DIAGONALLY_UP")
        self.assertTrue(row["left"].endswith("DiagonalUp_Running"))

    def test_missing_tables_rejected(self):
        with self.assertRaises(ValueError):
            collect("", "MAKE_ENUM(u8, SamusPose) {\nSPOSE_RUNNING,\nSPOSE_COUNT\n};")
