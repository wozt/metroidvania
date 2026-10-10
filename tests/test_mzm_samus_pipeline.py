# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for the table-driven Samus composer and canonical pipeline."""
import hashlib
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from scripts.asset_layout import METROID_SAMUS_RUNTIME
from scripts.mzm_samus_compose import (
    DRAW_Y_OFFSET,
    Variant,
    body_frame_count,
    build_variants,
    parse_symbols,
    parse_tables,
    render_indices,
    to_bmp,
    verify_symbols_against_rom,
)
from scripts.mzm_samus_pipeline import INDEX_SCHEMA, build_animation_map, produce

TABLES = """
const struct SamusAnimationData* const sSamusAnimPointers_PowerSuit[SPOSE_COUNT][2] = {
    [SPOSE_SPINNING] = {
        sSamusAnim_PowerSuit_Right_Spinning,
        sSamusAnim_PowerSuit_Left_Spinning
    },
};
const struct SamusAnimationData* const sSamusAnimPointers_PowerSuit_Running[4][2] = {
    [ACD_DIAGONALLY_UP] = {
        sSamusAnim_PowerSuit_Right_DiagonalUp_Running,
        sSamusAnim_PowerSuit_Left_DiagonalUp_Running
    },
};
const struct SamusAnimationData* const sSamusAnimPointers_PowerSuit_Skidding[2][2] = {
    [FALSE] = {
        sSamusAnim_PowerSuit_Right_Skidding,
        sSamusAnim_PowerSuit_Left_Skidding
    },
};
const struct SamusAnimationData* const sSamusAnimPointers_PowerSuit_AimingWhileHanging[5][2] = {
    [ACD_UP] = {
        sSamusAnim_PowerSuit_Right_Up_AimingWhileHanging,
        sSamusAnim_PowerSuit_Left_Up_AimingWhileHanging
    },
};
const struct SamusAnimationData* const sSamusAnimPointers_Suitless_CrawlingStopped[3][2] = {
    [TRUE] = {
        sSamusAnim_Suitless_Right_CrawlingStopped,
        sSamusAnim_Suitless_Left_CrawlingStopped
    },
};
const struct ArmCannonAnimationData* const sArmCannonAnimPointers_Suit_All[SPOSE_COUNT][2] = {
    [SPOSE_SPINNING] = {
        sArmCannonAnim_Suit_Right_Spinning,
        sArmCannonAnim_Suit_Left_Spinning
    },
    [SPOSE_SKIDDING] = {
        sArmCannonAnim_Suit_Right_Skidding,
        sArmCannonAnim_Suit_Left_Skidding
    },
};
const struct ArmCannonAnimationData* const sArmCannonAnimPointers_Suit_Running[4][2] = {
    [ACD_DIAGONALLY_UP] = {
        sArmCannonAnim_Suit_Right_DiagonalUp_Running,
        sArmCannonAnim_Suit_Left_DiagonalUp_Running
    },
};
const struct ArmCannonAnimationData* const sArmCannonAnimPointers_Suit_AimingWhileHanging[5][2] = {
    [ACD_UP] = {
        sArmCannonAnim_Suit_Right_Up_AimingWhileHanging,
        sArmCannonAnim_Suit_Left_Up_AimingWhileHanging
    },
};
const struct ArmCannonAnimationData* const sArmCannonAnimPointers_Suitless_CrawlingStopped[3][2] = {
    [FORCED_MOVEMENT_CRAWLING_ARM_CANNON_UP] = {
        sArmCannonAnim_Suitless_Right_CrawlingStopped_Up,
        sArmCannonAnim_Suitless_Left_CrawlingStopped_Up
    },
};
const u8* const sArmCannonGfxPointers_Upper_Standing[5] = {
    [ACD_FORWARD] = sArmCannonGfx_Upper_Forward_Standing,
    [ACD_DIAGONALLY_UP] = sArmCannonGfx_Upper_DiagonalUp_Standing,
};
const u8* const sArmCannonGfxPointers_Lower_Standing[5] = {
    [ACD_FORWARD] = sArmCannonGfx_Lower_Forward_Standing,
    [ACD_DIAGONALLY_UP] = sArmCannonGfx_Lower_DiagonalUp_Standing,
};
const u8* const sArmCannonGfxPointers_Upper_Left_Default[5] = {
    [ACD_FORWARD] = sArmCannonGfx_Upper_Forward_Left_Default,
    [ACD_DIAGONALLY_UP] = sArmCannonGfx_Upper_DiagonalUp_Left_Default,
};
const u8* const sArmCannonGfxPointers_Lower_Left_Default[5] = {
    [ACD_FORWARD] = sArmCannonGfx_Lower_Forward_Left_Default,
    [ACD_DIAGONALLY_UP] = sArmCannonGfx_Lower_DiagonalUp_Left_Default,
};
const u8* const sArmCannonGfxPointers_Upper_Right_Default[5] = {
    [ACD_FORWARD] = sArmCannonGfx_Upper_Forward_Right_Default,
};
const u8* const sArmCannonGfxPointers_Lower_Right_Default[5] = {
    [ACD_FORWARD] = sArmCannonGfx_Lower_Forward_Right_Default,
};
const u8* const sArmCannonGfxPointers_Upper_Right_Hanging[5] = {
    [ACD_UP] = sArmCannonGfx_Upper_Up_Right_Hanging,
};
const u8* const sArmCannonGfxPointers_Lower_Right_Hanging[5] = {
    [ACD_UP] = sArmCannonGfx_Lower_Up_Right_Hanging,
};
"""


