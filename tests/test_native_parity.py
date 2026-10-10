# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for native source inventory and rebuild orchestration."""
from pathlib import Path
import json
import tempfile
import unittest
from unittest import mock

from scripts import native_inventory, native_parity, rebuild


class NativeInventoryTests(unittest.TestCase):
    def test_c_discovery_preserves_static_calls_and_source_address(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "src/player.c"
            source.parent.mkdir()
            source.write_text(
                "/** @brief 1234 | Public update */\n"
                "void PlayerUpdate(void)\n{ Helper(); }\n"
                "static void Helper(void)\n{\n"
                "  const char *ignored = \"FakeCall()\";\n"
                "  (void)ignored;\n}\n",
                encoding="utf-8")

            routines = native_inventory._discover_c("mzm", root, source)

            self.assertEqual([item["symbol"] for item in routines],
                             ["PlayerUpdate", "Helper"])
            self.assertEqual(routines[0]["address"], "0x08001234")
            self.assertEqual(routines[0]["description"], "Public update")
            self.assertEqual(routines[1]["linkage"], "internal")
            self.assertNotIn("FakeCall", routines[1]["_raw_calls"])

    def test_assembly_discovery_excludes_exported_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "asm/code.s"
            source.parent.mkdir()
            source.write_text(
                ".global exported_data\nexported_data:\n.word 1\n"
                "thumb_func_start NativeFunction\n"
                "NativeFunction: @ 0x08001000\n"
                "  bl OtherFunction\n",
                encoding="utf-8")

            routines = native_inventory._discover_asm("aos", root, source)

            self.assertEqual(len(routines), 1)
            self.assertEqual(routines[0]["symbol"], "NativeFunction")
            self.assertEqual(routines[0]["_raw_calls"], ["OtherFunction"])

    def test_tracked_inventory_covers_both_pinned_sources(self):
        inventory = json.loads(native_inventory.DEFAULT_OUTPUT.read_text(
            encoding="utf-8"))
        self.assertEqual(inventory["schema"], native_inventory.SCHEMA)
        self.assertGreater(inventory["games"]["mzm"]["statistics"]["routines"], 2500)
        self.assertGreater(inventory["games"]["aos"]["statistics"]["routines"], 3000)
        for game in ("mzm", "aos"):
            self.assertEqual(len(inventory["games"][game]["source_revision"]), 40)
            self.assertTrue(inventory["games"][game]["routines"])


class NativeParityTests(unittest.TestCase):
    def test_annotations_validate_and_checklist_is_current(self):
        inventory = native_parity._load(native_parity.INVENTORY)
        annotations = native_parity._load(native_parity.ANNOTATIONS)

        diagnostics = native_parity.validate(inventory, annotations)
        generated = native_parity.render(inventory, annotations)

        self.assertEqual(diagnostics, [])
        self.assertEqual(native_parity.CHECKLIST.read_text(encoding="utf-8"),
                         generated)
        self.assertIn("mzm.player.samus_controller", generated)
        self.assertIn("aos.enemies.zombie", generated)
        self.assertIn("Counts are discovery coverage", generated)

    def test_rebuild_invalidates_changed_inputs(self):
        cache_root = rebuild.ROOT / ".cache"
        cache_root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=cache_root) as directory:
            temporary = Path(directory)
            source = temporary / "input.txt"
            output = temporary / "output.txt"
            state = temporary / "state.json"
            source.write_text("first", encoding="utf-8")

            def action() -> None:
                output.write_text(source.read_text(encoding="utf-8"),
                                  encoding="utf-8")

            task = rebuild.Task("sample", 1, (), lambda: [source],
                                (output,), action)
            with mock.patch.dict(rebuild.TASKS, {"sample": task}, clear=True):
                self.assertEqual(rebuild.rebuild({"sample"}, state_path=state),
                                 [("sample", "rebuilt")])
                self.assertEqual(rebuild.rebuild({"sample"}, state_path=state),
                                 [("sample", "up to date")])
                source.write_text("second", encoding="utf-8")
                self.assertEqual(rebuild.rebuild({"sample"}, state_path=state),
                                 [("sample", "rebuilt")])
            self.assertEqual(output.read_text(encoding="utf-8"), "second")


if __name__ == "__main__":
    unittest.main()
