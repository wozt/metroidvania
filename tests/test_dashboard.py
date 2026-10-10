# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for the development dashboard and its generator."""
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import tempfile
import unittest
from unittest import mock

from scripts import dashboard

ROOT = Path(__file__).resolve().parents[1]
DASHBOARD = ROOT / "dashboard"
HEADER = "\t".join(dashboard.REGISTRY_HEADER)


def _registry(rows: list[str]) -> Path:
    directory = Path(tempfile.mkdtemp())
    path = directory / "features.tsv"
    path.write_text(HEADER + "\n" + "\n".join(rows) + "\n", encoding="utf-8")
    return path


def _row(identifier="tools.sample", status="functional", area="tools", tests="",
         sources="scripts/dashboard.py", fidelity="n/a", validated=""):
    return "\t".join((area, identifier, "editor", status, fidelity, "Sample", sources, tests,
                      "", "", validated))


class RegistryTests(unittest.TestCase):
    def test_tracked_registry_is_valid(self):
        features = dashboard.load_registry()
        self.assertGreater(len(features), 20)
        self.assertEqual(len({item["id"] for item in features}), len(features))

    def test_registry_rejects_invalid_rows(self):
        for row, message in (
                (_row(status="done"), "unknown status"),
                (_row(fidelity="perfect"), "unknown fidelity"),
                (_row(area="aos", identifier="aos.x"), "unknown area"),
                (_row(identifier="assets.x"), "does not match"),
                (_row(validated="10/10/2026"), "YYYY-MM-DD")):
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    dashboard.load_registry(_registry([row]))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            dashboard.load_registry(_registry([_row(), _row()]))
        bad_header = _registry([])
        bad_header.write_text("area\tid\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "header"):
            dashboard.load_registry(bad_header)


def _feature(status, verdict="untested", sources=("x",)):
    return {"status": status, "local_sources": list(sources),
            "verification": {"verdict": verdict}}


class StatisticsTests(unittest.TestCase):
    def test_percentages_are_shares_of_registered_features(self):
        features = [_feature("functional", "passing"), _feature("faithful"),
                    _feature("partial", "passing"), _feature("unknown"),
                    _feature("not_started")]
        result = dashboard.indicator(features)
        self.assertEqual(result["total"], 5)
        self.assertEqual(sum(result["by_status"].values()), 5)
        self.assertEqual(result["functional_or_better"], 2)
        self.assertEqual(result["functional_or_better_percent"], 40.0)
        self.assertEqual(result["with_passing_tests_percent"], 40.0)
        self.assertEqual(result["unknown_or_blocked"], 1)

    def test_empty_and_unknown_features_never_count_as_progress(self):
        self.assertIsNone(dashboard.indicator([])["functional_or_better_percent"])
        result = dashboard.indicator([_feature("unknown"), _feature("unknown")])
        self.assertEqual(result["functional_or_better_percent"], 0.0)
        self.assertEqual(result["by_status"]["unknown"], 2)

    def test_verification_separates_declared_and_checked_facts(self):
        ctest = {"sample": {"name": "sample", "files": ["tests/test_debug_menu.c"]}}
        validated = {"status": "validated", "local_sources": ["scripts/dashboard.py"],
                     "tests": ["tests/test_debug_menu.c"]}
        # Declared validated, tests not run: flagged, not trusted.
        result = dashboard._verify(validated, ctest, {}, None, None)
        self.assertEqual(result["verdict"], "tests_not_run")
        self.assertTrue(result["warnings"])
        result = dashboard._verify(validated, ctest, {}, {"sample": "passed"}, None)
        self.assertEqual((result["verdict"], result["warnings"]), ("passing", []))
        result = dashboard._verify(validated, ctest, {}, {"sample": "failed"}, None)
        self.assertEqual(result["verdict"], "failing")
        # Missing sources and unregistered tests are reported.
        missing = {"status": "functional", "local_sources": ["does/not/exist.c"],
                   "tests": []}
        self.assertEqual(dashboard._verify(missing, ctest, {}, None, None)["verdict"],
                         "missing_sources")
        orphan = {"status": "partial", "local_sources": [], "tests": ["tests/orphan.c"]}
        self.assertEqual(dashboard._verify(orphan, ctest, {}, None, None)["verdict"],
                         "unregistered_tests")

    def test_cmake_tests_map_sources_to_ctest_names(self):
        tests = dashboard.cmake_tests()
        self.assertIn("tests/test_aos_enemy.c", tests["aos_enemy"]["files"])
        self.assertIn("tests/test_gba_input.c", tests["gba_input"]["files"])


class OutputTests(unittest.TestCase):
    def setUp(self):
        self.data = dashboard.build(now=datetime(2026, 10, 10, tzinfo=timezone.utc))

    def test_generated_data_is_coherent(self):
        data = self.data
        self.assertEqual(data["schema"], dashboard.SCHEMA)
        ids = [item["id"] for item in data["features"]]
        self.assertEqual(len(ids), len(set(ids)))
        overall = data["indicators"][0]
        self.assertEqual(overall["id"], "global")
        self.assertEqual(overall["total"], len(ids))
        self.assertEqual(sum(item["total"] for item in data["indicators"][1:]), len(ids))
        for item in data["indicators"]:
            self.assertEqual(sum(item["by_status"].values()), item["total"])
            for key in ("functional_or_better_percent", "with_passing_tests_percent"):
                if item[key] is not None:
                    self.assertGreaterEqual(item[key], 0)
                    self.assertLessEqual(item[key], 100)
        # Native features come from the parity annotations, unchanged in number.
        native = [item for item in data["features"]
                  if item["source"] == "data/native_parity/annotations.tsv"]
        self.assertTrue(native)
        self.assertFalse(data["tests"]["ran"])

    def test_public_outputs_hold_no_private_data(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = dashboard.write(self.data, Path(directory))
            for path in paths:
                text = path.read_text(encoding="utf-8")
                self.assertNotIn("assets/extracted", text)
                self.assertNotIn(str(ROOT), text)
                self.assertNotIn("data:image", text)
                self.assertNotRegex(text, r"roms/[^\"]+\.gba\b(?!\")")
            js = paths[1].read_text(encoding="utf-8")
            payload = js.split("window.METROIDVANIA_DASHBOARD = ", 1)[1].rstrip().rstrip(";")
            self.assertEqual(json.loads(payload), json.loads(paths[0].read_text("utf-8")))
        with self.assertRaisesRegex(ValueError, "assets/extracted"):
            dashboard.write({"leak": "assets/extracted/aria/x.bmp"}, Path(tempfile.mkdtemp()))

    def test_tracked_dashboard_is_static_and_offline(self):
        allowed = {"index.html", "style.css", "app.js", "data.js", "data.json"}
        self.assertEqual({path.name for path in DASHBOARD.iterdir()}, allowed)
        html = (DASHBOARD / "index.html").read_text(encoding="utf-8")
        scripts = re.findall(r'<script src="([^"]+)"', html)
        self.assertEqual(scripts, ["data.js", "app.js"])
        for page in ("index.html", "style.css", "app.js"):
            text = (DASHBOARD / page).read_text(encoding="utf-8")
            self.assertNotRegex(text, r"(src|href)=\"https?://")
            self.assertNotIn("@import", text)
        app = (DASHBOARD / "app.js").read_text(encoding="utf-8")
        self.assertNotIn("fetch(", app)
        self.assertNotIn("XMLHttpRequest", app)
        js = (DASHBOARD / "data.js").read_text(encoding="utf-8")
        payload = json.loads(js.split("window.METROIDVANIA_DASHBOARD = ", 1)[1]
                             .rstrip().rstrip(";"))
        self.assertEqual(payload["schema"], dashboard.SCHEMA)
        for path in DASHBOARD.iterdir():
            self.assertLess(path.stat().st_size, 2 * 1024 * 1024)

    def test_generation_does_not_run_tests_unless_asked(self):
        with mock.patch.object(dashboard, "run_ctest") as ctest, \
                mock.patch.object(dashboard, "run_python_tests") as python:
            dashboard.build()
        ctest.assert_not_called()
        python.assert_not_called()


if __name__ == "__main__":
    unittest.main()
