# SPDX-License-Identifier: GPL-3.0-only
"""Static parity checks for the MZM display boundary."""
import re
import unittest
from pathlib import Path


class MzmDisplayAuditTests(unittest.TestCase):
    def test_game_never_selects_an_unsupported_bitmap_mode(self):
        root = Path(__file__).resolve().parents[1]
        source_root = root / "third_party/mzm/src"
        selected = set()
        for path in source_root.rglob("*.c"):
            selected.update(re.findall(r"\bDCNT_MODE_([0-5])\b",
                                       path.read_text(encoding="utf-8")))
        self.assertTrue(selected <= {"0", "1", "2"}, selected)
        self.assertNotIn("3", selected)
        self.assertNotIn("4", selected)
        self.assertNotIn("5", selected)


if __name__ == "__main__":
    unittest.main()
