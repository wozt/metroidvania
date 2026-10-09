# SPDX-License-Identifier: GPL-3.0-only
import unittest

from scripts.object_catalog import (
    aria_entity_identity,
    build_aria,
    build_mzm,
    decode_aria_enemy_names,
    parse_aria_symbols,
    tsv,
)


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
        records = build_aria(catalog, {7: 'Killer Fish'})
        self.assertEqual(records[0]['placements'], 2)
        self.assertEqual(records[0]['name'], 'Killer Fish')
        self.assertIn('health=120', records[0]['summary'])
        self.assertIn('Aria of Sorrow\t7\tenemy:07', tsv(records))

    def test_aria_enemy_name_comes_from_native_constructor_pointer(self):
        rom = bytearray(0xE9644 + 0x71 * 0x24)
        for enemy_id in range(0x71):
            offset = 0xE9644 + enemy_id * 0x24
            rom[offset:offset + 4] = (0x08001235).to_bytes(4, 'little')
        symbols = parse_aria_symbols(['EnemyBatCreate: @ 0x08001234\n'])
        names = decode_aria_enemy_names(bytes(rom), symbols)
        self.assertEqual(names[0], 'Bat')

    def test_aria_non_enemy_types_have_semantic_names(self):
        self.assertEqual(aria_entity_identity(2, 0x1C),
                         ('Save point', 'World object / event'))
        self.assertEqual(aria_entity_identity(4, 5)[0], 'Red soul candle')

    def test_tsv_rejects_control_characters(self):
        with self.assertRaises(ValueError):
            tsv([{'world': 'bad\tworld', 'native_id': 1, 'native_type': 'x',
                 'name': 'x', 'category': 'x', 'summary': 'x',
                 'placements': None, 'editable': False}])


if __name__ == '__main__':
    unittest.main()