def variants_by_key():
    variants, problems = build_variants(parse_tables(TABLES))
    return {(v.family, v.table, v.selector, v.side): v for v in variants}, problems


class ComposerTableTests(unittest.TestCase):
    def test_body_cannon_and_graphics_follow_native_selection(self):
        variants, problems = variants_by_key()
        running_right = variants[("PowerSuit", "Running", "ACD_DIAGONALLY_UP", "right")]
        self.assertEqual(running_right.cannon,
                         "sArmCannonAnim_Suit_Right_DiagonalUp_Running")
        # Running right uses the Standing cannon graphics, left the default.
        self.assertEqual(running_right.cannon_gfx,
                         ("sArmCannonGfx_Upper_DiagonalUp_Standing",
                          "sArmCannonGfx_Lower_DiagonalUp_Standing"))
        running_left = variants[("PowerSuit", "Running", "ACD_DIAGONALLY_UP", "left")]
        self.assertEqual(running_left.cannon_gfx[0],
                         "sArmCannonGfx_Upper_DiagonalUp_Left_Default")
        # Pose tables pair with ..._All[pose]; non-ACD selectors use forward.
        spinning = variants[("PowerSuit", "pose", "SPOSE_SPINNING", "right")]
        self.assertEqual(spinning.cannon, "sArmCannonAnim_Suit_Right_Spinning")
        self.assertEqual(spinning.cannon_gfx[0], "sArmCannonGfx_Upper_Forward_Right_Default")
        skidding = variants[("PowerSuit", "Skidding", "FALSE", "left")]
        self.assertEqual(skidding.cannon, "sArmCannonAnim_Suit_Left_Skidding")
        hanging = variants[("PowerSuit", "AimingWhileHanging", "ACD_UP", "right")]
        self.assertEqual(hanging.cannon_gfx, ("sArmCannonGfx_Upper_Up_Right_Hanging",
                                              "sArmCannonGfx_Lower_Up_Right_Hanging"))
        # Differently spelled selectors with equal values still pair.
        crawling = variants[("Suitless", "CrawlingStopped", "TRUE", "right")]
        self.assertEqual(crawling.cannon, "sArmCannonAnim_Suitless_Right_CrawlingStopped_Up")
        self.assertEqual(problems, [])

    def test_symbols_are_filtered_and_conflicts_rejected(self):
        output = ("08230000 00000040 r sArmCannonGfx_Upper_Forward_Standing\n"
                  "08240000 00000020 r sSamusAnim_PowerSuit_Right_Spinning\n"
                  "08000000 00000004 t _start\n")
        symbols = parse_symbols(output)
        self.assertEqual(symbols["sSamusAnim_PowerSuit_Right_Spinning"], (0x08240000, 0x20))
        self.assertNotIn("_start", symbols)
        with self.assertRaisesRegex(ValueError, "conflicting"):
            parse_symbols(output + "08240010 00000020 r sSamusAnim_PowerSuit_Right_Spinning\n")


