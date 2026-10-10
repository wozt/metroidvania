# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free contracts for the canonical private asset layout and audit."""
from pathlib import Path
import tempfile
import unittest

from scripts.asset_layout import (
    EXTRACTED,
    METROID_SAMUS_BODY_SOURCE,
    METROID_SAMUS_COMPOSED_SOURCE,
    METROID_SAMUS_RUNTIME,
    METROID_SAMUS_SPECIAL_SOURCE,
    private_path,
)
from scripts.audit_extracted_assets import REPORT, audit


class AssetLayoutTests(unittest.TestCase):
    def test_audit_counts_exact_duplicates_and_source_references(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "assets/extracted/samus").mkdir(parents=True)
            canonical = root / METROID_SAMUS_RUNTIME
            canonical.mkdir(parents=True)
            (root / "assets/extracted/samus/frame.bmp").write_bytes(b"BMprivate")
            (canonical / "object.bmp").write_bytes(b"BMprivate")
            source = root / "scripts"
            source.mkdir()
            (source / "consumer.py").write_text(
                'path = "assets/extracted/samus/frame.bmp"\n', encoding="utf-8")

            report = audit(root)

            self.assertEqual(report["totals"], {"files": 2, "bytes": 18})
            self.assertEqual(report["duplicates"]["groups"], 1)
            self.assertEqual(report["duplicates"]["files"], 2)
            self.assertEqual(report["duplicates"]["reclaimable_bytes"], 9)
            self.assertEqual(report["layout"]["legacy_samus_roots"], ["samus"])
            by_name = {entry["name"]: entry for entry in report["top_level"]}
            self.assertEqual(by_name["samus"]["reference_files"], 1)
            self.assertEqual(by_name["metroid"]["classification"], "canonical")

    def test_audit_excludes_its_own_generated_report(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report = root / REPORT
            report.parent.mkdir(parents=True)
            report.write_text("previous report", encoding="utf-8")
            (report.parent / "asset_inventory.tmp").write_text(
                "interrupted report", encoding="utf-8")
            self.assertEqual(audit(root)["totals"], {"files": 0, "bytes": 0})

    def test_canonical_samus_layout_has_no_patch_number_directories(self):
        paths = (
            METROID_SAMUS_RUNTIME,
            METROID_SAMUS_BODY_SOURCE,
            METROID_SAMUS_COMPOSED_SOURCE,
            METROID_SAMUS_SPECIAL_SOURCE,
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
