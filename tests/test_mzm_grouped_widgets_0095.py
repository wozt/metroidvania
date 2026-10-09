# SPDX-License-Identifier: GPL-3.0-only
"""Regression: native Zero Mission screen mosaic uses Aria rectangle grouping."""
import unittest
from pathlib import Path


class MzmRectangularGrouping0095(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (Path(__file__).resolve().parents[1] /
                      'editor/world_atlas.c').read_text(encoding='utf-8')
        cls.grid = cls.source.split('static void grid_rebuild(WorldGrid *w)\n{', 1)[1].split(
            '\nstatic void begin_generation(WorldGrid *w);', 1)[0]

    def test_both_worlds_use_the_same_rectangle_partition(self):
        self.assertNotIn('w->world && c->provenance != 3', self.grid)
        self.assertEqual(self.grid.count('same_atlas_group(w, c, other)'), 2)
        self.assertIn('static gboolean same_atlas_group(', self.source)
        self.assertIn('a->provenance <= 1 && b->provenance <= 1', self.source)
        self.assertIn('a->room == 999', self.source)

    def test_screen_mosaic_is_inside_one_outer_cell(self):
        self.assertIn('GtkWidget *mosaic = gtk_grid_new();', self.grid)
        self.assertIn('gtk_grid_attach(GTK_GRID(mosaic), tile,', self.grid)
        self.assertIn('gtk_box_append(GTK_BOX(cell_content), mosaic);', self.grid)
        self.assertIn('gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_FILL);', self.grid)
        self.assertIn('has_image(w, part, tile_path, sizeof(tile_path))', self.grid)
        self.assertIn('map_cell_door_badge(tile, native_doors, part);', self.grid)
        self.assertNotIn('mv-join-left', self.source)

    def test_unowned_cells_remain_independent(self):
        self.assertIn('c->provenance != 3 && c->room != 999', self.grid)
        self.assertIn('a->provenance == 3 || b->provenance == 3', self.source)


if __name__ == '__main__':
    unittest.main()
