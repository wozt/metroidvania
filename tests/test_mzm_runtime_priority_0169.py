# SPDX-License-Identifier: GPL-3.0-only
import unittest

class Priority0169Tests(unittest.TestCase):
    def test_priority_source(self):
        from pathlib import Path
        text = (Path(__file__).resolve().parents[1] / "src/runtime/room_runtime.c").read_text()
        self.assertIn("if (spin_jump && (state == RUNTIME_JUMPING", text)
        self.assertIn("spin_jump=(dx > 0.1f || dx < -0.1f)", text)
        self.assertIn("armor_index=(armor_index+1u)%5u", text)
