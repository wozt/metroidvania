# SPDX-License-Identifier: GPL-3.0-only
"""ROM-free tests for native source inventory and rebuild orchestration."""
from pathlib import Path
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

    def test_headers_types_constants_and_pointer_tables_are_discovered(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / "include/example.h"
            source = root / "src/example.c"
            header.parent.mkdir()
            source.parent.mkdir()
            header.write_text(
                "#define EXAMPLE_LIMIT 4\n"
                "#define EXAMPLE_SCALE(value) ((value) * 2)\n"
                "struct Example { int value; void (*callback)(void); };\n"
                "typedef struct { int x, y; unsigned flags:3, mode:2; } Point;\n"
                "enum ExampleState { EXAMPLE_IDLE, EXAMPLE_ACTIVE = 3 };\n"
                "void PublicFunction(void);\n"
                "extern int ExternalData;\n",
                encoding="utf-8")
            source.write_text(
                "typedef void (*ExampleFunc)(void);\n"
                "static ExampleFunc sFunctions[] = { First, Second };\n"
                "static int sFirst = 1, sSecond[2] = {2, 3}, *sThird;\n",
                encoding="utf-8")

            declarations = native_inventory._discover_declarations(
                "mzm", root, header)
            types = native_inventory._discover_types("mzm", root, header)
            constants = native_inventory._discover_constants("mzm", root, header)
            data = native_inventory._discover_c_data("mzm", root, source)

            self.assertEqual([item["symbol"] for item in declarations],
                             ["PublicFunction"])
            self.assertEqual({item["symbol"] for item in types},
                             {"Example", "Point", "ExampleState"})
            example = next(item for item in types if item["symbol"] == "Example")
            self.assertEqual(example["members"], ["value", "callback"])
            point = next(item for item in types if item["symbol"] == "Point")
            self.assertTrue(point["anonymous"])
            self.assertEqual(point["members"], ["x", "y", "flags", "mode"])
            self.assertEqual([item["symbol"] for item in constants],
                             ["EXAMPLE_LIMIT"])
            table = next(item for item in data if item["symbol"] == "sFunctions")
            self.assertEqual(table["_raw_references"], ["First", "Second"])
            by_symbol = {item["symbol"]: item for item in data}
            self.assertLessEqual({"sFirst", "sSecond", "sThird"}, set(by_symbol))
            self.assertEqual(by_symbol["sSecond"]["_raw_references"], [])
            self.assertEqual(by_symbol["sThird"]["linkage"], "internal")

    def test_tracked_inventory_covers_both_pinned_sources(self):
        inventory = native_inventory.load_inventory()
        self.assertEqual(inventory["schema"], native_inventory.SCHEMA)
        self.assertGreater(inventory["games"]["mzm"]["statistics"]["routines"], 2500)
        self.assertGreater(inventory["games"]["aos"]["statistics"]["routines"], 3000)
        for game in ("mzm", "aos"):
            self.assertEqual(len(inventory["games"][game]["source_revision"]), 40)
            self.assertTrue(inventory["games"][game]["routines"])
            self.assertTrue(inventory["games"][game]["declarations"])
            self.assertTrue(inventory["games"][game]["data_symbols"])
            self.assertTrue(inventory["games"][game]["types"])
            self.assertTrue(inventory["games"][game]["constants"])
        mzm = inventory["games"]["mzm"]
        handler = next(item for item in mzm["routines"]
                       if item["symbol"] == "SamusExecutePoseHandler")
        tables = {item["table"] for item in handler["indirect_calls"]}
        self.assertIn("mzm:data:src/samus.c:sSamusPoseFunctionPointers", tables)
        self.assertGreater(mzm["statistics"]["resolved_indirect_call_edges"], 0)
        for path in native_inventory.inventory_output_paths():
            self.assertLess(path.stat().st_size, 4 * 1024 * 1024)

    def test_shards_are_manifested_and_checksum_validated(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "inventory.json"
            game = {
                "title": "Fixture",
                "source_root": "third_party/fixture",
                "source_revision": "0" * 40,
                "statistics": {},
                "coverage": {},
                "files": {},
                "routines": [{"id": "fixture:routine", "symbol": "Run"}],
                "declarations": [],
                "data_symbols": [],
                "types": [],
                "constants": [],
            }
            document = {
                "schema": native_inventory.SCHEMA,
                "generator_version": native_inventory.GENERATOR_VERSION,
                "evidence": "fixture",
                "games": {"fixture": game},
            }
            native_inventory.write_inventory(document, output)

            loaded = native_inventory.load_inventory(output)

            self.assertEqual(loaded["games"]["fixture"]["routines"],
                             game["routines"])
            shard = native_inventory.inventory_output_paths(output)[1]
            shard.write_bytes(shard.read_bytes() + b" ")
            with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                native_inventory.load_inventory(output)


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
        self.assertIn("not game fidelity percentages", generated)

    def test_annotation_tsv_round_trip_is_stable(self):
        text = native_parity.ANNOTATIONS.read_text(encoding="utf-8")

        annotations = native_parity.load_annotations(native_parity.ANNOTATIONS)

        self.assertEqual(native_parity.format_annotations(annotations), text)
        reparsed = native_parity.load_annotations(native_parity.ANNOTATIONS)
        self.assertEqual(reparsed, annotations)

    def test_annotation_tsv_rejects_malformed_rows(self):
        header = "\t".join(native_parity.ANNOTATION_COLUMNS)
        valid = ("mzm\tmzm.player.x\tplayer\tpartial\tTitle\tSamusUpdate\t"
                 "src/runtime/mzm_samus.c\ttests/test_mzm_samus.c\t\t\tNotes\tNext")
        cases = {
            "bad header": ("game\tid\n", "header"),
            "column count": (f"{header}\nmzm\tonly.two.columns\n", "columns"),
            "unknown game": (f"{header}\n{valid.replace('mzm', 'other', 1)}\n",
                             "unknown game"),
            "game prefix": (f"{header}\n{valid.replace('mzm.player', 'aos.player')}\n",
                            "must start with"),
            "unsorted rows": (f"{header}\n{valid}\n"
                              f"{valid.replace('mzm.player.x', 'mzm.player.a')}\n",
                              "out of order or duplicated"),
            "list pipe in scalar": (f"{header}\n"
                                    f"{valid.replace('Title', 'a|b')}\n",
                                    "not allowed"),
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "annotations.tsv"
            for name, (text, message) in cases.items():
                with self.subTest(case=name):
                    path.write_text(text, encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, message):
                        native_parity.load_annotations(path)

    def test_validate_rejects_unknown_dependency(self):
        inventory = native_parity._load(native_parity.INVENTORY)
        annotations = native_parity._load(native_parity.ANNOTATIONS)
        annotations["games"]["mzm"]["features"][0]["dependencies"] = [
            "mzm.player.does_not_exist"]

        with self.assertRaisesRegex(ValueError, "unknown dependency"):
            native_parity.validate(inventory, annotations)

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
