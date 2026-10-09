"""ROM-independent validation of the machine-readable GTK room-draft listing."""
from __future__ import annotations

import io
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from scripts import authored_rooms


class TestAuthoredRoomTsv(unittest.TestCase):
    def test_world_filtered_tsv_and_unchanged_human_listing(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for world in ("zero_mission", "aria"):
                room = authored_rooms.new_room(world=world, area=0, slug="custom_hall",
                                               name="Custom Hall", width_screens=2,
                                               height_screens=1)
                authored_rooms.save_room(room, root)
            data = io.StringIO()
            with redirect_stdout(data):
                self.assertEqual(authored_rooms.main([
                    "--root", str(root), "list", "--world", "aria", "--format", "tsv",
                ]), 0)
            self.assertEqual(data.getvalue(), "aria:00:custom_hall\tCustom Hall\t2\t1\n")
            human = io.StringIO()
            with redirect_stdout(human):
                self.assertEqual(authored_rooms.main(["--root", str(root), "list"]), 0)
            self.assertIn("aria:00:custom_hall | Custom Hall | draft_not_playable", human.getvalue())
            self.assertIn("zero_mission:00:custom_hall | Custom Hall | draft_not_playable", human.getvalue())


if __name__ == "__main__":
    unittest.main()
