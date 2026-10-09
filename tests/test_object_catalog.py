# SPDX-License-Identifier: GPL-3.0-only
import unittest

from scripts.object_catalog import build_aria, build_mzm, tsv


class ObjectCatalogTests(unittest.TestCase):
    def test_mzm_catalog_covers_primary_sprite_enum_and_stats(self):
        records = build_mzm()
        self.assertGreaterEqual(len(records), 200)
        kraid = next(record for record in records
                     if record['native_type'] == 'PSPRITE_KRAID')
        self.assertIn('health=300', kraid['summary'])
        self.assertFalse(kraid['editable'])

    def test_aria_groups_native_types_and_preserves_boss_identity(self):
        catalog = {'format': 'MV_AOS_WORLD_2', 'rooms': [{
            'entities': [
                {'kind': 1, 'entity_id': 7, 'boss_id': 'boss-a', 'native_health': 120},
                {'kind': 1, 'entity_id': 7, 'boss_id': 'boss-a', 'native_health': 120},
                {'kind': 2, 'entity_id': 3},
            ]
        }]}
        records = build_aria(catalog)
        self.assertEqual(records[0]['placements'], 2)
        self.assertEqual(records[0]['name'], 'boss-a')
        self.assertIn('health=120', records[0]['summary'])
        self.assertIn('Aria of Sorrow\t7\tkind-1:id-7', tsv(records))

    def test_tsv_rejects_control_characters(self):
        with self.assertRaises(ValueError):
            tsv([{'world': 'bad\tworld', 'native_id': 1, 'native_type': 'x',
                 'name': 'x', 'category': 'x', 'summary': 'x',
                 'placements': None, 'editable': False}])


if __name__ == '__main__':
    unittest.main()
