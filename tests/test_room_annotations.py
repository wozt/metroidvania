# SPDX-License-Identifier: GPL-3.0-only
import unittest

from scripts.room_annotations import (
    _mzm_door_entries,
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

    def test_mzm_real_door_and_none_sentinel(self):
        source = """const struct Door sBrinstarDoors[3] = {
            {
                .type = DOOR_TYPE_CLOSED_HATCH | DOOR_TYPE_NORMAL,
                .sourceRoom = 7,
                .xStart = 2, .xEnd = 2,
                .yStart = 15, .yEnd = 18,
                .destinationDoor = 2,
            },
            {
                .type = DOOR_TYPE_AREA_CONNECTION | DOOR_TYPE_NORMAL,
                .sourceRoom = 9,
                .xStart = 10, .xEnd = 12,
                .yStart = 4, .yEnd = 4,
                .destinationDoor = 0,
            },
            {
                .type = DOOR_TYPE_NONE,
                .sourceRoom = 0,
                .xStart = 0, .xEnd = 0,
                .yStart = 0, .yEnd = 0,
                .destinationDoor = 0,
            }
};"""
        self.assertEqual(_mzm_door_entries(source, "Brinstar", 7), [{
            "index": 0, "type": "DOOR_TYPE_CLOSED_HATCH | DOOR_TYPE_NORMAL",
            "sourceRoom": 7, "destinationDoor": 2, "xStart": 2, "xEnd": 2,
            "yStart": 15, "yEnd": 18,
        }])
        self.assertEqual(_mzm_door_entries(source, "Brinstar", 9)[0]["index"], 1)
        self.assertEqual(_mzm_door_entries(source, "Brinstar", 0), [])

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
