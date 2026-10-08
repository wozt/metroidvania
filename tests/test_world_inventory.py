import collections
import pathlib
import tomllib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]

MZM_BOSSES = {
    "mzm.deorem",
    "mzm.mua",
    "mzm.kraid",
    "mzm.kiru_giru",
    "mzm.imago",
    "mzm.ridley",
    "mzm.mother_brain",
    "mzm.ruins_test",
    "mzm.mecha_ridley",
}

ARIA_BOSSES = {
    "aria.creaking_skull",
    "aria.manticore",
    "aria.great_armor",
    "aria.big_golem",
    "aria.headhunter",
    "aria.death",
    "aria.legion",
    "aria.balore",
    "aria.graham",
    "aria.julius",
    "aria.chaos",
}


class WorldInventoryTests(unittest.TestCase):
    def setUp(self):
        with (ROOT / "data/story/world_inventory.toml").open("rb") as stream:
            self.data = tomllib.load(stream)
        self.bosses = self.data["bosses"]
        self.savepoints = self.data["savepoints"]

    def test_schema_and_source_aliases(self):
        self.assertEqual(self.data["schema"], "metroid-vania-world-inventory")
        self.assertEqual(self.data["version"], 1)
        source_aliases = set(self.data["sources"])
        for record in self.bosses + self.savepoints:
            self.assertTrue(set(record["sources"]).issubset(source_aliases), record["id"])

    def test_campaign_boss_rosters_are_exact(self):
        by_game = collections.defaultdict(set)
        for boss in self.bosses:
            by_game[boss["game"]].add(boss["id"])
        self.assertEqual(by_game["zero_mission"], MZM_BOSSES)
        self.assertEqual(by_game["aria_of_sorrow"], ARIA_BOSSES)
        self.assertEqual(len(self.bosses), 20)
        self.assertTrue(all(boss["campaign_required"] for boss in self.bosses))

    def test_boss_ids_and_required_fields(self):
        required = {
            "id", "game", "name", "zone", "classification",
            "campaign_required", "original_requirement", "source_rooms",
            "rom_addresses", "spawn_conditions", "prerequisites",
            "capabilities", "stats", "attacks", "music", "resources",
            "sequences", "reward", "flags", "decode_state", "story_role",
            "sources",
        }
        ids = [boss["id"] for boss in self.bosses]
        self.assertEqual(len(ids), len(set(ids)))
        for boss in self.bosses:
            self.assertEqual(required - boss.keys(), set(), boss["id"])
            self.assertTrue(boss["classification"], boss["id"])
            self.assertTrue(boss["decode_state"], boss["id"])

    def test_unverified_aria_rom_fields_stay_empty(self):
        for boss in self.bosses:
            if boss["game"] != "aria_of_sorrow":
                continue
            self.assertIn("unverified", boss["decode_state"], boss["id"])
            self.assertEqual(boss["source_rooms"], [], boss["id"])
            self.assertEqual(boss["rom_addresses"], [], boss["id"])
            self.assertEqual(boss["stats"], {}, boss["id"])

    def test_boss_rush_encounter_is_explicitly_excluded(self):
        excluded = {entry["id"] for entry in self.data["excluded_encounters"]}
        campaign = {boss["id"] for boss in self.bosses}
        self.assertIn("aria.man_eater", excluded)
        self.assertNotIn("aria.man_eater", campaign)

    def test_zero_mission_savepoint_set_is_complete(self):
        mzm = [point for point in self.savepoints if point["game"] == "zero_mission"]
        self.assertEqual(len(mzm), 29)
        self.assertEqual(
            collections.Counter(point["area"] for point in mzm),
            {
                "Brinstar": 4,
                "Kraid": 5,
                "Norfair": 5,
                "Ridley": 4,
                "Tourian": 3,
                "Chozodia": 8,
            },
        )
        expected_rooms = {
            "Brinstar": {33, 34, 36, 39},
            "Kraid": {20, 31, 32, 36, 39},
            "Norfair": {36, 39, 41, 44, 45},
            "Ridley": {1, 20, 24, 25},
            "Tourian": {6, 11, 17},
            "Chozodia": {4, 15, 21, 27, 40, 61, 74, 75},
        }
        for area, rooms in expected_rooms.items():
            self.assertEqual(
                {point["room"] for point in mzm if point["area"] == area},
                rooms,
            )
        coverage = self.data["savepoint_coverage"]["zero_mission"]
        self.assertEqual(coverage["status"], "verified_source")
        self.assertEqual(coverage["expected_count"], len(mzm))

    def test_savepoint_ids_locations_and_required_fields(self):
        required = {
            "id", "game", "area", "room", "map_x", "map_y", "type",
            "access", "gates", "boss_context", "capabilities",
            "connections", "graphics", "music", "verification", "sources",
            "cross_world_link",
        }
        ids = [point["id"] for point in self.savepoints]
        locations = [
            (point["game"], point["area"], point["room"])
            for point in self.savepoints
        ]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual(len(locations), len(set(locations)))
        for point in self.savepoints:
            self.assertEqual(required - point.keys(), set(), point["id"])
            if point["game"] == "zero_mission":
                self.assertEqual(
                    point["id"],
                    f"mzm.save.{point['area'].lower()}.{point['room']}",
                )
                self.assertEqual(point["type"], "save_platform")
                self.assertEqual(point["verification"], "verified_source")
            else:
                self.assertEqual(
                    point["id"],
                    f"aria.save.{point['engine_area']}.{point['room']}",
                )
                self.assertEqual(point["type"], "save_room_flag")
            self.assertIn("save", point["capabilities"])
            self.assertEqual(point["cross_world_link"], "")

    def test_aria_savepoints_match_verified_map_table(self):
        aria = [point for point in self.savepoints if point["game"] == "aria_of_sorrow"]
        coverage = self.data["savepoint_coverage"]["aria_of_sorrow"]
        expected = {
            (5, 11, 19, 1, "0x085179d0"),
            (6, 31, 38, 5, "0x085196e0"),
            (9, 22, 29, 6, "0x085209b8"),
            (4, 20, 15, 10, "0x08516768"),
            (6, 15, 39, 11, "0x08518d28"),
            (3, 21, 9, 13, "0x08514c70"),
            (0, 31, 40, 16, "0x0851021c"),
            (3, 22, 10, 19, "0x08514ce8"),
            (5, 12, 29, 20, "0x08517a48"),
            (1, 14, 48, 20, "0x085122fc"),
            (0, 14, 17, 21, "0x0850f7ec"),
            (0, 36, 9, 22, "0x08510504"),
            (2, 12, 39, 26, "0x085132d4"),
            (7, 39, 18, 27, "0x0851c0d4"),
            (7, 48, 27, 28, "0x0851c6b4"),
            (8, 21, 2, 29, "0x0851f128"),
            (7, 41, 24, 33, "0x0851c1cc"),
        }
        actual = {
            (
                point["engine_area"], point["room"], point["map_x"],
                point["map_y"], point["room_pointer"],
            )
            for point in aria
        }
        self.assertEqual(actual, expected)
        self.assertTrue(
            all(point["verification"] == "verified_rom_map_table" for point in aria)
        )
        self.assertEqual(coverage["status"], "verified_rom_map_table")
        self.assertEqual(coverage["expected_count"], len(aria))


if __name__ == "__main__":
    unittest.main()
