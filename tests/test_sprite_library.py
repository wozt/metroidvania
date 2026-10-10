# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free contracts for the shared content-addressed sprite library."""
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from scripts import aos_soma_pipeline
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


class SomaOriginTests(unittest.TestCase):
    def test_body_cells_are_placed_relative_to_somas_origin(self):
        rgba = bytearray(64 * 64 * 4)
        rgba[(47 * 64 + 32) * 4:(47 * 64 + 32) * 4 + 4] = bytes((1, 2, 3, 255))
        with mock.patch.object(aos_soma_pipeline, "extract_cell_tiles",
                               return_value=(b"", {"sheet_index": 0, "quadrant": (0, 0)})), \
                mock.patch.object(aos_soma_pipeline, "decode_cell",
                                  return_value=(bytes(rgba), 64, 64)):
            (_, left, top), _ = aos_soma_pipeline.soma_frame(b"", 0, b"")
        self.assertEqual((left, top), (0, 0))

    def test_knife_offsets_come_from_its_oam_position(self):
        rgba = bytes((5, 6, 7, 255)) + bytes(4 * 63)
        metadata = {"width": 8, "height": 8, "position": (32 + 12, 47 - 20)}
        with mock.patch.object(aos_soma_pipeline, "extract_knife_frame",
                               return_value=(b"", metadata)), \
                mock.patch.object(aos_soma_pipeline, "decode_tiles",
                                  return_value=(rgba, 8, 8)):
            (_, left, top), _ = aos_soma_pipeline.knife_frame(b"", 0, b"")
        self.assertEqual((left, top), (12, -20))


if __name__ == "__main__":
    unittest.main()
