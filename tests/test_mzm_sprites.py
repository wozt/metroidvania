# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests of the Zero Mission sprite placements and data parsing."""
from pathlib import Path
import unittest
from unittest import mock

from scripts import mzm_projectile_compose, mzm_sprites

DECOMP = Path(__file__).resolve().parents[1] / "third_party/mzm"


@unittest.skipUnless((DECOMP / "src/sprite.c").is_file(), "pinned MZM decompilation missing")
class MzmSpriteDataTests(unittest.TestCase):
    def test_oam_bounds_keep_transparent_edges_and_follow_global_flips(self):
        entries = [
            {"x": -5, "y": -4, "w": 8, "h": 8},
            {"x": 4, "y": -2, "w": 16, "h": 8},
        ]
        with mock.patch.object(mzm_projectile_compose, "oam_entries",
                               return_value=entries):
            self.assertEqual(mzm_projectile_compose.object_bounds(b"", 0, "none"),
                             (-5, -4, 25, 10, 2))
            self.assertEqual(mzm_projectile_compose.object_bounds(b"", 0, "x"),
                             (-20, -4, 25, 10, 2))
            self.assertEqual(mzm_projectile_compose.object_bounds(b"", 0, "y"),
                             (-5, -6, 25, 10, 2))

    def test_affine_whole_part_ignores_native_y_flip(self):
        entry = {"x": -5, "y": 6, "w": 8, "h": 8, "tile": 0, "bank": 0,
                 "hflip": False, "vflip": True, "priority": 0}
        vram = bytes([0x01]) + bytes(31)
        palette = [None] * 256
        palette[1] = (1, 2, 3)
        with mock.patch.object(mzm_projectile_compose, "oam_entries",
                               return_value=[entry]):
            pixels = mzm_projectile_compose.render_affine_whole_part(
                b"", vram, palette, 0, 0)
            self.assertIn((-5, 6), pixels)
            self.assertNotIn((-5, 13), pixels)
            self.assertEqual(
                mzm_projectile_compose.affine_whole_part_bounds(b"", 0, 0),
                (-5, 6, 8, 8, 1))

    def test_psprite_enum_matches_the_runtime_ids(self):
        ids = mzm_sprites.psprite_ids(
            (DECOMP / "include/constants/sprite.h").read_text(encoding="utf-8"))
        self.assertEqual(ids["PSPRITE_UNUSED16"], 16)
        self.assertEqual(ids["PSPRITE_ZOOMER_YELLOW"], 18)
        self.assertEqual(ids["PSPRITE_ZOOMER_RED"], 19)

    def test_room_sprites_resolve_spriteset_slots(self):
        sprites = mzm_sprites.room_sprites("Brinstar", 0, DECOMP)
        names = [item["sprite"] for item in sprites]
        self.assertEqual(names.count("PSPRITE_ZOOMER_YELLOW"), 6)
        self.assertIn("PSPRITE_MORPH_BALL", names)
        first = sprites[0]
        self.assertEqual((first["y"], first["x"], first["slot"], first["gfx_slot"]),
                         (22, 27, 17, 0))
        launcher = next(item for item in sprites if item["sprite"] == "PSPRITE_MORPH_BALL_LAUNCHER")
        self.assertEqual(launcher["gfx_slot"], 4)

    def test_room_event_variants_resolve_placements_and_priority(self):
        variants = mzm_sprites.room_sprite_variants("Brinstar", 9, DECOMP)
        self.assertEqual([(variant["variant"], variant["event"], variant["spriteset"])
                          for variant in variants], [(0, 0, 94), (1, 42, 94)])
        self.assertEqual(variants[0]["sprites"], [])
        self.assertEqual([sprite["sprite"] for sprite in variants[1]["sprites"]],
                         ["PSPRITE_DESSGEEGA_AFTER_LONG_BEAM"])

        # RoomLoadEntry checks the second event before the first one. Both
        # choices are exported, including their different room placements.
        variants = mzm_sprites.room_sprite_variants("Brinstar", 2, DECOMP)
        self.assertEqual([(variant["variant"], variant["event"])
                          for variant in variants], [(0, 0), (1, 37), (2, 2)])
        self.assertEqual(len(variants[2]["sprites"]), 6)

        statue_room = mzm_sprites.room_sprite_variants("Brinstar", 10, DECOMP)[0]
        self.assertEqual([sprite["sprite"] for sprite in statue_room["sprites"]],
                         ["PSPRITE_RIDLEY_STATUE", "PSPRITE_KRAID_STATUE"])
        self.assertIn("PSPRITE_KRAID_STATUE", mzm_sprites.PORTED)
        self.assertIn("PSPRITE_RIDLEY_STATUE", mzm_sprites.PORTED)

    def test_zoomer_data_file_and_tables(self):
        sprite_c = (DECOMP / "src/sprite.c").read_text(encoding="utf-8")
        graphics = mzm_sprites.pointer_table(sprite_c, "sSpritesGraphicsPointers")
        palettes = mzm_sprites.pointer_table(sprite_c, "sSpritesPalettePointers")
        self.assertEqual(graphics["PSPRITE_ZOOMER_RED"], "sZoomerGfx")
        self.assertEqual(palettes["PSPRITE_ZOOMER_YELLOW"], "sZoomerPal")
        source = mzm_sprites.data_file(DECOMP, "sZoomerGfx").read_text(encoding="utf-8")
        tables = mzm_sprites.frame_tables(source)
        for name in ("sZoomerOam_OnGround", "sZoomerOam_OnWall", "sZoomerOam_Falling",
                     "sZoomerOam_TurningEdgeLeft", "sZoomerOam_WalkingOnLeftSlope"):
            self.assertIn(name, tables)
        self.assertEqual(len(tables), 9)

    def test_drop_tables_and_ids_match_the_runtime(self):
        ids = mzm_sprites.psprite_ids(
            (DECOMP / "include/constants/sprite.h").read_text(encoding="utf-8"))
        runtime = {"PSPRITE_LARGE_ENERGY_DROP": 26, "PSPRITE_SMALL_ENERGY_DROP": 27,
                   "PSPRITE_MISSILE_DROP": 28, "PSPRITE_SUPER_MISSILE_DROP": 29,
                   "PSPRITE_POWER_BOMB_DROP": 30,
                   "PSPRITE_MULTIPLE_LARGE_ENERGY": 53}
        for name, value in runtime.items():
            self.assertEqual(ids[name], value)
        self.assertEqual(set(mzm_sprites.DROPS), set(runtime))
        source = (DECOMP / "src/data/sprites/enemy_drop.c").read_text(encoding="utf-8")
        tables = mzm_sprites.frame_tables(source)
        for drop_tables in mzm_sprites.DROPS.values():
            for table in drop_tables:
                self.assertIn(table, tables)
        hud = (DECOMP / "src/hud_generic.c").read_text(encoding="utf-8")
        self.assertIn("sCommonSpritesGfx, VRAM_OBJ + 0x800", hud)
        self.assertEqual(mzm_sprites.COMMON_GFX_VRAM, 0x800)
        in_game = (DECOMP / "src/in_game.c").read_text(encoding="utf-8")
        self.assertIn("sCommonSpritesPal, PALRAM_BASE + 0x240", in_game)
        self.assertEqual(0x200 + mzm_sprites.COMMON_PALETTE_ROW * 32, 0x240)


    def test_hopper_ids_tables_and_ai_tables_match_the_runtime(self):
        ids = mzm_sprites.psprite_ids(
            (DECOMP / "include/constants/sprite.h").read_text(encoding="utf-8"))
        self.assertEqual((ids["PSPRITE_DESSGEEGA"], ids["PSPRITE_DESSGEEGA_AFTER_LONG_BEAM"],
                          ids["PSPRITE_SIDEHOPPER"]), (70, 71, 121))
        for sprite in ("PSPRITE_SIDEHOPPER", "PSPRITE_DESSGEEGA",
                       "PSPRITE_DESSGEEGA_AFTER_LONG_BEAM"):
            self.assertIn(sprite, mzm_sprites.PORTED)
        for name, path in (("sSidehopperGfx", "src/data/sprites/sidehopper.c"),
                           ("sDessgeegaGfx", "src/data/sprites/dessgeega.c")):
            self.assertEqual(mzm_sprites.data_file(DECOMP, name), DECOMP / path)
        tables = mzm_sprites.frame_tables(
            (DECOMP / "src/data/sprites/sidehopper.c").read_text(encoding="utf-8"))
        for name in ("sSidehopperOam_Idle", "sSidehopperOam_ShakingHead",
                     "sSidehopperOam_JumpWarning", "sSidehopperOam_Jumping",
                     "sSidehopperOam_Landing"):
            self.assertIn(name, tables)
        tables = mzm_sprites.frame_tables(
            (DECOMP / "src/data/sprites/dessgeega.c").read_text(encoding="utf-8"))
        for name in ("sDessGeegaOam_Idle", "sDessGeegaOam_Screaming",
                     "sDessGeegaOam_JumpWarning", "sDessGeegaOam_Jumping",
                     "sDessGeegaOam_Landing"):
            self.assertIn(name, tables)
        # Every AI table is defined in the decompilation's data files.
        sources = "".join(path.read_text(encoding="utf-8")
                          for path in sorted((DECOMP / "src/data").rglob("*.c")))
        for table in mzm_sprites.AI_TABLES:
            self.assertRegex(sources, rf"const\s+[su]16\s+{table}\s*\[")
        for table in mzm_sprites.BYTE_PAIR_TABLES:
            self.assertRegex(sources, rf"const\s+u8\s+{table}\s*\[\d+\]\[2\]")
        for table in mzm_sprites.PARTICLES:
            self.assertRegex(sources, rf"const\s+struct\s+FrameData\s+{table}\s*\[")


