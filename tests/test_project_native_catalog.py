# SPDX-License-Identifier: GPL-3.0-only
"""Functional 0080 catalog binding (all fixtures ROM-free)."""
import unittest
from copy import deepcopy
from scripts import project_room_entities as pe


class ProjectNativeCatalogTests(unittest.TestCase):
    def test_zero_native_id_filters_enemies_items_and_objects(self):
        rows = [
            {"native_type": "PSPRITE_ZOOMER_RED", "name": "Zoomer", "category": "Enemy / actor"},
            {"native_type": "PSPRITE_MISSILE_TANK", "name": "Missile tank", "category": "Item / pickup"},
            {"native_type": "PSPRITE_POWER_GRIP", "name": "Power grip", "category": "Upgrade / ability"},
            {"native_type": "PSPRITE_SAVE_PLATFORM", "name": "Save platform", "category": "World object"},
            {"native_type": "PSPRITE_UNKNOWN", "name": "Unknown", "category": "Unused native type"},
        ]
        self.assertEqual([e['native_type'] for e in pe.catalog_options('mzm', 'ENEMY', rows)],
                         ['PSPRITE_ZOOMER_RED'])
        self.assertEqual({e['native_type'] for e in pe.catalog_options('mzm', 'ITEM', rows)},
                         {'PSPRITE_MISSILE_TANK', 'PSPRITE_POWER_GRIP'})
        self.assertEqual([e['native_type'] for e in pe.catalog_options('mzm', 'OBJECT', rows)],
                         ['PSPRITE_SAVE_PLATFORM'])

    def test_aria_constructor_categories_exclude_doors(self):
        rows = [{"native_type": token, "name": token, "category": "native"}
                for token in ('enemy:07', 'special-object:0A', 'special-object:00',
                              'special-object:1C', 'pickup:02', 'hard-mode-pickup:03',
                              'generic-candle:00')]
        self.assertEqual({e['native_type'] for e in pe.catalog_options('aria', 'ENEMY', rows)},
                         {'enemy:07', 'special-object:0A'})
        item_tokens = {e['native_type'] for e in pe.catalog_options('aria', 'ITEM', rows)}
        self.assertTrue({'pickup:02', 'hard-mode-pickup:03'}.issubset(item_tokens))
        self.assertIn('all-souls-reward:08', item_tokens)
        self.assertIn('pickup:01', item_tokens)
        self.assertNotIn('special-object:00', item_tokens)
        self.assertNotIn('special-object:00',
                         {e['native_type'] for e in pe.catalog_options('aria', 'OBJECT', rows)})

    def test_assign_preserves_room_position_and_rejects_wrong_role(self):
        doc = pe._new('mzm', 'Brinstar', 7, 320, 160)
        pe.create(doc, 'ENEMY', 16, 32, 'Unassigned marker')
        original = deepcopy(doc)
        rows = [{"native_type": "PSPRITE_ZOOMER_RED", "name": "Zoomer", "category": "Enemy / actor"},
                {"native_type": "PSPRITE_MISSILE_TANK", "name": "Missile tank", "category": "Item / pickup"}]
        with self.assertRaises(ValueError):
            pe.assign(doc, 1, 'PSPRITE_MISSILE_TANK', rows)
        self.assertEqual(doc, original)
        pe.assign(doc, 1, 'PSPRITE_ZOOMER_RED', rows)
        self.assertEqual(doc['entities'][0], {"id": 1, "kind": "ENEMY", "x": 16,
            "y": 32, "label": "Zoomer", "native_type": "PSPRITE_ZOOMER_RED"})
        pe.assign(doc, 1, 'unassigned', rows)
        self.assertEqual(doc['entities'][0]['native_type'], 'unassigned')

    def test_create_rejects_forged_native_type_without_mutation(self):
        from unittest.mock import patch
        doc = pe._new('mzm', 'Brinstar', 7, 320, 160)
        records = [{"native_type": "PSPRITE_ZOOMER_RED", "name": "Zoomer",
                    "category": "Enemy / actor"}]
        with patch.object(pe, 'catalog_options', return_value=records):
            with self.assertRaises(ValueError):
                pe.create(doc, 'ENEMY', 0, 0, 'Bad', 'PSPRITE_MISSILE_TANK')
            self.assertEqual(doc['entities'], [])
            entry = pe.create(doc, 'ENEMY', 0, 0, 'Custom Zoomer',
                              'PSPRITE_ZOOMER_RED')
            self.assertEqual(entry['native_type'], 'PSPRITE_ZOOMER_RED')

    def test_catalog_field_validation_and_no_arbitrary_source(self):
        with self.assertRaises(ValueError):
            pe.catalog_options('badworld', 'ENEMY', [])
        with self.assertRaises(ValueError):
            pe.catalog_options('mzm', 'DOOR', [])
        self.assertFalse(pe.catalog_options('mzm', 'ENEMY',
                         [{"native_type": '../../ROM', "name": "No", "category": "Enemy / actor"}]))


if __name__ == '__main__':
    unittest.main()
