# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free safety tests for the asset database importer."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from scripts import import_game_assets as asset


class ImporterTests(unittest.TestCase):
    def test_offsets_and_exact_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(asset, "OUTPUT", Path(directory)):
                entries = asset.extract_database_entries([
                    {"dir": "sound", "path": "sample.bin", "addr": {"us": "0x2"},
                     "count": "0x3", "size": 1},
                    {"dir": "data", "path": "rooms/brinstar.bin", "addr": {"us": "0x6"},
                     "count": "0x2", "size": 2},
                ], bytes(range(12)), "metroid")
                self.assertEqual([e["length"] for e in entries], [3, 4])
                self.assertEqual((Path(directory) / "metroid/raw/sound/sample.bin")
                                 .read_bytes(), bytes([2, 3, 4]))
                self.assertEqual((Path(directory) / "metroid/raw/data/rooms/brinstar.bin")
                                 .read_bytes(), bytes([6, 7, 8, 9]))
                self.assertEqual(entries[0]["status"], "RAW_UNDECODED")
                # Re-import is deterministic and non-destructive.
                self.assertEqual(asset.extract_database_entries([
                    {"dir": "sound", "path": "sample.bin", "addr": {"us": "0x2"},
                     "count": "0x3", "size": 1},
                ], bytes(range(12)), "metroid"), [entries[0]])

    def test_reject_traversal(self):
        for name in ("../escape.bin", "/tmp/leak.bin", "../../rom.gba", "folder/../x"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    asset.extract_database_entries([
                        {"path": name, "addr": {"us": "0x0"},
                         "count": "0x1", "size": 1}], bytes(10), "aria", write=False)

    def test_reject_out_of_bounds_before_any_write(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(asset, "OUTPUT", Path(directory)):
                with self.assertRaises(ValueError):
                    asset.extract_database_entries([
                        {"path": "valid.bin", "addr": {"us": "0x0"},
                         "count": "0x2", "size": 1},
                        {"path": "bad.bin", "addr": {"us": "0x9"},
                         "count": "0x100", "size": 1},
                    ], bytes(10), "aria")
                self.assertEqual(list(Path(directory).rglob("*")), [])

    def test_reject_symlink_redirect(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "metroid").symlink_to(root / "elsewhere")
            with patch.object(asset, "OUTPUT", root):
                with self.assertRaises(ValueError):
                    asset.write_generated("metroid/raw/data/block.bin", b"secret")
            self.assertFalse((root / "elsewhere").exists())

    def test_reject_duplicates(self):
        block = {"path": "same.bin", "addr": {"us": "0x0"},
                 "count": "0x1", "size": 1}
        with self.assertRaises(ValueError):
            asset.extract_database_entries([block, block], bytes(10), "aria", write=False)

    def test_dont_bundle_rom_bytes_in_manifest(self):
        self.assertEqual(asset.classify_raw("sound", "direct_sound_samples/1.bin"),
                         "raw_audio_or_sound_data")
        self.assertEqual(asset.safe_parts("room/data_123.bin"), ("room", "data_123.bin"))

    def test_database_entries_use_canonical_world_roots(self):
        metroid = asset.extract_database_entries([
            {"path": "block.bin", "addr": {"us": "0x0"},
             "count": "0x1", "size": 1},
        ], bytes(1), "metroid", write=False)
        aria = asset.extract_database_entries([
            {"path": "block.bin", "addr": {"us": "0x0"},
             "count": "0x1", "size": 1},
        ], bytes(1), "aria", write=False)
        self.assertEqual(metroid[0]["path"], "metroid/raw/data/block.bin")
        self.assertEqual(aria[0]["path"], "aria/raw/data/block.bin")


if __name__ == "__main__":
    unittest.main()
