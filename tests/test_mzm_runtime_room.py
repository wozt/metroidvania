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


if __name__ == "__main__":
    unittest.main()
