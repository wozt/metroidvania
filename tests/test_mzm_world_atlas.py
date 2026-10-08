# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.mzm_world_atlas import AREAS, decode_doors, make_atlas, render_tsv

class WorldAtlasTests(unittest.TestCase):
    def setUp(self):
        self.rooms = []
        for area in AREAS:
            for i in range(2):
                self.rooms.append({'area': area, 'index': i, 'id': f'mzm:{area.lower()}:{i:03}',
                                   'fields': {'mapX': str(i+4), 'mapY': '2', 'tileset': '27',
                                              'musicTrack': 'MUSIC_BRINSTAR',
                                              'pBg3Data': 'sBrinstar_Bg3_0'}})
        self.src = '\n'.join(
            f'const struct Door s{area}Doors[2] = {{\n'
            '  {.type = DOOR_TYPE_CLOSED_HATCH | DOOR_TYPE_NORMAL, .sourceRoom = 0, '
            '.xStart = 2, .xEnd = 2, .yStart = 1, .yEnd = 1, .destinationDoor = 1},\n'
            '  {.type = DOOR_TYPE_CLOSED_HATCH | DOOR_TYPE_NORMAL, .sourceRoom = 1, '
            '.xStart = 9, .xEnd = 9, .yStart = 1, .yEnd = 1, .destinationDoor = 0},\n};'
            for area in AREAS)
    def test_topology(self):
        edges, stats = decode_doors(self.src, self.rooms)
        self.assertEqual(len(edges), 14)
        self.assertEqual(stats['door_entries'], 14)
        atlas = make_atlas(self.rooms, edges, stats)
        self.assertEqual(len(atlas['rooms']), 14)
        self.assertEqual(render_tsv(atlas).count(b'\nD|'), 14)
        self.assertIn(b'R|Brinstar|0|4|2|27', render_tsv(atlas))
    def test_cross_area_not_fabricated(self):
        src = self.src.replace('DOOR_TYPE_CLOSED_HATCH | DOOR_TYPE_NORMAL',
                               'DOOR_TYPE_AREA_CONNECTION | DOOR_TYPE_NORMAL', 1)
        edges, stats = decode_doors(src, self.rooms)
        self.assertEqual(len(edges), 13)
        self.assertEqual(stats['unresolved_area_or_unknown_doors'], 1)
    def test_malformed_rejected(self):
        with self.assertRaises(ValueError): decode_doors(self.src.replace('sourceRoom = 0','sourceRoom = 255',1),self.rooms)
        with self.assertRaises(ValueError): decode_doors(self.src.replace('Doors[2]','Doors[3]',1),self.rooms)
        with self.assertRaises(ValueError): decode_doors(self.src.replace('destinationDoor = 1','destinationDoor = -1',1),self.rooms)

if __name__ == '__main__': unittest.main()
