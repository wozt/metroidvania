# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free contracts for the shared content-addressed sprite library."""
import struct
import tempfile
import unittest
from pathlib import Path
from scripts.asset_layout import ARIA_SOMA_RUNTIME
from scripts.sprite_library import INDEX_SCHEMA, LibraryWriter, bmp_from_pixels


class BitmapTests(unittest.TestCase):
    def test_pixels_are_cropped_and_keep_their_origin(self):
        bmp, left, top = bmp_from_pixels({(-3, -7): (1, 2, 3), (-2, -6): (4, 5, 6)})
        self.assertEqual((left, top), (-3, -7))
        self.assertEqual(struct.unpack_from("<ii", bmp, 18), (2, 2))
        # Bottom-up rows: the first stored row is the lower one.
        self.assertEqual(bmp[122 + 4:122 + 8], bytes((6, 5, 4, 255)))
        self.assertEqual(bmp[122 + 8:122 + 12], bytes((3, 2, 1, 255)))
        with self.assertRaisesRegex(ValueError, "no visible"):
            bmp_from_pixels({})


class LibraryWriterTests(unittest.TestCase):
    def test_deduplicates_indexes_and_prunes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bmp, left, top = bmp_from_pixels({(0, -1): (9, 9, 9)})
            writer = LibraryWriter(root, ARIA_SOMA_RUNTIME)
            (writer.objects / ("f" * 64 + ".bmp")).write_bytes(b"BMold")
            writer.add("Soma/animation_000", [(bmp, 4, left, top), (bmp, 2, left, top)])
            writer.add("Knife/animation_000", [(bmp, 1, left, top)])
            with self.assertRaisesRegex(ValueError, "duplicate"):
                writer.add("Soma/animation_000", [(bmp, 1, 0, 0)])
            with self.assertRaisesRegex(ValueError, "duration"):
                writer.add("Soma/animation_001", [(bmp, 0, 0, 0)])
            totals = writer.finish()
            self.assertEqual((totals["sequences"], totals["frames"],
                              totals["unique_bmps"], totals["removed_stale_objects"]),
                             (2, 3, 1, 1))
            lines = (writer.destination / "runtime_index.tsv").read_text().splitlines()
            self.assertEqual(lines[0], "schema\t" + INDEX_SCHEMA)
            self.assertTrue(lines[2].startswith("Soma/animation_000\t1\t2\t0\t-1\t"))


if __name__ == "__main__":
    unittest.main()
