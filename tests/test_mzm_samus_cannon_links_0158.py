# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_samus_cannon_links_0158 import cannon_symbols, candidates, build

class CannonLinks0158(unittest.TestCase):
    def test_exact_suffix(self):
        symbols = {'sArmCannonAnim_Suit_Right_Forward_Running':
                   {'address': '0x08234120', 'frames_in_elf': 10}}
        rows = candidates('sSamusAnim_PowerSuit_Right_Forward_Running', symbols)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['status'], 'candidate-not-composed')

    def test_no_false_pose_alias(self):
        symbols = {'sArmCannonAnim_Suit_Right_Forward_Running':
                   {'address': '0x08234120', 'frames_in_elf': 10}}
        self.assertEqual(candidates('sSamusAnim_PowerSuit_Right_Running', symbols), [])

    def test_nm_sizes(self):
        symbols = cannon_symbols('08234120 00000050 R sArmCannonAnim_Suit_Right_Forward_Running')
        self.assertEqual(symbols['sArmCannonAnim_Suit_Right_Forward_Running']['frames_in_elf'], 10)
