# SPDX-License-Identifier: GPL-3.0-only
"""Semantic entity regression. Local ROM assets are not required."""
import unittest
from scripts.object_catalog import mzm_sprite_kind
from scripts.room_annotations import build_aria, serialize


class EntityCategories(unittest.TestCase):
    def test_mzm_item_door_world_enemy_unknown(self):
        self.assertEqual(mzm_sprite_kind('PSPRITE_MISSILE_TANK'), 'ITEM')
        self.assertEqual(mzm_sprite_kind('PSPRITE_POWER_GRIP'), 'ITEM')
        self.assertEqual(mzm_sprite_kind('PSPRITE_MISSILE_DOOR'), 'DOOR')
        self.assertEqual(mzm_sprite_kind('PSPRITE_CHOZO_STATUE'), 'OBJECT')
        self.assertEqual(mzm_sprite_kind('PSPRITE_ZOOMER_RED'), 'ENEMY')
        self.assertEqual(mzm_sprite_kind('PSPRITE_SOMETHING_UNKNOWN'), 'OTHER')

    def test_mzm_stats_help_identify_other_enemies(self):
        self.assertEqual(mzm_sprite_kind('PSPRITE_NEW_HOSTILE', {
            'SPRITE_STATS_HEALTH': '20', 'SPRITE_STATS_DAMAGE': '3',
        }), 'ENEMY')
        self.assertEqual(mzm_sprite_kind('PSPRITE_UNKNOWN_ACTOR', {
            'SPRITE_STATS_HEALTH': '0', 'SPRITE_STATS_DAMAGE': '0',
        }), 'OTHER')
        with self.assertRaises(ValueError):
            mzm_sprite_kind('../PSPRITE_INVALID')

    def test_aria_kinds_are_distinct(self):
        base = {'entry_pointer': '0x08000100', 'x': 32, 'y': 16,
                'persistent_index': 1, 'flags': 0, 'parameters': []}
        entities = [dict(base, kind=k, entity_id=ident) for k, ident in
                    ((1, 7), (4, 3), (2, 0), (2, 0x1C), (3, 0), (0, 0))]
        rows = build_aria({'entities': entities, 'transitions': []}, {7: 'Bat'})
        self.assertEqual([row[0] for row in rows],
                         ['ENEMY', 'ITEM', 'DOOR', 'OBJECT', 'OBJECT', 'OTHER'])
        self.assertEqual(rows[0][7], 'kind-01:id-07')
        self.assertIn(b'ENEMY', serialize(rows))

    def test_conditional_aria_enemy_and_pickup_are_not_events(self):
        base = {'entry_pointer': '0x08000100', 'x': 32, 'y': 16,
                'persistent_index': 1, 'flags': 0, 'parameters': []}
        rows = build_aria({'entities': [dict(base, kind=2, entity_id=0x0B),
                                        dict(base, kind=5, entity_id=2)],
                           'transitions': []})
        self.assertEqual([row[0] for row in rows], ['ENEMY', 'ITEM'])

    def test_mzm_event_variants_keep_entity_kind(self):
        import tempfile
        from pathlib import Path
        from unittest.mock import patch
        from scripts import room_annotations as ann
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            room_dir = root / 'brinstar'
            room_dir.mkdir()
            (room_dir / 'brinstar_7.c').write_text('native fixture', encoding='utf-8')
            source = root / 'spritesets.c'
            source.write_text('native fixture', encoding='utf-8')
            header = root / 'rooms.c'
            header.write_text('native fixture', encoding='utf-8')
            fields = {
                'pDefaultSpriteData': 'sBrinstar_7_Spriteset1',
                'defaultSpriteset': '1',
                'pFirstSpriteData': 'sBrinstar_7_Spriteset2',
                'firstSpriteset': '2',
                'firstSpritesetEvent': 'EVENT_A',
                'pSecondSpriteData': 'sBrinstar_7_Spriteset3',
                'secondSpriteset': '3',
                'secondSpritesetEvent': 'EVENT_B',
            }
            descriptors = [{'area': 'Brinstar', 'index': 7, 'fields': fields}]
            placements = {1: [(1, 2, 0)], 2: [(2, 3, 0)], 3: [(3, 4, 0)]}
            spritesets = {
                1: [('PSPRITE_ZOOMER_RED', 0)],
                2: [('PSPRITE_MISSILE_TANK', 0)],
                3: [('PSPRITE_SAVE_PLATFORM', 0)],
            }
            records = [
                {'native_type': 'PSPRITE_ZOOMER_RED', 'category': 'Enemy / actor'},
                {'native_type': 'PSPRITE_MISSILE_TANK', 'category': 'Item / pickup'},
                {'native_type': 'PSPRITE_SAVE_PLATFORM', 'category': 'World object'},
            ]
            with patch.object(ann, 'ROOMS_ROOT', root),                  patch.object(ann, 'ROOM_SOURCE', header),                  patch.object(ann, 'SPRITESET_SOURCE', source),                  patch.object(ann, 'decode_room_descriptors', return_value=descriptors),                  patch.object(ann, 'parse_mzm_placements', return_value=placements),                  patch.object(ann, 'parse_mzm_spritesets', return_value=spritesets),                  patch.object(ann, 'build_mzm_catalog', return_value=records),                  patch.object(ann, '_mzm_door_entries', return_value=[]):
                rows = ann.build_mzm('Brinstar', 7)
            self.assertEqual([row[0] for row in rows], ['ENEMY', 'ITEM', 'OBJECT'])
            self.assertEqual([row[6] for row in rows], ['default', 'event-1', 'event-2'])
            self.assertIn('event=EVENT_A', rows[1][9])
            self.assertIn('event=EVENT_B', rows[2][9])
            self.assertEqual(rows[1][2:4], (3 * 16, 2 * 16))

    def test_mzm_variant_is_not_relabelled_event(self):
        from pathlib import Path
        source = Path('scripts/room_annotations.py').read_text(encoding='utf-8')
        self.assertNotIn("'ENTITY' if variant_name == 'default' else 'EVENT'", source)
        self.assertIn('category_to_kind.get(category, mzm_sprite_kind(sprite_name))', source)
        self.assertIn('event={event}', source)

    def test_gtk_distinct_filters_for_both_games(self):
        from pathlib import Path
        source = Path('editor/native_workspace.c').read_text(encoding='utf-8')
        self.assertIn('OVERLAY_ENEMIES', source)
        self.assertIn('OVERLAY_ITEMS', source)
        self.assertIn('OVERLAY_OTHER', source)
        self.assertIn('"Walls", "Enemies", "Items", "Objects", "Doors"', source)
        self.assertIn('if (item->kind >= OVERLAY_COUNT || !doc->overlays[item->kind])', source)


if __name__ == '__main__':
    unittest.main()
