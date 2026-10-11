# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests of the Zero Mission sprite placements and data parsing."""
from pathlib import Path
import unittest

from scripts import mzm_sprites

DECOMP = Path(__file__).resolve().parents[1] / "third_party/mzm"


@unittest.skipUnless((DECOMP / "src/sprite.c").is_file(), "pinned MZM decompilation missing")
class MzmSpriteDataTests(unittest.TestCase):
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
                   "PSPRITE_POWER_BOMB_DROP": 30}
        for name, value in runtime.items():
            self.assertEqual(ids[name], value)
        self.assertEqual(set(mzm_sprites.DROPS), set(runtime))
        source = (DECOMP / "src/data/sprites/enemy_drop.c").read_text(encoding="utf-8")
        tables = mzm_sprites.frame_tables(source)
        for table in mzm_sprites.DROPS.values():
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
            self.assertRegex(sources, rf"const\s+s16\s+{table}\s*\[")
        for table in mzm_sprites.PARTICLES:
            self.assertRegex(sources, rf"const\s+struct\s+FrameData\s+{table}\s*\[")


if __name__ == "__main__":
    unittest.main()
