# SPDX-License-Identifier: GPL-3.0-only
"""Runtime's verified native MZM full-solid mapping remains ID 16."""
from pathlib import Path
import re
import unittest


class NativeSolidContract(unittest.TestCase):
    def test_verified_solid_id_and_diagnostic(self):
        code = (Path(__file__).resolve().parents[1] / "src/runtime/room_runtime.c").read_text()
        self.assertIn("if (kind == 'N' && code == 16) {", code)
        self.assertIn("verified full-solid cells (Clipdata 16)", code)
        self.assertNotIn("if (kind == 'N' && code == 5)", code)


if __name__ == "__main__":
    unittest.main()