class MzmMechaRidleyWeaponTests(unittest.TestCase):
    def test_index_palette_shifts_rows_and_skips_unloaded_ones(self):
        raw = [None] * 256
        for index in range(128, 256):
            raw[index] = index
        base = mzm_sprites.index_palette(raw, 0)
        self.assertEqual(base[8 * 16 + 5], 8 * 16 + 5)
        self.assertIsNone(base[5])
        tinted = mzm_sprites.index_palette(raw, 4)
        self.assertEqual(tinted[8 * 16 + 1], 12 * 16 + 1)
        # Bank 12 + 4 wraps to row 0, which Mecha Ridley does not load.
        self.assertIsNone(tinted[12 * 16 + 1])

    def test_destroyed_graphics_take_the_seven_native_copies(self):
        destroyed = bytes(range(256)) * 6
        vram = mzm_sprites.mecha_ridley_destroyed_vram(bytes(0x8000), destroyed[:1344])
        for source, target, size in mzm_sprites.MECHA_RIDLEY_DESTROYED_COPIES:
            self.assertEqual(vram[target:target + size], destroyed[source:source + size])
        self.assertEqual(mzm_sprites.MECHA_RIDLEY_DESTROYED_COPIES[-1], (1216, 0x5580 + 6144 + 64, 128))

    def test_weapon_stages_copy_four_tile_rows_to_0x4280(self):
        weapons = bytes(range(256)) * 16
        vram = mzm_sprites.mecha_ridley_weapon_vram(bytes(0x8000), weapons, 12 + 4 * 2)
        for row in range(4):
            target = 0x4280 + row * 1024
            source = row * 1024 + 20 * 32
            self.assertEqual(vram[target:target + 128], weapons[source:source + 128])
        self.assertEqual(vram[0x4280 + 128], 0)
        self.assertEqual(mzm_sprites.MECHA_RIDLEY_WEAPON_STAGES, (0, 4, 8, 12, 16, 20, 24))

    def test_weapon_tables_exist_in_the_decomp(self):
        source = (mzm_sprites.DECOMP / "src/data/sprites/mecha_ridley.c").read_text(encoding="utf-8")
        tables = mzm_sprites.frame_tables(source)
        for table in mzm_sprites.MECHA_RIDLEY_WEAPON_TABLES:
            self.assertIn(table, tables)
        self.assertIn("const u32 sMechaRidleyWeaponsGfx[32 * 32]", source)


