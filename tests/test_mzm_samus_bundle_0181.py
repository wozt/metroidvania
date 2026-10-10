# SPDX-License-Identifier: GPL-3.0-only
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts.asset_layout import (
    METROID_SAMUS_BODY_SOURCE,
    METROID_SAMUS_COMPOSED_SOURCE,
    METROID_SAMUS_METADATA,
    METROID_SAMUS_RUNTIME,
    METROID_SAMUS_SPECIAL_SOURCE,
)
from scripts.mzm_samus_bundle_0181 import SUITS
from scripts.mzm_samus_pipeline import build_animation_map, prepare_sources, produce

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
                    "PowerSuit/standing_forward_left": {
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
            animation_map = destination / "animation_map.tsv"
            self.assertEqual(result, repeated)
            self.assertEqual(result["unique_bmps"], 1)
            self.assertIn("assets/extracted/metroid/sprites/samus/runtime/objects/",
                          index.read_text(encoding="utf-8"))
            self.assertIn(
                "idle\tVariaSuit\tleft\tforward\tloop\t"
                "PowerSuit/standing_forward_left\n",
                animation_map.read_text(encoding="utf-8"))
            self.assertFalse((root / "assets/extracted/samus").exists())

    def test_animation_map_marks_transitions_and_missing_native_suit_states(self):
        catalogue = {"sequences": {
            "PowerSuit/standing_forward_left": {},
            "PowerSuit/startingspinjump_left": {},
            "Suitless/left_standing": {},
        }}
        content, report = build_animation_map(catalogue)
        self.assertIn(
            "spin_start\tPowerSuit\tleft\tnone\tonce\t"
            "PowerSuit/startingspinjump_left\n", content)
        self.assertIn(
            "idle\tSuitless\tleft\tforward\tloop\tSuitless/left_standing\n",
            content)
        self.assertIn("space_jump/Suitless/left/none", report["missing"])
        self.assertEqual(report["requested_suit_sources"]["GravitySuit"],
                         "PowerSuit")

    def test_full_preparation_orchestrates_all_native_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rom = root / "rom.gba"
            elf = root / "mzm.elf"
            decomp = root / "mzm"
            rom.write_bytes(b"verified-rom-fixture")
            elf.write_bytes(b"ELF fixture")
            (decomp / "src/data/samus").mkdir(parents=True)
            (decomp / "include/constants").mkdir(parents=True)
            (decomp / "src/data/samus/samus_animation_pointers.c").write_text(
                "pointers", encoding="utf-8")
            (decomp / "include/constants/samus.h").write_text(
                "constants", encoding="utf-8")
            symbolic = {"variant_count": 4}
            addresses = {"resolved_symbols": 3}
            body = {"animations": {"one": {"body_status": "exported-body-only"}}}
            composed = {"sequences": {"one": {"status": "diagnostic-composed"}}}
            special = {"sequences": {
                "one": {"status": "body-only-diagnostic-composed"}}}
            expected_sha1 = hashlib.sha1(rom.read_bytes()).hexdigest()
            patches = (
                mock.patch("scripts.mzm_samus_pipeline.EXPECTED_SHA1", expected_sha1),
                mock.patch("scripts.mzm_samus_pipeline.collect_symbolic_catalog",
                           return_value=symbolic),
                mock.patch("scripts.mzm_samus_pipeline._run_nm",
                           side_effect=("plain", "sized")),
                mock.patch("scripts.mzm_samus_pipeline.parse_symbols",
                           return_value={}),
                mock.patch("scripts.mzm_samus_pipeline.resolve",
                           return_value=addresses),
                mock.patch("scripts.mzm_samus_pipeline.parse_nm_sizes",
                           return_value={}),
                mock.patch("scripts.mzm_samus_pipeline.resolve_suit_palettes",
                           return_value={}),
                mock.patch("scripts.mzm_samus_pipeline.export_bodies",
                           return_value=body),
                mock.patch("scripts.mzm_samus_pipeline.parse_sized_symbols",
                           return_value={}),
                mock.patch("scripts.mzm_samus_pipeline.collect_compositions",
                           return_value=composed),
                mock.patch("scripts.mzm_samus_pipeline.compose_special",
                           return_value=special),
            )
            with patches[0], patches[1], patches[2], patches[3], patches[4], \
                    patches[5], patches[6], patches[7], patches[8], patches[9], \
                    patches[10]:
                result = prepare_sources(root, rom, elf, decomp)
            self.assertEqual(result["body_animations"], 1)
            self.assertTrue((root / METROID_SAMUS_METADATA / "catalog.json").is_file())
            self.assertTrue((root / METROID_SAMUS_BODY_SOURCE / "manifest.json").is_file())
            self.assertTrue((root / METROID_SAMUS_COMPOSED_SOURCE / "manifest.json").is_file())
            self.assertTrue((root / METROID_SAMUS_SPECIAL_SOURCE / "manifest.json").is_file())
