# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free safety tests for the shared private-asset writer."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from scripts import import_game_assets as asset


class ImporterTests(unittest.TestCase):
    def test_reject_symlink_redirect(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "metroid").symlink_to(root / "elsewhere")
            with patch.object(asset, "OUTPUT", root):
                with self.assertRaises(ValueError):
                    asset.write_generated("metroid/raw/data/block.bin", b"secret")
            self.assertFalse((root / "elsewhere").exists())

    def test_safe_parts_accept_only_plain_components(self):
        self.assertEqual(asset.safe_parts("room/data_123.bin"), ("room", "data_123.bin"))
        for name in ("../escape.bin", "/tmp/leak.bin", "folder/../x", "a\\b"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    asset.safe_parts(name)


if __name__ == "__main__":
    unittest.main()
