import struct
import unittest
from scripts.mzm_samus_palette_scan import palette_rows, score_palette, find_candidates, bmp_swatch


class PaletteScanTests(unittest.TestCase):
    def test_palette_decode(self):
        data = b"".join(struct.pack("<16H", *range(i, i + 16)) for i in (0, 16, 32))
        self.assertEqual(palette_rows(data, 0)[0][1], 1)
        self.assertEqual(len(bmp_swatch(palette_rows(data, 0))) > 54, True)

    def test_reject_invalid_bounds(self):
        with self.assertRaises(ValueError):
            palette_rows(bytes(50), 0)
        with self.assertRaises(ValueError):
            palette_rows(bytes(96), 1)

    def test_rank_and_scan(self):
        row = tuple(range(16))
        data = struct.pack("<16H", *row) * 3 + bytes(96)
        self.assertGreater(score_palette(palette_rows(data, 0)), 0)
        self.assertEqual(find_candidates(data, threshold=1)[0][1], 0)

    def test_empty_range(self):
        with self.assertRaises(ValueError):
            find_candidates(bytes(128), start=200)