def synthetic_rom(cannon_header):
    rom = bytearray(0x1000)
    # Body frame: upper/lower graphics, OAM, duration 5, then a terminator.
    struct.pack_into("<IIIB", rom, 0x100, 0x08000200, 0x08000300, 0x08000400, 5)
    rom[0x200:0x202] = bytes((1, 0))
    rom[0x202 + 2] = 0x01          # tile 0, pixel x=4, y=0 -> index 1
    rom[0x300:0x302] = bytes((0, 0))
    # One 8x8 OBJ at x=-4, y=-8 using tile 0, bank 0.
    struct.pack_into("<HHHH", rom, 0x400, 1, 0xF8, 0x1FC, 0)
    # Arm cannon record and OAM: one 8x8 OBJ at x=0, y=-8 using tile 64.
    struct.pack_into("<II", rom, 0x500, 0x08000600, 0x08000700)
    struct.pack_into("<HHHH", rom, 0x700, cannon_header, 0xF8, 0, 64)
    rom[0x800] = 0x02              # upper cannon tile, pixel x=0 -> index 2
    return bytes(rom)


SYMBOLS = {"body": (0x08000100, 32), "cannon": (0x08000500, 8),
           "upper": (0x08000800, 64), "lower": (0x08000840, 64)}
VARIANT = Variant("PowerSuit", "Standing", "ACD_FORWARD", "right", "body",
                  "cannon", ("upper", "lower"), None)


class ComposerRenderTests(unittest.TestCase):
    def test_cannon_in_front_wins_and_offsets_are_native(self):
        rom = synthetic_rom(0x1001)
        self.assertEqual(body_frame_count(rom, 0x08000100, 32), [5])
        pixels, duration, parts = render_indices(rom, SYMBOLS, VARIANT, 0)
        self.assertEqual((duration, parts), (5, 1))
        self.assertEqual(pixels, {(0, -8): 2})

    def test_cannon_behind_is_covered_by_the_body(self):
        pixels, _, _ = render_indices(synthetic_rom(0x2001), SYMBOLS, VARIANT, 0)
        self.assertEqual(pixels, {(0, -8): 1})

    def test_dying_pose_never_draws_the_cannon(self):
        dying = Variant("PowerSuit", "pose", "SPOSE_DYING", "right", "body",
                        "cannon", ("upper", "lower"), "SPOSE_DYING")
        pixels, _, parts = render_indices(synthetic_rom(0x1001), SYMBOLS, dying, 0)
        self.assertEqual((pixels, parts), ({(0, -8): 1}, 0))

    def test_bmp_is_cropped_with_its_oam_origin(self):
        colors = [(0, 0, 0)] * 32
        colors[3] = (10, 20, 30)
        bmp, left, top = to_bmp({(-5, -9): 3, (-4, -9): 3}, colors)
        self.assertEqual((left, top), (-5, -9))
        width, height = struct.unpack_from("<ii", bmp, 18)
        self.assertEqual((width, height), (2, 1))
        self.assertEqual(bmp[122:126], bytes((30, 20, 10, 255)))
        self.assertEqual(DRAW_Y_OFFSET, 2)


class ReferenceElfTests(unittest.TestCase):
    def test_only_consumed_symbol_ranges_must_match(self):
        rom = bytes(range(256)) * 4
        image = bytearray(rom)
        image[0] = 0xEE                      # header-like difference is ignored
        symbols = {"sSamusPal_PowerSuit_Default": (0x08000010, 32)}
        verify_symbols_against_rom(rom, bytes(image), symbols)
        image[0x20] ^= 1
        with self.assertRaisesRegex(ValueError, "sSamusPal_PowerSuit_Default"):
            verify_symbols_against_rom(rom, bytes(image), symbols)


