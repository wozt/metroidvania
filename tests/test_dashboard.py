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
        aos_inventory = data["inventory"]["games"]["aos"]
        self.assertEqual(aos_inventory["routines_attached_to_features"] +
                         aos_inventory["unassigned_routines"],
                         aos_inventory["routines"])
        report_rows = len((ROOT / "data/native_parity/aos_unassigned_routines.tsv")
                          .read_text(encoding="utf-8").splitlines()) - 1
        self.assertEqual(aos_inventory["unassigned_routines"], report_rows)
        self.assertGreater(report_rows, 3000)
        self.assertFalse(data["tests"]["ran"])

    def test_bestiary_lists_monsters_and_items_from_tracked_sources(self):
        bestiary = self.data["bestiary"]
        mzm, aos = bestiary["games"]["mzm"], bestiary["games"]["aos"]
        self.assertGreaterEqual(len(mzm["monsters"]), 90)
        self.assertEqual(len(aos["monsters"]), dashboard.ARIA_ENEMY_COUNT)
        for game in (mzm, aos):
            for row in game["monsters"] + game["items"]:
                self.assertIn(row["status"], dashboard.STATUS_IDS)
                self.assertEqual(set(row["evidence"]), {
                    "unit_tests", "in_game_check", "emulator_compared", "played_through",
                })
        mzm_done = {row["name"] for row in mzm["monsters"]
                    if row["status"] != "not_started"}
        self.assertIn("Zoomer Yellow", mzm_done)
        self.assertIn("Sidehopper", mzm_done)
        self.assertIn("Mother Brain", mzm_done)
        self.assertIn("Acid Worm", mzm_done)
        self.assertIn("Tangle Vine Red Garuta", mzm_done)
        self.assertIn("Crocomire", mzm_done)
        self.assertIn("Gekitai Machine", mzm_done)
        mzm_items = {row["name"]: row["status"] for row in mzm["items"]}
        # The drops follow mzm.items.enemy_drops.
        self.assertEqual(mzm_items["Small Energy Drop"], "functional")
        self.assertEqual(mzm_items["Missile Drop"], "functional")
        self.assertEqual(mzm_items["Multiple Large Energy"], "functional")
        self.assertEqual(mzm_items["Morph Ball"], "functional")
        self.assertEqual(mzm_items["Power Grip"], "functional")
        self.assertEqual(mzm_items["Charge Beam"], "functional")
        self.assertEqual(mzm_items["Long Beam (Chozo statue)"], "functional")
        self.assertEqual(mzm_items["Plasma Beam (Chozo statue)"], "functional")
        self.assertEqual(mzm_items["Energy Tank"], "functional")
        self.assertNotIn("Long Beam Hint (Chozo statue)", mzm_items)
        # Not one of the five enemy drops of is_drop().
        self.assertNotIn("Water Drop", mzm_items)
        aos_partial = {row["name"] for row in aos["monsters"] if row["status"] == "partial"}
        self.assertEqual(aos_partial, set())
        aos_functional = {row["name"] for row in aos["monsters"]
                          if row["status"] == "functional"}
        self.assertEqual(aos_functional, {"Bat", "Zombie", "Blue Crow", "Zombie Soldier",
                                          "Axe Armor", "Skull Archer", "Needles",
                                          "Medusa Head", "Zombie Officer", "Gargoyle",
                                          "Ghost Dancer", "Curly", "Durga",
                                          "Valkyrie", "Erinys", "Gremlin",
                                          "Skeleton Knight", "Dead Warrior",
                                          "Fleaman", "Red Crow", "Mimic",
                                          "Dead Crusader", "Cagnazzo", "Skull Millione",
                                          "Lubicant"})
        unnamed = [row for row in aos["monsters"] if row["status"] == "not_started"]
        self.assertTrue(all(row["name"].startswith("Enemy 0x") for row in unnamed))
        self.assertEqual(len(aos["items"]), len(dashboard.ARIA_ITEM_FAMILIES))
        self.assertTrue(all(row["status"] == "not_started" for row in aos["items"]))

    def test_bestiary_tracks_the_engine_switch(self):
        engine = (ROOT / "src/runtime/mzm_sprites.c").read_text(encoding="utf-8")
        annotation_ids = {item["id"] for item in dashboard.load_native_features()}
        mapping = dashboard._mzm_sprite_annotations(engine, annotation_ids)
        self.assertEqual(mapping["ZOOMER_YELLOW"], "mzm.enemies.zoomer")
        self.assertEqual(mapping["DESSGEEGA_AFTER_LONG_BEAM"],
                         "mzm.enemies.dessgeega")
        self.assertEqual(mapping["MOTHER_BRAIN"], "mzm.enemies.mother_brain")
        self.assertEqual(mapping["ZEB"], "mzm.enemies.pipe_bugs")
        self.assertEqual(mapping["ZEBETITE_ONE_AND_THREE"],
                         "mzm.enemies.tourian_defences")
        self.assertEqual(mapping["CHOZO_STATUE_LONG"], "mzm.items.chozo_statues")
        self.assertEqual(mapping["MORPH_BALL"], "mzm.items.item_sprites")
        self.assertEqual(mapping["MAP_STATION"], "mzm.environment.room_gadgets")
        self.assertEqual(mapping["ACID_WORM"], "mzm.enemies.acid_worm")
        self.assertEqual(mapping["GADORA_KRAID"], "mzm.enemies.gadora")
        self.assertEqual(mapping["GADORA_RIDLEY"], "mzm.enemies.gadora")
        self.assertEqual(mapping["IMAGO_LARVA_RIGHT"], "mzm.enemies.imago_larva")
        self.assertEqual(mapping["IMAGO_LARVA_LEFT"], "mzm.enemies.imago_larva")
        self.assertEqual(mapping["IMAGO_LARVA_RIGHT_SIDE"], "mzm.enemies.imago_larva")
        self.assertEqual(mapping["IMAGO_COCOON_AFTER_FIGHT"],
                         "mzm.enemies.imago_cocoon")
        self.assertEqual(mapping["IMAGO_COCOON"], "mzm.enemies.imago_cocoon")
        self.assertEqual(mapping["DISCOVERED_IMAGO_PASSAGE_EVENT_TRIGGER"],
                         "mzm.enemies.imago_cocoon")
        self.assertEqual(mapping["TANGLE_VINE_RED_GARUTA"],
                         "mzm.enemies.tangle_vine")
        self.assertEqual(mapping["CROCOMIRE"], "mzm.enemies.crocomire")
        self.assertEqual(mapping["GEKITAI_MACHINE"], "mzm.enemies.crocomire")
        # Drops take the MZM_FAMILY_DROP default, not a case label.
        self.assertNotIn("SMALL_ENERGY_DROP", mapping)
        self.assertTrue(set(mapping.values()).issubset(annotation_ids))

    def test_bestiary_rejects_an_unregistered_sprite_family(self):
        engine = """int mzm_sprite_family(int type) {
            switch (type) {
            case MZM_PSPRITE_SAMPLE:
                return MZM_FAMILY_SAMPLE;
            default:
                return MZM_FAMILY_NONE;
            }
        }"""
        with self.assertRaisesRegex(ValueError, "unknown parity annotation"):
            dashboard._mzm_sprite_annotations(engine, set())


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
        self.assertIn('location.protocol === "file:"', app)
        self.assertIn("https://github.com/wozt/metroidvania", app)
        for path in ("docs/PROJECT_STATUS.md", "docs/NATIVE_PARITY_CHECKLIST.md",
                     "data/native_parity/annotations.tsv"):
            self.assertIn(f'data-repo-path="{path}"', html)
        js = (DASHBOARD / "data.js").read_text(encoding="utf-8")
        payload = json.loads(js.split("window.METROIDVANIA_DASHBOARD = ", 1)[1]
                             .rstrip().rstrip(";"))
        self.assertEqual(payload["schema"], dashboard.SCHEMA)
        for path in DASHBOARD.iterdir():
            self.assertLess(path.stat().st_size, 2 * 1024 * 1024)

    def test_check_compares_only_registry_derived_facts(self):
        fixed = datetime(2026, 1, 2, tzinfo=timezone.utc)
        with mock.patch.object(dashboard, "run_ctest") as ctest, \
                mock.patch.object(dashboard, "run_python_tests") as python:
            data = dashboard.build(now=fixed)
            with tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                dashboard.write(data, output)
                self.assertEqual(dashboard.check(output), [])
                # Volatile facts (time, git, outcomes) are not compared.
                volatile = json.loads((output / "data.json").read_text(encoding="utf-8"))
                volatile["generated_at"] = "2000-01-01T00:00:00+00:00"
                volatile["git"]["commit"] = "0000000"
                volatile["features"][0]["verification"]["verdict"] = "passing"
                dashboard.write(volatile, output)
                self.assertEqual(dashboard.check(output), [])
                # A declared field or a status count that changed is stale.
                stale = json.loads((output / "data.json").read_text(encoding="utf-8"))
                stale["features"][0]["limitations"] += " (old)"
                stale["indicators"][0]["functional_or_better"] += 1
                dashboard.write(stale, output)
                problems = dashboard.check(output)
                self.assertIn(f"{stale['features'][0]['id']}: stale limitations", problems)
                self.assertIn(f"indicator {stale['indicators'][0]['id']}: stale status counts",
                              problems)
                # A data.js that does not carry data.json is stale.
                dashboard.write(data, output)
                (output / "data.js").write_text("// x\nwindow.METROIDVANIA_DASHBOARD = {};\n",
                                                encoding="utf-8")
                self.assertIn("data.js does not carry data.json", dashboard.check(output))
        ctest.assert_not_called()
        python.assert_not_called()

    def test_generation_does_not_run_tests_unless_asked(self):
        with mock.patch.object(dashboard, "run_ctest") as ctest, \
                mock.patch.object(dashboard, "run_python_tests") as python:
            dashboard.build()
        ctest.assert_not_called()
        python.assert_not_called()


if __name__ == "__main__":
    unittest.main()
