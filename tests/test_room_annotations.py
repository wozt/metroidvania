# SPDX-License-Identifier: GPL-3.0-only
import unittest

from scripts.room_annotations import (
    build_aria,
    parse_mzm_placements,
    parse_mzm_spritesets,
    serialize,
)


class RoomAnnotationTests(unittest.TestCase):
    def test_mzm_spriteset_and_variant_placements(self):
        sets = parse_mzm_spritesets('\n'.join(
            f'const u8 sSpriteset{i}[4] = {{ PSPRITE_ZOOMER_RED, 2, SPRITESET_EMPTY\n}};'
            for i in range(100)))
        self.assertEqual(sets[4], [('PSPRITE_ZOOMER_RED', 2)])
        source = '''const u8 sBrinstar_7_Spriteset1[ENEMY_ROOM_DATA_ARRAY_SIZE(3)] = {
            4, 8, SPRITESET_IDX(0),
            7, 9, SPRITESET_IDX(1),
            ROOM_SPRITE_DATA_TERMINATOR
        };'''
        self.assertEqual(parse_mzm_placements(source, 'Brinstar', 7),
                         {1: [(4, 8, 0), (7, 9, 1)]})

    def test_mzm_room_without_placements_is_valid(self):
        self.assertEqual(parse_mzm_placements(
            'const u8 unrelated[] = { 0 };', 'Brinstar', 7), {})

    def test_aria_entities_and_transition_anchors(self):
        room = {'entities': [{
            'entry_pointer': '0x08000100', 'x': 40, 'y': 56,
            'persistent_index': 2, 'kind': 1, 'entity_id': 9,
            'flags': 3, 'parameters': [4, 5],
        }], 'transitions': [{
            'entry_pointer': '0x08000200', 'source_screen_x': 2,
            'source_screen_y': 1, 'target_engine_area': 3, 'target_room': 7,
            'load_x': 12, 'load_y': 20,
        }]}
        rows = build_aria(room, {9: 'Blue Crow'})
        self.assertEqual(rows[0][0:6], ('ENEMY', 0, 32, 48, 16, 16))
        self.assertEqual(rows[1][0:6], ('DOOR', 0, 480, 160, 16, 16))
        self.assertIn(b'Blue Crow', serialize(rows))

    def test_aria_entity_marker_stays_inside_room_origin(self):
        room = {'entities': [{
            'entry_pointer': '0x08000100', 'x': 0, 'y': 0,
            'persistent_index': 0, 'kind': 2, 'entity_id': 3,
            'flags': 0, 'parameters': [],
        }], 'transitions': []}
        self.assertEqual(build_aria(room)[0][2:4], (0, 0))

    def test_reject_untrusted_row(self):
        with self.assertRaises(ValueError):
            serialize([('UNKNOWN', 0, 0, 0, 1, 1, '-', '-', '-', '-')])


if __name__ == '__main__':
    unittest.main()
