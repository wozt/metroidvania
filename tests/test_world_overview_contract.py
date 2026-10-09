# SPDX-License-Identifier: GPL-3.0-only
"""Aria overview uses only real native minimap cells, never room extents."""
from __future__ import annotations

import unittest

from scripts.world_overview import build_aria, output_rows


class AriaOverviewContract(unittest.TestCase):
    def catalog(self):
        return {
            'format': 'MV_AOS_WORLD_2', 'mapped_cells': 3,
            'rooms': [
                {'engine_area': 0, 'room': 4, 'map_cells': [
                    {'map_x': 0, 'map_y': 0, 'save': False, 'warp': False},
                    {'map_x': 1, 'map_y': 0, 'save': True, 'warp': False},
                ]},
                {'engine_area': 2, 'room': 1, 'map_cells': [
                    {'map_x': 63, 'map_y': 34, 'save': False, 'warp': True},
                ]},
            ],
        }

    def test_native_room_cells_no_synthetic_footprints(self):
        cells = build_aria(self.catalog())
        self.assertEqual(cells, [(0, 4, 0, 0, 0, 0),
                                 (0, 4, 1, 0, 1, 0),
                                 (2, 1, 63, 34, 0, 1)])
        self.assertIn(b'2|1|63|34|0|1', output_rows('aria', cells))

    def test_top_level_cells_backwards_compatible(self):
        cat = self.catalog()
        cat['map_cells'] = [dict(engine_area=r['engine_area'], room=r['room'], **cell)
                            for r in cat['rooms'] for cell in r['map_cells']]
        self.assertEqual(len(build_aria(cat)), 3)

    def test_count_mismatch_rejected(self):
        cat = self.catalog()
        cat['mapped_cells'] = 4
        with self.assertRaisesRegex(ValueError, 'mapped_cells'):
            build_aria(cat)

    def test_overlapping_native_cell_rejected(self):
        cat = self.catalog()
        cat['rooms'][1]['map_cells'][0]['map_x'] = 1
        cat['rooms'][1]['map_cells'][0]['map_y'] = 0
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            build_aria(cat)

    def test_missing_native_cells_rejected(self):
        with self.assertRaisesRegex(ValueError, 'no native room'):
            build_aria({'format': 'MV_AOS_WORLD_2'})

    def test_out_of_bounds_and_wrong_types_rejected(self):
        for key, value in [('map_x', 64), ('map_y', 35), ('map_x', True),
                           ('warp', 1), ('save', 'false')]:
            with self.subTest(key=key, value=value):
                cat = self.catalog()
                cat['rooms'][0]['map_cells'][0][key] = value
                with self.assertRaises(ValueError):
                    build_aria(cat)

    def test_empty_native_grid_rejected(self):
        cat = self.catalog()
        cat['rooms'] = []
        cat['mapped_cells'] = 0
        with self.assertRaisesRegex(ValueError, 'empty Aria map'):
            build_aria(cat)


if __name__ == '__main__':
    unittest.main()
