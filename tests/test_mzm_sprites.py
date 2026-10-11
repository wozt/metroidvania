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


if __name__ == "__main__":
    unittest.main()
