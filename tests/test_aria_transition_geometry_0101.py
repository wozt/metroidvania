# SPDX-License-Identifier: GPL-3.0-only
"""Aria room transitions use 256px native map screens, not 240x160 LCDs."""
import unittest
from scripts.room_annotations import aria_transition_geometry, build_aria


def transition(x, y, idx=0):
    return {'entry_pointer': '0x08010000', 'source_screen_x': x,
            'source_screen_y': y, 'target_engine_area': 3,
            'target_room': 7, 'load_x': 0, 'load_y': 0}


def room(transitions):
    return {'entities': [], 'backgrounds': [
        {'layer': 1, 'width_screens': 2, 'height_screens': 2}],
        'transitions': transitions}


class AriaTransitionGeometry0101(unittest.TestCase):
    def test_all_four_boundaries_room_2x2(self):
        data = room([transition(-1, 1), transition(2, 1),
                     transition(1, 2), transition(1, -1)])
        self.assertEqual([x[:3] for x in
                         (aria_transition_geometry(data, tr)
                          for tr in data['transitions'])], [
            (0, 376, 'left-edge'), (496, 376, 'right-edge'),
            (376, 496, 'bottom-edge'), (376, 0, 'top-edge')])
        rows = build_aria(data)
        self.assertEqual([r[2:4] for r in rows],
                         [(0, 376), (496, 376), (376, 496), (376, 0)])
        self.assertEqual([r[0] for r in rows], ['DOOR'] * 4)
        self.assertIn('pixel_exact=false', rows[2][-1])
        self.assertIn('bottom-edge', rows[2][-1])

    def test_internal_source_screen_is_not_pixel_exact(self):
        self.assertEqual(aria_transition_geometry(room([]), transition(0, 1)),
                         (120, 376, 'screen-location-only'))

    def test_incomplete_background_only_locates_screen(self):
        data = {'backgrounds': [], 'entities': [], 'transitions': [transition(2, 1)]}
        self.assertEqual(aria_transition_geometry(data, data['transitions'][0]),
                         (632, 376, 'screen-location-only'))

    def test_invalid_source_does_not_create_fake_door(self):
        data = room([transition(8, 9)])
        rows = build_aria(data)
        self.assertEqual(rows[0][0], 'OTHER')
        self.assertIn('placement=unresolved', rows[0][-1])


if __name__ == '__main__':
    unittest.main()
