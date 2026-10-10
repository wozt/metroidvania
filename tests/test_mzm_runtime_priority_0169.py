# SPDX-License-Identifier: GPL-3.0-only
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Priority0169Tests(unittest.TestCase):
    def test_spin_poses_select_spin_actions(self):
        # The pose controller owns animation priority: spin poses map to
        # their own registry actions and are exercised by the C tests.
        text = (ROOT / "src/runtime/room_runtime.c").read_text()
        self.assertIn('case MZM_POSE_SPINNING: return "spin";', text)
        self.assertIn('"screw_attack_space":"screw_attack"', text)
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn("add_test(NAME mzm_samus_controller", cmake)
