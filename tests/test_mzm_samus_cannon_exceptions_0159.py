# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_cannon_exceptions_0159 import classify_missing, build


class CannonExceptions0159(unittest.TestCase):
    def test_running_requires_aim_choice(self):
        symbols = {"sArmCannonAnim_Suit_Right_None_Running":
                   {"address": "0x08234120", "frames_in_elf": 10},
                   "sArmCannonAnim_Suit_Right_DiagonalUp_Running":
                   {"address": "0x08234170", "frames_in_elf": 10}}
        got = classify_missing("sSamusAnim_PowerSuit_Right_Running", 10, symbols)
        self.assertEqual(len(got["candidates"]), 2)
        self.assertEqual(got["status"], "exceptional-candidates")

    def test_hurt_not_assumed_identical(self):
        got = classify_missing("sSamusAnim_FullSuit_Left_GettingKnockedBack", 3,
                               {"sArmCannonAnim_Suit_Left_GettingHurt":
                                {"address": "0x08230000", "frames_in_elf": 3}})
        self.assertIn("not proven", got["reason"])

    def test_suitless_dying_remains_unmapped(self):
        got = classify_missing("sSamusAnim_Suitless_Right_Dying", 25,
                               {"sArmCannonAnim_Suit_Dying":
                                {"address": "0x08230000", "frames_in_elf": 25}})
        self.assertEqual(got["status"], "unmapped-no-native-counterpart")

    def test_build_rejects_wrong_schema(self):
        with self.assertRaises(ValueError):
            build({"schema": "wrong"})
