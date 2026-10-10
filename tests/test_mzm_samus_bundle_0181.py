# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts.asset_layout import METROID_SAMUS_RUNTIME
from scripts.mzm_samus_bundle_0181 import SUITS
from scripts.mzm_samus_pipeline import produce

class SamusBundle0181Tests(unittest.TestCase):
    def test_suits(self):
        self.assertEqual(len(SUITS), 5)
        self.assertEqual(SUITS[0], "PowerSuit")

    def test_pipeline_writes_only_the_canonical_content_addressed_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "assets/extracted/source/frame.bmp"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"BMprivate-frame")
            catalogue = {
                "schema": "test-source",
                "suits": {suit: int(suit == "PowerSuit") for suit in SUITS},
                "sources": ["test"],
                "sequences": {
                    "PowerSuit/idle": {
                        "frames": [{
                            "index": 0, "duration_ticks": 7,
                            "bmp": "assets/extracted/source/frame.bmp",
                        }],
                    },
                },
            }
            with mock.patch("scripts.mzm_samus_pipeline.build",
                            return_value=catalogue):
                result = produce(root)
                repeated = produce(root)
            destination = root / METROID_SAMUS_RUNTIME
            index = destination / "runtime_index.tsv"
            self.assertEqual(result, repeated)
            self.assertEqual(result["unique_bmps"], 1)
            self.assertIn("assets/extracted/metroid/sprites/samus/runtime/objects/",
                          index.read_text(encoding="utf-8"))
            self.assertFalse((root / "assets/extracted/samus").exists())
