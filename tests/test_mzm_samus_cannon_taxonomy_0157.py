# SPDX-License-Identifier: GPL-3.0-only
"""Independent tests for conservative cannon resource classification."""
import unittest
from scripts.mzm_samus_cannon_taxonomy_0157 import classify, generate


class CannonTaxonomy0157(unittest.TestCase):
    def test_diagonal_and_armed(self):
        result = classify('sArmCannonGfx_Upper_DiagonalUp_Left_Armed_Default')
        self.assertEqual(result['direction'], 'DiagonalUp')
        self.assertEqual(result['facing'], 'Left')
        self.assertTrue(result['armed'])
        self.assertEqual(result['part'], 'upper')

    def test_does_not_invent_animation_links(self):
        prev = {'schema': 'metroidvania-mzm-samus-cannon-v1',
                'verified_compositions': {'idle': {'frames': [{}, {}]}}}
        result = generate({
            'sArmCannonGfx_Upper_Forward_Right_Default':
                {'address': '0x082337ec', 'size': 64}}, prev)
        self.assertEqual(result['body_animation_links'], {})
        self.assertEqual(result['verified_sequences']['idle']['frame_count'], 2)

    def test_bad_manifest_rejected(self):
        with self.assertRaises(ValueError):
            generate({}, {'schema': 'incorrect'})