class MzmRuinsTestReflectionTests(unittest.TestCase):
    def test_reflection_graphics_replace_the_two_native_vram_ranges(self):
        base = bytes([0x33]) * 0x8000
        top = bytes([0x55]) * 384
        bottom = bytes([0x77]) * 256
        staged = mzm_sprites.ruins_test_reflection_vram(base, top, bottom)
        self.assertEqual(staged[0x4280:0x4280 + 384], top)
        self.assertEqual(staged[0x4680:0x4680 + 256], bottom)
        self.assertEqual(staged[0x4280 - 1], 0x33)
        self.assertEqual(staged[0x4280 + 384], 0x33)
        self.assertEqual(staged[0x4680 - 1], 0x33)
        self.assertEqual(staged[0x4680 + 256], 0x33)

    def test_reflection_sources_and_full_suit_palette_exist(self):
        source = (mzm_sprites.DECOMP / "src/data/sprites/ruins_test.c").read_text(
            encoding="utf-8")
        self.assertIn(f"const u16 {mzm_sprites.RUINS_TEST_REFLECTION_FULL_PAL}[15 * 16]", source)
        self.assertRegex(source, rf"const struct FrameData "
                         rf"{mzm_sprites.RUINS_TEST_REFLECTION_TABLE}\[\d+\]")
        for pair in mzm_sprites.RUINS_TEST_REFLECTION_GFX.values():
            for symbol in pair:
                self.assertRegex(source, rf"const u32 {symbol}\[\d+\]")


class MzmSpritePaletteVariantTests(unittest.TestCase):
    def test_frozen_and_stun_rows_shift_the_sprite_rows(self):
        palette = [None] * 256
        common = [None] * 256
        for row in range(8, 16):
            for index in range(16):
                palette[row * 16 + index] = ("sprite", row, index)
        for row in range(2, 8):
            for index in range(16):
                common[row * 16 + index] = ("common", row, index)
        variants = mzm_sprites.shifted_palettes(palette, common)
        frozen, stun = variants["/frozen"], variants["/stun"]
        # A row 8 part shows OBJ row 7 (frozen) or 6 (stun): common rows.
        self.assertEqual(frozen[8 * 16 + 3], ("common", 7, 3))
        self.assertEqual(stun[8 * 16 + 3], ("common", 6, 3))
        # A row 9 part shows row 8 (frozen) or row 7 (stun).
        self.assertEqual(frozen[9 * 16], ("sprite", 8, 0))
        self.assertEqual(stun[9 * 16], ("common", 7, 0))
        # Rows below 8 are untouched.
        self.assertEqual(frozen[:128], palette[:128])


if __name__ == "__main__":
    unittest.main()
