# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for projectile sprite composition and Samus weapon data."""
import struct
import unittest

from scripts import mzm_projectile_compose as projectiles
from scripts.mzm_samus_compose import Variant, _cannon_gfx, muzzle_offset
from scripts.mzm_samus_pipeline import cannon_offsets

SOURCE = """
const struct FrameData sNormalBeamOam_Horizontal[3] = {
const struct FrameData sNormalBeamOam_Horizontal_Unused[3] = {
const struct FrameData sChargedWaveBeamOam_Diagonal[8] = {
const struct FrameData sMissileOam_Vertical[3] = {
const struct FrameData sParticleMissileTrailOam[8] = {
"""


def synthetic_rom():
    rom = bytearray(0x2000)
    # FrameData table at 0x100: one frame (OAM at 0x200, 3 ticks) + terminator.
    struct.pack_into("<IB", rom, 0x100, 0x08000200, 3)
    # One 8x8 OBJ at x=-6, y=-4, tile 0x80 (OBJ offset 0x1000), bank 2.
    struct.pack_into("<HHHH", rom, 0x200, 1, 0xFC, 0x1FA, 0x2080)
    return bytes(rom)


class ProjectileTableTests(unittest.TestCase):
    def test_tables_skip_particles_and_unused_data(self):
        self.assertEqual(projectiles.frame_tables(SOURCE),
                         ["sNormalBeamOam_Horizontal", "sChargedWaveBeamOam_Diagonal",
                          "sMissileOam_Vertical"])
        self.assertEqual(projectiles.beam_set("sChargedWaveBeamOam_Diagonal"), "WaveBeam")
        self.assertEqual(projectiles.beam_set("sPlasmaBeamOam_Vertical_Wave"), "PlasmaBeam")
        self.assertEqual(projectiles.beam_set("sMissileOam_Vertical"), "NormalBeam")

    def test_frames_stop_at_the_terminator(self):
        rom = synthetic_rom()
        symbols = {"sNormalBeamOam_Horizontal": (0x08000100, 24)}
        self.assertEqual(projectiles.frames(rom, symbols, "sNormalBeamOam_Horizontal"),
                         [(0x08000200, 3)])

    def test_flips_mirror_around_the_projectile_position(self):
        rom = synthetic_rom()
        vram = bytearray(0x8000)
        vram[0x1000] = 0x01            # tile 0x80, pixel (0, 0) -> index 1
        palette = [None] * 256
        palette[33] = (10, 20, 30)
        none = projectiles.render(rom, bytes(vram), palette, 0x08000200, "none")
        self.assertEqual(none, {(-6, -4): (10, 20, 30)})
        self.assertEqual(projectiles.render(rom, bytes(vram), palette, 0x08000200, "x"),
                         {(5, -4): (10, 20, 30)})
        self.assertEqual(projectiles.render(rom, bytes(vram), palette, 0x08000200, "xy"),
                         {(5, 3): (10, 20, 30)})
        palette[33] = None
        with self.assertRaisesRegex(ValueError, "unloaded palette"):
            projectiles.render(rom, bytes(vram), palette, 0x08000200, "none")


class ArmCannonDataTests(unittest.TestCase):
    TABLES = {
        "sArmCannonGfxPointers_Upper_Armed_Standing": {"ACD_FORWARD": ["upper_armed_run"]},
        "sArmCannonGfxPointers_Lower_Armed_Standing": {"ACD_FORWARD": ["lower_armed_run"]},
        "sArmCannonGfxPointers_Upper_Left_Armed_Default": {"ACD_FORWARD": ["upper_armed_l"]},
        "sArmCannonGfxPointers_Lower_Left_Armed_Default": {"ACD_FORWARD": ["lower_armed_l"]},
    }

    def test_armed_graphics_follow_the_native_rules(self):
        self.assertEqual(_cannon_gfx(self.TABLES, "SPOSE_RUNNING", "ACD_FORWARD",
                                     "right", armed=True),
                         ("upper_armed_run", "lower_armed_run"))
        self.assertEqual(_cannon_gfx(self.TABLES, "SPOSE_RUNNING", "ACD_FORWARD",
                                     "left", armed=True),
                         ("upper_armed_l", "lower_armed_l"))
        self.assertIsNone(_cannon_gfx(self.TABLES, None, "ACD_FORWARD", "right",
                                      armed=True))

    def test_muzzle_offsets_use_native_sign_rules(self):
        rom = bytearray(0x400)
        struct.pack_into("<II", rom, 0x100, 0x08000200, 0x08000300)
        struct.pack_into("<HH", rom, 0x200, 0xE5, 0x1EE)   # y=-27, x=-18
        symbols = {"cannon": (0x08000100, 8)}
        self.assertEqual(muzzle_offset(bytes(rom), symbols, "cannon", 0), (-18, -26))

    def test_cannon_offset_table_skips_armed_duplicates(self):
        text = cannon_offsets({
            "PowerSuit/Standing/ACD_FORWARD/right": {"muzzle": [[18, -26]]},
            "PowerSuit/Standing/ACD_FORWARD/right/armed": {"muzzle": [[18, -26]]},
            "PowerSuit/pose/SPOSE_MORPH_BALL/right": {"muzzle": None},
        })
        self.assertEqual(text.splitlines(), [
            "schema\tmetroidvania-samus-cannon-offsets-v1",
            "PowerSuit/Standing/ACD_FORWARD/right\t0\t18\t-26"])


if __name__ == "__main__":
    unittest.main()
