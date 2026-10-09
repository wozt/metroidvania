# SPDX-License-Identifier: GPL-3.0-only
"""Original minimap room-span contracts without ROM files."""
import unittest
from scripts.world_overview import (
    build_aria,
    expand_mzm_clip_cells,
    output_rows,
    resolve_mzm_minimap_cells,
)


class MulticellWorldTests(unittest.TestCase):
    def test_mzm_source_bounds_expand_to_multiple_screen_cells(self):
        anchors = [(0, 3, 4, 2, 0, 0)]
        rows = expand_mzm_clip_cells(anchors, {(0, 3): (34, 34)})
        self.assertEqual(rows, [
            (0, 3, 4, 2, 0, 0, 1), (0, 3, 5, 2, 0, 0, 1),
            (0, 3, 4, 3, 0, 0, 1), (0, 3, 5, 3, 0, 0, 1),
            (0, 3, 4, 4, 0, 0, 1), (0, 3, 5, 4, 0, 0, 1)])
        self.assertIn(b'0|3|5|4|0|0|1', output_rows('mzm', rows))

    def test_no_clipdata_keeps_anchor(self):
        self.assertEqual(expand_mzm_clip_cells([(0, 3, 4, 2, 1, 0)], {}),
                         [(0, 3, 4, 2, 1, 0, 0)])

    def test_bounds_never_claim_other_room_anchor(self):
        anchors = [(0, 3, 4, 2, 0, 0), (0, 4, 5, 2, 0, 0)]
        rows = expand_mzm_clip_cells(anchors, {(0, 3): (34, 14)})
        self.assertEqual([r[:4] for r in rows], [r[:4] for r in anchors])
        self.assertTrue(all(r[-1] == 0 for r in rows))

    def test_candidate_collision_falls_back_both_rooms(self):
        anchors = [(0, 3, 3, 3, 0, 0), (0, 4, 2, 5, 0, 0)]
        rows = expand_mzm_clip_cells(anchors, {(0, 3): (19, 54), (0, 4): (79, 14)})
        self.assertEqual([r[:4] for r in rows], [r[:4] for r in anchors])
        self.assertTrue(all(r[-1] == 0 for r in rows))

    def test_out_of_minimap_bounds_keeps_anchor(self):
        anchors = [(0, 4, 31, 31, 0, 0)]
        self.assertEqual(expand_mzm_clip_cells(anchors, {(0, 4): (34, 14)}),
                         [(0, 4, 31, 31, 0, 0, 0)])

    def test_native_occupancy_clips_engine_rectangle_and_records_variants(self):
        anchors = [(0, 7, 4, 2, 0, 0), (0, 9, 4, 2, 1, 0)]
        native = [
            (0, 999, 4, 2, 0, 0, 3, 0x141),
            (0, 999, 5, 2, 0, 0, 3, 0x142),
            # The rectangle's lower-right hole is absent from the native map.
            (0, 999, 4, 3, 0, 0, 3, 0x143),
        ]
        rows, report = resolve_mzm_minimap_cells(
            anchors, {(0, 7): (34, 24), (0, 9): (34, 24)}, native)
        self.assertEqual([(r[1], r[2], r[3], r[6], r[8]) for r in rows], [
            (7, 4, 2, 1, '7,9'), (7, 4, 3, 1, '7,9'),
            (7, 5, 2, 1, '7,9'),
        ])
        self.assertEqual(report['owned_native_cells'], 3)
        self.assertEqual(report['variant_families'][0]['rooms'], [7, 9])

    def test_overlapping_geometry_without_origin_evidence_stays_ambiguous(self):
        anchors = [(0, 1, 2, 2, 0, 0), (0, 2, 3, 1, 0, 0)]
        native = [(0, 999, 3, 2, 0, 0, 3, 0x155)]
        rows, report = resolve_mzm_minimap_cells(
            anchors, {(0, 1): (34, 14), (0, 2): (19, 24)}, native)
        self.assertEqual(rows[0][1], 999)
        self.assertEqual(rows[0][8], 'ambiguous:1,2')
        self.assertEqual(report['ambiguous_native_cells'], 1)

    def test_aria_native_multi_cells_never_fill_holes(self):
        cat = {'format': 'MV_AOS_WORLD_2', 'mapped_cells': 3, 'rooms': [
            {'engine_area': 0, 'room': 7, 'map_cells': [
                {'map_x': 3, 'map_y': 4, 'save': False, 'warp': False},
                {'map_x': 4, 'map_y': 4, 'save': False, 'warp': False},
                {'map_x': 3, 'map_y': 5, 'save': False, 'warp': False}]}]}
        rows = build_aria(cat)
        self.assertEqual(len(rows), 3)
        self.assertNotIn((0, 7, 4, 5, 0, 0), rows)
        self.assertTrue(all(line.endswith('|2') for line in
                            output_rows('aria', rows).decode().splitlines()[2:]))

    def test_invalid_provenance_rejected(self):
        with self.assertRaisesRegex(ValueError, 'provenance'):
            output_rows('aria', [(0, 1, 2, 3, 0, 0, 8)])


if __name__ == '__main__':
    unittest.main()