class PipelineTests(unittest.TestCase):
    def test_animation_map_uses_native_keys_for_every_visual_suit(self):
        keys = ["VariaSuit/Standing/ACD_FORWARD/left",
                "VariaSuit/Standing/ACD_UP/left",
                "GravitySuit/pose/SPOSE_SPINNING/right",
                "PowerSuit/ScrewAttacking/TRUE/right"]
        content, report = build_animation_map(keys)
        self.assertIn("idle\tVariaSuit\tleft\tforward\tloop\t"
                      "VariaSuit/Standing/ACD_FORWARD/left\n", content)
        self.assertIn("idle\tVariaSuit\tleft\tup\tloop\tVariaSuit/Standing/ACD_UP/left\n",
                      content)
        self.assertIn("spin\tGravitySuit\tright\tnone\tloop\t"
                      "GravitySuit/pose/SPOSE_SPINNING/right\n", content)
        self.assertIn("screw_attack_space\tPowerSuit\tright\tnone\tloop\t"
                      "PowerSuit/ScrewAttacking/TRUE/right\n", content)
        self.assertIn("spin/Suitless/left/none", report["missing"])
        self.assertEqual(report["graphics_by_suit"]["GravitySuit"], "FullSuit")
        self.assertEqual(report["palette_by_suit"]["VariaSuit"],
                         "sSamusPal_VariaSuit_Default")

    def test_produce_deduplicates_prunes_and_is_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rom = root / "rom.gba"
            elf = root / "mzm.elf"
            decomp = root / "mzm/src/data/samus"
            rom.write_bytes(b"rom-fixture")
            elf.write_bytes(b"elf")
            decomp.mkdir(parents=True)
            (decomp / "samus_animation_pointers.c").write_text(TABLES, encoding="utf-8")
            objects = root / METROID_SAMUS_RUNTIME / "objects"
            objects.mkdir(parents=True)
            (objects / ("0" * 64 + ".bmp")).write_bytes(b"BMstale")

            def fake_compose(_rom, _tables, _symbols, sink):
                frame = (b"BMframe", 4, -10, -34)
                sink("PowerSuit/Standing/ACD_FORWARD/right", [frame, frame], {})
                sink("VariaSuit/Standing/ACD_FORWARD/right", [frame], {})
                return {"variants": 1, "sequences": 2, "frames": 3,
                        "unresolved": {}, "table_problems": []}

            sha1 = hashlib.sha1(rom.read_bytes()).hexdigest()
            with mock.patch("scripts.mzm_samus_pipeline.EXPECTED_SHA1", sha1), \
                    mock.patch("scripts.mzm_samus_pipeline._run_nm", return_value=""), \
                    mock.patch("scripts.mzm_samus_pipeline._elf_image",
                               return_value=b""), \
                    mock.patch("scripts.mzm_samus_pipeline.compose_all", fake_compose):
                result = produce(root, rom, elf, root / "mzm")
                repeated = produce(root, rom, elf, root / "mzm")
            self.assertEqual(result["unique_bmps"], 1)
            self.assertEqual(result["removed_stale_objects"], 1)
            self.assertEqual(repeated["removed_stale_objects"], 0)
            index = (root / METROID_SAMUS_RUNTIME / "runtime_index.tsv").read_text()
            lines = index.splitlines()
            self.assertEqual(lines[0], "schema\t" + INDEX_SCHEMA)
            self.assertEqual(len(lines), 4)
            self.assertTrue(lines[1].startswith(
                "PowerSuit/Standing/ACD_FORWARD/right\t0\t4\t-10\t-34\t"
                "assets/extracted/metroid/sprites/samus/runtime/objects/"))
            self.assertEqual(len(list(objects.glob("*.bmp"))), 1)

    def test_produce_rejects_unresolved_variants(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rom = root / "rom.gba"
            elf = root / "mzm.elf"
            decomp = root / "mzm/src/data/samus"
            rom.write_bytes(b"rom-fixture")
            elf.write_bytes(b"elf")
            decomp.mkdir(parents=True)
            (decomp / "samus_animation_pointers.c").write_text(TABLES, encoding="utf-8")
            report = {"variants": 1, "sequences": 0, "frames": 0,
                      "unresolved": {"PowerSuit/x": "bad"}, "table_problems": []}
            sha1 = hashlib.sha1(rom.read_bytes()).hexdigest()
            with mock.patch("scripts.mzm_samus_pipeline.EXPECTED_SHA1", sha1), \
                    mock.patch("scripts.mzm_samus_pipeline._run_nm", return_value=""), \
                    mock.patch("scripts.mzm_samus_pipeline._elf_image",
                               return_value=b""), \
                    mock.patch("scripts.mzm_samus_pipeline.compose_all",
                               return_value=report):
                with self.assertRaisesRegex(ValueError, "unresolved"):
                    produce(root, rom, elf, root / "mzm")


if __name__ == "__main__":
    unittest.main()
