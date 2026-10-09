# SPDX-License-Identifier: GPL-3.0-only
"""ROM-independent project-only placements, bounds and overlap validation."""
import json
import tempfile
import unittest
from pathlib import Path
from scripts import authored_rooms, map_placements


class PlacementContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.make('zero_mission', 0, 'hall', 2, 3)
        self.make('zero_mission', 0, 'tiny', 1, 1)
        self.make('zero_mission', 1, 'other', 2, 2)
        self.make('aria', 0, 'chapel', 2, 2)
        self.make('aria', 1, 'tower', 1, 2)
        folder = self.root / 'assets' / 'extracted' / 'world_overview'
        folder.mkdir(parents=True)
        (folder / 'mzm.tsv').write_text('# area|room|x|y|save|warp|provenance\n'
                                        '0|999|3|3|0|0|3\n1|999|3|3|0|0|3\n')
        (folder / 'aria.tsv').write_text('# native map\n'
                                         '0|7|20|20|0|0|2\n1|9|22|22|0|0|2\n')

    def make(self, world, area, slug, width, height):
        draft = authored_rooms.new_room(world=world, area=area, slug=slug,
                                       name=slug.title(), width_screens=width,
                                       height_screens=height)
        authored_rooms.save_room(draft, self.root)

    def test_place_move_unplace_does_not_modify_draft(self):
        original = next(iter(authored_rooms.list_rooms(self.root, 'zero_mission')))
        before = authored_rooms.room_path(original, self.root).read_bytes()
        a = 'zero_mission:00:hall'
        self.assertEqual(map_placements.place(a, 1, 7, self.root)['x'], 1)
        self.assertEqual(map_placements.place(a, 8, 7, self.root)['x'], 8)
        self.assertEqual(len(map_placements.lines(self.root)), 1)
        self.assertTrue(map_placements.remove(a, self.root))
        self.assertFalse(map_placements.remove(a, self.root))
        self.assertEqual(map_placements.lines(self.root), [])
        self.assertEqual(authored_rooms.room_path(original, self.root).read_bytes(), before)

    def test_reject_original_tile_footprint_overlap_and_bounds(self):
        a = 'zero_mission:00:hall'
        for x, y in [(2, 2), (30, 30), (-1, 0), (True, 1), (0, 31)]:
            with self.subTest(x=x, y=y), self.assertRaises(ValueError):
                map_placements.place(a, x, y, self.root)
        map_placements.place(a, 7, 7, self.root)
        with self.assertRaisesRegex(ValueError, 'overlapping private'):
            map_placements.place('zero_mission:00:tiny', 7, 8, self.root)
        # Distinct Zero Mission regions use distinct 32x32 grids.
        map_placements.place('zero_mission:01:other', 7, 7, self.root)

    def test_aria_regions_share_one_castle_grid(self):
        map_placements.place('aria:00:chapel', 1, 1, self.root)
        with self.assertRaisesRegex(ValueError, 'overlapping private'):
            map_placements.place('aria:01:tower', 2, 1, self.root)
        with self.assertRaisesRegex(ValueError, 'original'):
            map_placements.place('aria:01:tower', 20, 20, self.root)
        with self.assertRaises(ValueError):
            map_placements.place('aria:01:tower', 63, 34, self.root)

    def test_missing_source_and_tampered_document_fail_closed(self):
        source = self.root / 'assets/extracted/world_overview/mzm.tsv'
        source.unlink()
        with self.assertRaisesRegex(ValueError, 'generate verified'):
            map_placements.place('zero_mission:00:hall', 0, 0, self.root)
        source.write_text('0|999|3|3|0|0|3\n')
        map_placements.place('zero_mission:00:hall', 6, 6, self.root)
        path = map_placements.path_for(self.root)
        data = json.loads(path.read_text())
        data['placements'][0]['x'] = True
        path.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            map_placements.load(self.root)

    def test_cli_lines_and_symlink_refused(self):
        map_placements.place('aria:00:chapel', 7, 9, self.root)
        self.assertEqual(map_placements.lines(self.root, 'zero_mission'), [])
        self.assertEqual(map_placements.lines(self.root, 'aria'),
                         ['aria:00:chapel\tChapel\taria\t0\t7\t9\t2\t2'])
        file = map_placements.path_for(self.root)
        file.unlink()
        file.symlink_to(self.root / 'outside.json')
        with self.assertRaisesRegex(ValueError, 'unsafe'):
            map_placements.load(self.root)

    def test_reject_untrusted_placement_reference(self):
        with self.assertRaises(ValueError):
            map_placements.place('../roms', 1, 1, self.root)
        with self.assertRaises(ValueError):
            map_placements.remove('../roms', self.root)


if __name__ == '__main__':
    unittest.main()
