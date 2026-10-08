# SPDX-License-Identifier: GPL-3.0-only
"""Authored rooms must remain private, valid and explicitly unplayable."""
from __future__ import annotations

import json
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path

from scripts.authored_rooms import (
    AREAS, export_for_engine, list_rooms, load_room, new_room, room_path,
    save_room, validate_room,
)


class AuthoredRoomsContract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def room(self, world="zero_mission", area=0, slug="first_room"):
        return new_room(world=world, area=area, slug=slug, name="New room",
                        width_screens=2, height_screens=1)

    def test_both_worlds_and_private_paths(self):
        for world in AREAS:
            room = self.room(world)
            self.assertEqual(validate_room(room), f"{world}:00:first_room")
            path = save_room(room, self.root)
            self.assertIn("/assets/extracted/authored_rooms/", path.as_posix())
            self.assertEqual(load_room(path, self.root), room)
            self.assertEqual(list_rooms(self.root, world), [room])
            self.assertEqual(room_path(room, self.root), path)

    def test_every_area_has_a_stable_identity(self):
        for world, names in AREAS.items():
            for idx, label in enumerate(names):
                draft = self.room(world, idx)
                self.assertEqual(draft["area"]["name"], label)
                self.assertEqual(draft["id"], f"{world}:{idx:02d}:first_room")

    def test_duplicate_create_must_not_overwrite(self):
        room = self.room()
        path = save_room(room, self.root)
        content = path.read_bytes()
        with self.assertRaises(FileExistsError):
            save_room(room, self.root)
        self.assertEqual(path.read_bytes(), content)

    def test_reject_traversal_invalid_bounds_and_bool_as_number(self):
        for slug in ("../escape", "../../roms", "BAD", "two names", "", "a" * 41):
            with self.subTest(slug=slug), self.assertRaises(ValueError):
                self.room(slug=slug)
        for bad_area in (-1, 999, True):
            with self.subTest(area=bad_area), self.assertRaises(ValueError):
                self.room(area=bad_area)
        for width in (0, 9, True, -1):
            with self.subTest(width=width), self.assertRaises(ValueError):
                new_room(world="aria", area=0, slug="new", name="New room",
                         width_screens=width, height_screens=1)

    def test_reject_forged_engine_ready_or_source_data(self):
        original = self.room()
        changes = (
            ("status", "engine_ready"),
            ("origin", "original_rom"),
            ("collision", {"unsafe": True}),
            ("layers", [{"rom_pointer": "0x08000000"}]),
            ("engine_adapter", "aria"),
            ("name", "bad\nname"),
        )
        for key, value in changes:
            with self.subTest(key=key):
                draft = deepcopy(original)
                draft[key] = value
                with self.assertRaises(ValueError):
                    validate_room(draft)
        with self.assertRaises(NotImplementedError):
            export_for_engine(original)

    def test_unknown_fields_and_mutated_ids_are_rejected(self):
        original = self.room()
        for key, value in (("id", "zero_mission:00:other"),
                           ("geometry", {"width_screens": 2}),
                           ("area", {"index": 0, "name": "Wrong"}),
                           ("version", 2)):
            draft = deepcopy(original)
            draft[key] = value
            with self.assertRaises(ValueError):
                validate_room(draft)
        draft = deepcopy(original)
        draft["extra"] = 12
        with self.assertRaises(ValueError):
            validate_room(draft)

    def test_refuse_symlink_directory_and_identity_mismatch(self):
        room = self.room()
        base = self.root / "assets" / "extracted"
        base.mkdir(parents=True)
        (base / "authored_rooms").symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            save_room(room, self.root)
        (base / "authored_rooms").unlink()
        path = save_room(room, self.root)
        renamed = path.with_name("different.json")
        renamed.write_bytes(path.read_bytes())
        with self.assertRaisesRegex(ValueError, "path and identity"):
            load_room(renamed, self.root)
        link = path.with_name("link.json")
        link.symlink_to(path)
        with self.assertRaisesRegex(ValueError, "symlink"):
            load_room(link, self.root)

    def test_reject_malformed_private_json(self):
        room = self.room()
        path = save_room(room, self.root)
        bad = deepcopy(room)
        bad["status"] = "playable"
        path.write_text(json.dumps(bad), encoding="utf-8")
        with self.assertRaises(ValueError):
            load_room(path, self.root)


if __name__ == "__main__":
    unittest.main()
