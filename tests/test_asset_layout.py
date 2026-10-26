# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free contracts for the canonical private asset layout and audit."""
from pathlib import Path
import tempfile
import unittest

from scripts.asset_layout import (
    EXTRACTED,
    METROID_SAMUS_ANIMATIONS,
    METROID_SAMUS_RUNTIME,
    private_path,
)


class AssetLayoutTests(unittest.TestCase):
    def test_canonical_samus_layout_has_no_patch_number_directories(self):
        paths = (
            METROID_SAMUS_RUNTIME,
            METROID_SAMUS_ANIMATIONS,
                )
        for path in paths:
            with self.subTest(path=path):
                self.assertFalse(any(part[-4:].isdigit() for part in path.parts))

    def test_private_path_rejects_symlink_components(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            outside = root / "outside"
            outside.mkdir()
            (root / "assets").mkdir()
            (root / EXTRACTED).symlink_to(outside, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "symlink"):
                private_path(root, METROID_SAMUS_RUNTIME, create=True)


if __name__ == "__main__":
    unittest.main()
