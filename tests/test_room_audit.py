# SPDX-License-Identifier: GPL-3.0-only
"""Structured room-render audit contracts independent of private ROM data."""
from __future__ import annotations

from unittest import mock
from pathlib import Path
import tempfile
import unittest

from scripts.editor_backend import execute
from scripts.room_audit import _diagnostic_status, _finish_record, audit_world


class RoomAuditTests(unittest.TestCase):
    @mock.patch("scripts.editor_backend.write_private_report")
    @mock.patch("scripts.editor_backend.audit_world")
    def test_backend_persists_only_explicit_safe_report(self, audit, write):
        audit.return_value = {"format": "MV_ROOM_RENDER_AUDIT_1", "world": "aria"}
        write.return_value = Path("/private/aria_ci.json")
        with tempfile.TemporaryDirectory() as directory:
            result = execute("room-audit", {
                "world": "aria", "workers": 1, "report": "aria_ci",
            }, root=Path(directory))
        write.assert_called_once_with(audit.return_value, "aria_ci.json")
        self.assertEqual(result["report_path"], "/private/aria_ci.json")

    def test_diagnostics_distinguish_unsupported_and_errors(self):
        self.assertEqual(_diagnostic_status(ValueError("unsupported affine")),
                         "unsupported")
        self.assertEqual(_diagnostic_status(ValueError("truncated resource")),
                         "error")

    def test_completed_record_preserves_dimensions_and_unresolved_counts(self):
        base = {"area_index": 0, "area": "Area", "room": 2}
        result = {
            "status": "PARTIAL_NATIVE_BG_RENDER", "width": 320, "height": 160,
            "limitations": ["diagnostic only"],
            "layers": {"Bg1": {"width_blocks": 20, "height_blocks": 10,
                                  "unresolved_pixels_or_cells": 7}},
        }
        record = _finish_record(base, 0.0, result, None)
        self.assertEqual(record["status"], "partial")
        self.assertEqual(record["expected_render"],
                         {"width_px": 320, "height_px": 160})
        self.assertEqual(record["invalid_graphics_references"][0]["unresolved_count"], 7)

    @mock.patch("scripts.room_audit.audit_mzm_area")
    @mock.patch("scripts.room_audit.discovered_scopes", return_value=["B", "A"])
    def test_world_report_is_deterministic(self, _scopes, area_audit):
        area_audit.side_effect = lambda area: [{
            "status": "partial", "area_index": None, "area": area,
            "room": 0,
        }]
        report = audit_world("zero_mission", workers=1)
        self.assertEqual(report["room_count"], 2)
        self.assertEqual(report["summary"]["partial"], 2)
        self.assertEqual([item["area"] for item in report["rooms"]], ["A", "B"])


if __name__ == "__main__":
    unittest.main()
