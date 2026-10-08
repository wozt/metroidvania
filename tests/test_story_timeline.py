import pathlib
import tomllib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class StoryTimelineTests(unittest.TestCase):
    def setUp(self):
        with (ROOT / "data/story/timeline.toml").open("rb") as stream:
            self.data = tomllib.load(stream)
        self.events = self.data["events"]

    def test_schema_and_required_fields(self):
        self.assertEqual(self.data["schema"], "metroid-vania-timeline")
        self.assertEqual(self.data["version"], 1)
        required = {
            "id", "title", "act", "track", "world", "source_room",
            "character", "event_type", "canonical", "original",
            "min_order", "max_order", "requires", "sets", "rewards",
            "cutscene", "status",
        }
        for event in self.events:
            self.assertEqual(required - event.keys(), set(), event["id"])
            self.assertLessEqual(event["min_order"], event["max_order"])
            self.assertIn(event["track"], {"metroid", "castlevania", "shared"})
            self.assertIn(event["status"], {"planned", "data", "prototype", "verified", "integrated"})

    def test_ids_and_outputs_are_unique(self):
        ids = [event["id"] for event in self.events]
        outputs = [flag for event in self.events for flag in event["sets"]]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual(len(outputs), len(set(outputs)))

    def test_every_requirement_has_a_producer(self):
        produced = {flag: event for event in self.events for flag in event["sets"]}
        for event in self.events:
            for requirement in event["requires"]:
                self.assertIn(requirement, produced, event["id"])
                self.assertLessEqual(
                    produced[requirement]["min_order"], event["max_order"],
                    f"{produced[requirement]['id']} -> {event['id']}",
                )

    def test_dependency_graph_is_acyclic(self):
        producer = {
            flag: event["id"] for event in self.events for flag in event["sets"]
        }
        dependencies = {
            event["id"]: {producer[flag] for flag in event["requires"]}
            for event in self.events
        }
        visiting = set()
        visited = set()

        def visit(event_id):
            self.assertNotIn(event_id, visiting, f"cycle through {event_id}")
            if event_id in visited:
                return
            visiting.add(event_id)
            for dependency in dependencies[event_id]:
                visit(dependency)
            visiting.remove(event_id)
            visited.add(event_id)

        for event_id in dependencies:
            visit(event_id)

    def test_required_final_routes_and_epilogues(self):
        ids = {event["id"] for event in self.events}
        self.assertTrue({
            "mzm.mother_brain", "mzm.chozodia_route", "mzm.mecha_ridley",
            "cv.graham_branch", "cv.julius", "cv.chaos",
            "mzm.samus_secret_final", "cv.soma_secret_final",
            "shared.credits",
        }.issubset(ids))


if __name__ == "__main__":
    unittest.main()
