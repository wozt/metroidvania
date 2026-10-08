# SPDX-License-Identifier: GPL-3.0-only
import unittest
from scripts.aos_room_index import index_rows


def sample_world():
    return {"format": "MV_AOS_WORLD_2", "native_room_count": 2,
            "savepoints": [{"engine_area": 0, "room": 1}],
            "boss_rooms": [{"engine_area": 1, "room": 2}],
            "rooms": [
                {"engine_area": 1, "room": 2, "area": "Chapel",
                 "backgrounds": [{"width_screens": 1, "height_screens": 2}, {}, {}],
                 "entities": [1, 2, 3], "transitions": [1]},
                {"engine_area": 0, "room": 1, "area": "Castle Corridor",
                 "backgrounds": [{"width_screens": 2, "height_screens": 1}, {}, {}],
                 "entities": [], "transitions": []}]}


class AriaIndexTests(unittest.TestCase):
    def test_deterministic_index_and_save_boss_tags(self):
        rows = index_rows(sample_world())
        self.assertEqual(rows[2], "0|1|Castle Corridor|2|1|0|0|1|0")
        self.assertEqual(rows[3], "1|2|Chapel|1|2|3|1|0|1")

    def test_reject_duplicate_room(self):
        w = sample_world()
        w["rooms"].append(dict(w["rooms"][1]))
        w["native_room_count"] += 1
        with self.assertRaisesRegex(ValueError, "duplicate"):
            index_rows(w)

    def test_reject_invalid_dimensions(self):
        w = sample_world()
        w["rooms"][0]["backgrounds"][0]["width_screens"] = 255
        with self.assertRaisesRegex(ValueError, "dimensions"):
            index_rows(w)

    def test_reject_untrusted_area_label(self):
        w = sample_world()
        w["rooms"][0]["area"] = "\nBAD|INJECT"
        with self.assertRaisesRegex(ValueError, "label"):
            index_rows(w)

    def test_reject_catalog_count(self):
        w = sample_world()
        w["native_room_count"] = 3
        with self.assertRaisesRegex(ValueError, "count mismatch"):
            index_rows(w)


if __name__ == "__main__":
    unittest.main()
