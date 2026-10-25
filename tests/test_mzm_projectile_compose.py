# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for the shared FrameData and projectile render helpers."""
import struct
import unittest

from scripts import mzm_projectile_compose as projectiles


def synthetic_rom():
    rom = bytearray(0x2000)
    # FrameData table at 0x100: one frame (OAM at 0x200, 3 ticks) + terminator.
    struct.pack_into("<IB", rom, 0x100, 0x08000200, 3)
    # One 8x8 OBJ at x=-6, y=-4, tile 0x80 (OBJ offset 0x1000), bank 2.
    struct.pack_into("<HHHH", rom, 0x200, 1, 0xFC, 0x1FA, 0x2080)
    return bytes(rom)


class ProjectileTableTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
