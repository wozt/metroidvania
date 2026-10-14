# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests of the Zero Mission runtime room encoding."""
from pathlib import Path
import unittest

from scripts import mzm_runtime_room

DECOMP = Path(__file__).resolve().parents[1] / "third_party/mzm"


@unittest.skipUnless((DECOMP / "src/data/clipdata_data.c").is_file(),
                     "pinned MZM decompilation missing")
class MzmRuntimeRoomTests(unittest.TestCase):
    def test_movement_table_follows_the_clip_behaviors(self):
        tables = mzm_runtime_room.collision_tables(DECOMP)
        behaviors = tables["behaviors"]
        movement = tables["movement"]
        # sMovementClipdataValues: CLIPDATA_MOVEMENT_SPACE_PIRATE_ZONELINE (8)
        # and ..._WALLJUMP_POINT (9); the transitions move nothing.
        self.assertEqual(movement[behaviors["CLIP_BEHAVIOR_SPACE_PIRATE_ZONELINE"]], 8)
        self.assertEqual(movement[behaviors["CLIP_BEHAVIOR_SPACE_PIRATE_WALLJUMP_POINT"]], 9)
        self.assertEqual(movement[behaviors["CLIP_BEHAVIOR_STOP_ENEMY_SOLID"]], 7)
        self.assertEqual(movement[behaviors["CLIP_BEHAVIOR_DOOR_TRANSITION"]], 0)

    def test_pirate_blocks_export_movement_lines(self):
        tables = mzm_runtime_room.collision_tables(DECOMP)
        behaviors = tables["behaviors"]
        raw = {mzm_runtime_room.clip_behavior(value, tables): value for value in range(64)}
        zoneline = raw[behaviors["CLIP_BEHAVIOR_SPACE_PIRATE_ZONELINE"]]
        wall_jump = raw[behaviors["CLIP_BEHAVIOR_SPACE_PIRATE_WALLJUMP_POINT"]]
        cells = (zoneline, 0) + (0, wall_jump)
        lines = mzm_runtime_room.encode_room("Chozodia", 2, 32, 32, (2, 2, cells),
                                             tables).splitlines()
        self.assertIn("M\t0\t0\t8", lines)
        self.assertIn("M\t1\t1\t9", lines)
        self.assertEqual(sum(line.startswith("M\t") for line in lines), 2)

    def test_spriteset_lines_follow_the_room_spriteset(self):
        tables = mzm_runtime_room.collision_tables(DECOMP)
        lines = mzm_runtime_room.encode_room(
            "Chozodia", 2, 16, 16, (1, 1, (0,)), tables,
            spriteset=(53, [("PSPRITE_SPACE_PIRATE", 0), ("PSPRITE_SEARCHLIGHT", 4)])
        ).splitlines()
        self.assertEqual(lines[-3:], ["S\t53\tSPACE_PIRATE\t0", "S\t53\tSEARCHLIGHT\t4", "END"])

    def test_doors_export_their_own_exit(self):
        # RoomLoad places Samus by the used door's own xExit / yExit (door 0
        # at a new game's start), exported as X lines.
        tables = mzm_runtime_room.collision_tables(DECOMP)
        doors = [{"kind": "area", "sourceRoom": 0, "xStart": 38, "xEnd": 40, "yStart": 2,
                  "yEnd": 2, "destinationDoor": 11, "xExit": 16, "yExit": 16},
                 {"kind": "hatch", "sourceRoom": 1, "xStart": 2, "xEnd": 2, "yStart": 15,
                  "yEnd": 18, "destinationDoor": 0, "xExit": -32, "yExit": 0}]
        lines = mzm_runtime_room.encode_room("Brinstar", 0, 16, 16, (1, 1, (0,)), tables,
                                             doors=doors).splitlines()
        self.assertIn("X\t0\t16\t16", lines)
        self.assertFalse(any(line.startswith("X\t1\t") for line in lines))

    def test_room_spriteset_is_the_default_one(self):
        from scripts import mzm_sprites
        index, members = mzm_sprites.room_spriteset("Chozodia", 2, DECOMP)
        self.assertEqual(index, 53)
        self.assertEqual(members[:2], [("PSPRITE_SPACE_PIRATE", 0),
                                       ("PSPRITE_SPACE_PIRATE_WAITING1", 0)])


if __name__ == "__main__":
    unittest.main()
