# SPDX-License-Identifier: GPL-3.0-only
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from scripts.mzm_samus_runtime_library_0175 import build, inside

class NativeLibrary0175Tests(unittest.TestCase):
    def test_missing_sources(self):
        with TemporaryDirectory() as d:
            root = Path(d)
            (root / "assets/extracted").mkdir(parents=True)
            with self.assertRaisesRegex(ValueError, "no animation sources"):
                build(root)
    def test_directory_safety(self):
        with TemporaryDirectory() as d:
            root = Path(d)
            (root / "assets/extracted").mkdir(parents=True)
            self.assertEqual(inside(root, "samus_library"), root / "assets/extracted/samus_library")
