# SPDX-License-Identifier: GPL-3.0-only
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Priority0169Tests(unittest.TestCase):
    def test_spin_poses_select_spin_actions(self):
        # The controller selects the native body animation
        # (SamusUpdateGraphicsOam) and the runtime draws that library key;
        # the spin poses use their own sSamusAnimPointers rows.
        text = (ROOT / "src/runtime/room_runtime.c").read_text()
        self.assertIn("mzm_samus_graphics_key(samus,equipment,weapon_highlighted", text)
        samus = (ROOT / "src/runtime/mzm_samus.c").read_text()
        self.assertIn('table = "ScrewAttacking";', samus)
        self.assertIn('selector = eq->items & MZM_ITEM_SPACE_JUMP ? "TRUE" : "FALSE";', samus)
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn("add_test(NAME mzm_samus_controller", cmake)
