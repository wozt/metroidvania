# SPDX-License-Identifier: GPL-3.0-only
"""Compare the native Aria area directory with the Python reference."""
from __future__ import annotations

import hashlib
from pathlib import Path
import json
import struct
import subprocess
import tempfile
import unittest

from scripts.import_aos_world import (
    GBA_ROM_BASE,
    decode_area_directory,
    decode_entities,
    decode_load_list,
    decode_map_cells,
    decode_room_backgrounds,
    decode_room_descriptor,
    decode_room_transitions,
    decode_world,
)
from scripts.aos_room_index import index_rows
from scripts.aos_room_render import (
    decode_background,
    load_video_memory,
    render_background,
)
from scripts.aos_runtime_room import native_cell


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/fusion_importer"
ROM = ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba"


@unittest.skipUnless(BINARY.exists() and ROM.exists(),
                     "native importer or local Aria ROM is unavailable")
class FusionAssetsAosEquivalenceTests(unittest.TestCase):
    def test_area_room_counts_match_python_reference(self):
        _, expected_counts = decode_area_directory(ROM.read_bytes())
        result = subprocess.run(
            [str(BINARY), "--aria-directory", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        fields = dict(line.split("=", 1) for line in result.stdout.splitlines())
        actual_counts = [int(fields[f"area_{area:02d}"])
                         for area in range(len(expected_counts))]
        self.assertEqual(actual_counts, expected_counts)
        self.assertEqual(int(fields["total_rooms"]), sum(expected_counts))

    def test_global_map_cells_match_python_reference(self):
        rom = ROM.read_bytes()
        area_pointers, room_counts = decode_area_directory(rom)
        expected = decode_map_cells(rom, area_pointers, room_counts)
        result = subprocess.run(
            [str(BINARY), "--aria-map", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            kind, x, y, area, room, save, warp, pointer = line.split("\t")
            self.assertEqual(kind, "cell")
            actual.append((int(x), int(y), int(area), int(room),
                           int(save), int(warp), int(pointer, 16)))
        expected_values = [
            (cell["map_x"], cell["map_y"], cell["engine_area"], cell["room"],
             int(cell["save"]), int(cell["warp"]),
             int(cell["room_pointer"], 16))
            for cell in expected
        ]
        self.assertEqual(actual, expected_values)
        kind, count, saves, warps = lines[-1].split("\t")
        self.assertEqual(kind, "summary")
        self.assertEqual(int(count), len(expected))
        self.assertEqual(int(saves), sum(cell["save"] for cell in expected))
        self.assertEqual(int(warps), sum(cell["warp"] for cell in expected))

    def test_room_descriptor_headers_match_python_reference(self):
        rom = ROM.read_bytes()
        area_pointers, room_counts = decode_area_directory(rom)
        expected = []
        for area, (table_pointer, room_count) in enumerate(
                zip(area_pointers, room_counts)):
            table_offset = table_pointer - GBA_ROM_BASE
            for room in range(room_count):
                source = struct.unpack_from("<I", rom, table_offset + room * 4)[0]
                header = decode_room_descriptor(rom, source)
                expected.append((
                    area, room, header["source_pointer"], header["resolved_pointer"],
                    header["display_control"], header["next_pointer"],
                    header["backgrounds_pointer"], header["graphics_pointer"],
                    header["palette_pointer"], header["entities_pointer"],
                    header["transitions_pointer"], header["transition_count"],
                    header["blend_control"], header["map_x"], header["map_y"],
                    *header["flags"],
                    tuple((item["pointer"], item["selector"])
                          for item in header["variant_chain"]),
                ))
        result = subprocess.run(
            [str(BINARY), "--aria-rooms", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = []
        for line in result.stdout.splitlines():
            fields = line.split("\t")
            self.assertEqual(fields[0], "room")
            self.assertEqual(int(fields[18]), len(fields[19].split(",")))
            variants = tuple(
                (int(pointer, 16), int(selector, 16))
                for pointer, selector in (item.split(":")
                                          for item in fields[19].split(","))
            )
            actual.append(tuple(int(value, 0) for value in fields[1:18]) +
                          (variants,))
        self.assertEqual(actual, expected)

    def test_room_transitions_and_targets_match_python_reference(self):
        rom = ROM.read_bytes()
        area_pointers, room_counts = decode_area_directory(rom)
        rooms = []
        locations = {}
        for area, (table_pointer, room_count) in enumerate(
                zip(area_pointers, room_counts)):
            table_offset = table_pointer - GBA_ROM_BASE
            for room in range(room_count):
                source = struct.unpack_from("<I", rom, table_offset + room * 4)[0]
                self.assertNotIn(source, locations)
                locations[source] = (area, room)
                rooms.append((area, room, decode_room_descriptor(rom, source)))
        expected = []
        for area, room, descriptor in rooms:
            for index, transition in enumerate(
                    decode_room_transitions(rom, descriptor)):
                target_area, target_room = locations[transition["target_pointer"]]
                expected.append((
                    area, room, index, transition["entry_pointer"],
                    transition["target_pointer"], transition["source_screen_x"],
                    transition["source_screen_y"], transition["field_6"],
                    transition["field_8"], transition["load_x"],
                    transition["load_y"], transition["field_e"],
                    target_area, target_room,
                ))
        result = subprocess.run(
            [str(BINARY), "--aria-transitions", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = []
        for line in result.stdout.splitlines():
            fields = line.split("\t")
            self.assertEqual(fields[0], "transition")
            actual.append(tuple(int(value, 0) for value in fields[1:]))
        self.assertEqual(actual, expected)

    def test_room_background_headers_match_python_reference(self):
        rom = ROM.read_bytes()
        area_pointers, room_counts = decode_area_directory(rom)
        expected = []
        for area, (table_pointer, room_count) in enumerate(
                zip(area_pointers, room_counts)):
            table_offset = table_pointer - GBA_ROM_BASE
            for room in range(room_count):
                source = struct.unpack_from("<I", rom, table_offset + room * 4)[0]
                descriptor = decode_room_descriptor(rom, source)
                for background in decode_room_backgrounds(
                        rom, descriptor["backgrounds_pointer"]):
                    expected.append((
                        area, room, background["layer"],
                        background["descriptor_pointer"], background["field_0"],
                        background["field_1"], background["control"],
                        background["field_4"], background["field_6"],
                        background["metadata_pointer"],
                        (-1 if background["width_screens"] is None
                         else background["width_screens"]),
                        (-1 if background["height_screens"] is None
                         else background["height_screens"]),
                    ))
        result = subprocess.run(
            [str(BINARY), "--aria-backgrounds", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = []
        for line in result.stdout.splitlines():
            fields = line.split("\t")
            self.assertEqual(fields[0], "background")
            actual.append(tuple(int(value, 0) for value in fields[1:]))
        self.assertEqual(actual, expected)

    def test_background_tilemaps_match_python_reference(self):
        rom = ROM.read_bytes()
        expected = []
        status_counts = {"decoded": 0, "absent": 0, "affine": 0}
        total_cells = 0
        for room in decode_world(rom)["rooms"]:
            for background in room["backgrounds"]:
                pointer_text = background["metadata_pointer"]
                pointer = int(pointer_text, 16) if pointer_text is not None else 0
                decoded = decode_background(rom, background)
                if decoded["status"] == "ABSENT":
                    values = ("absent", pointer, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, "-")
                else:
                    offset = pointer - GBA_ROM_BASE
                    width, height, flags, blocks, _, block_map = \
                        struct.unpack_from("<BBHIII", rom, offset)
                    if decoded["status"] == "UNSUPPORTED_AFFINE":
                        values = ("affine", pointer, width, height, flags,
                                  blocks, block_map, 0, 0, 0, 0, "-")
                    else:
                        map_count = width * height * 64
                        map_offset = block_map - GBA_ROM_BASE
                        map_values = struct.unpack_from(
                            f"<{map_count}H", rom, map_offset)
                        maximum_block = max(
                            (value & 0x3FFF for value in map_values), default=0)
                        payload = struct.pack(
                            f"<{len(decoded['tiles'])}H", *decoded["tiles"])
                        values = (
                            "decoded", pointer, width, height, flags, blocks,
                            block_map, maximum_block, decoded["width_tiles"],
                            decoded["height_tiles"], len(decoded["tiles"]),
                            hashlib.sha1(payload).hexdigest(),
                        )
                        total_cells += len(decoded["tiles"])
                status_counts[values[0]] += 1
                expected.append((room["engine_area"], room["room"],
                                 background["layer"], *values))

        result = subprocess.run(
            [str(BINARY), "--aria-background-tiles", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "tilemap")
            actual.append((int(fields[1]), int(fields[2]), int(fields[3]),
                           fields[4],
                           *(int(value, 0) for value in fields[5:15]),
                           fields[15]))
        self.assertEqual(actual, expected)
        self.assertEqual(
            lines[-1],
            "summary\t{}\t{}\t{}\t{}".format(
                status_counts["decoded"], status_counts["absent"],
                status_counts["affine"], total_cells),
        )

    def test_background_renders_match_python_reference(self):
        rom = ROM.read_bytes()
        resource_cache = {}
        expected = []
        skipped_count = 0
        total_pixels = 0
        unresolved_count = 0
        for room in decode_world(rom)["rooms"]:
            vram, palette, _ = load_video_memory(rom, room, resource_cache)
            for background in room["backgrounds"]:
                decoded = decode_background(rom, background)
                if decoded["status"] != "DECODED_TEXT_BACKGROUND":
                    skipped_count += 1
                    continue
                rgba, unresolved = render_background(
                    decoded, vram, palette)
                width = decoded["width_tiles"] * 8
                height = decoded["height_tiles"] * 8
                pixels = width * height
                expected.append((
                    room["engine_area"], room["room"], background["layer"],
                    width, height, pixels, unresolved,
                    hashlib.sha1(rgba).hexdigest(),
                ))
                total_pixels += pixels
                unresolved_count += unresolved

        result = subprocess.run(
            [str(BINARY), "--aria-background-renders", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "render")
            actual.append(tuple(int(value) for value in fields[1:8]) +
                          (fields[8],))
        self.assertEqual(actual, expected)
        self.assertEqual(
            lines[-1],
            f"summary\t{len(expected)}\t{skipped_count}\t"
            f"{total_pixels}\t{unresolved_count}",
        )

    def test_bg1_collision_maps_match_python_reference(self):
        rom = ROM.read_bytes()
        area_pointers, room_counts = decode_area_directory(rom)
        expected = []
        status_counts = {"decoded": 0, "absent": 0, "affine": 0, "none": 0}
        total_cells = 0
        for area, (table_pointer, room_count) in enumerate(
                zip(area_pointers, room_counts)):
            table_offset = table_pointer - GBA_ROM_BASE
            for room in range(room_count):
                source = struct.unpack_from("<I", rom, table_offset + room * 4)[0]
                descriptor = decode_room_descriptor(rom, source)
                bg1 = decode_room_backgrounds(
                    rom, descriptor["backgrounds_pointer"])[0]
                pointer = bg1["metadata_pointer"] or 0
                if pointer == 0:
                    values = ("absent", 0, 0, 0, 0, 0, 0, 0, 0, 0, "-")
                else:
                    offset = pointer - GBA_ROM_BASE
                    width, height, flags, blocks, collision, block_map = \
                        struct.unpack_from("<BBHIII", rom, offset)
                    decoded = decode_background(
                        rom, dict(bg1, metadata_pointer=f"0x{pointer:08x}"))
                    if decoded["status"] == "UNSUPPORTED_AFFINE":
                        values = ("affine", width, height, flags, blocks,
                                  collision, block_map, 0, 0, 0, "-")
                    else:
                        map_count = width * height * 64
                        map_offset = block_map - GBA_ROM_BASE
                        map_values = struct.unpack_from(
                            f"<{map_count}H", rom, map_offset)
                        maximum_block = max(
                            (value & 0x3FFF for value in map_values), default=0)
                        if decoded["collision"] is None:
                            values = ("none", width, height, flags, blocks,
                                      collision, block_map, maximum_block,
                                      0, 0, "-")
                        else:
                            cells = bytes(native_cell(value, flipped)
                                          for value, flipped in zip(
                                              decoded["collision"],
                                              decoded["collision_xflip"]))
                            values = (
                                "decoded", width, height, flags, blocks,
                                collision, block_map, maximum_block,
                                decoded["width_tiles"], decoded["height_tiles"],
                                hashlib.sha1(cells).hexdigest(),
                            )
                            total_cells += len(cells)
                status_counts[values[0]] += 1
                cell_count = values[8] * values[9]
                expected.append((area, room, values[0], pointer, *values[1:10],
                                 cell_count, values[10]))

        result = subprocess.run(
            [str(BINARY), "--aria-collisions", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "collision")
            actual.append((int(fields[1]), int(fields[2]), fields[3],
                           *(int(value, 0) for value in fields[4:15]),
                           fields[15]))
        self.assertEqual(actual, expected)
        self.assertEqual(
            lines[-1],
            "summary\t{}\t{}\t{}\t{}\t{}".format(
                status_counts["decoded"], status_counts["absent"],
                status_counts["affine"], status_counts["none"], total_cells),
        )

    def test_room_loading_lists_match_python_reference(self):
        rom = ROM.read_bytes()
        area_pointers, room_counts = decode_area_directory(rom)
        expected = []
        for area, (table_pointer, room_count) in enumerate(
                zip(area_pointers, room_counts)):
            table_offset = table_pointer - GBA_ROM_BASE
            for room in range(room_count):
                source = struct.unpack_from("<I", rom, table_offset + room * 4)[0]
                descriptor = decode_room_descriptor(rom, source)
                for kind, pointer in (
                        ("graphics", descriptor["graphics_pointer"]),
                        ("palette", descriptor["palette_pointer"])):
                    for index, entry in enumerate(decode_load_list(rom, pointer)):
                        expected.append((
                            area, room, kind, index, entry["entry_pointer"],
                            entry["resource_pointer"], *entry["parameters"],
                        ))
        result = subprocess.run(
            [str(BINARY), "--aria-loads", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = []
        for line in result.stdout.splitlines():
            fields = line.split("\t")
            self.assertEqual(fields[0], "load")
            actual.append((int(fields[1]), int(fields[2]), fields[3],
                           *(int(value, 0) for value in fields[4:])))
        self.assertEqual(actual, expected)

    def test_room_video_memory_matches_python_reference(self):
        rom = ROM.read_bytes()
        resource_cache = {}
        expected = []
        graphics_count = 0
        palette_count = 0
        world = decode_world(rom)
        for room in world["rooms"]:
            vram, palette, _ = load_video_memory(rom, room, resource_cache)
            graphics = len(room["graphics_loads"])
            palettes = len(room["palette_loads"])
            expected.append((
                room["engine_area"], room["room"], graphics, palettes,
                hashlib.sha1(vram).hexdigest(),
                hashlib.sha1(palette).hexdigest(),
            ))
            graphics_count += graphics
            palette_count += palettes

        result = subprocess.run(
            [str(BINARY), "--aria-video-memory", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            kind, area, room, graphics, palettes, vram, palette = line.split("\t")
            self.assertEqual(kind, "video")
            actual.append((int(area), int(room), int(graphics), int(palettes),
                           vram, palette))
        self.assertEqual(actual, expected)
        self.assertEqual(
            lines[-1],
            f"summary\t{len(expected)}\t{graphics_count}\t{palette_count}",
        )

    def test_room_entity_lists_match_python_reference(self):
        rom = ROM.read_bytes()
        area_pointers, room_counts = decode_area_directory(rom)
        expected = []
        for area, (table_pointer, room_count) in enumerate(
                zip(area_pointers, room_counts)):
            table_offset = table_pointer - GBA_ROM_BASE
            for room in range(room_count):
                source = struct.unpack_from("<I", rom, table_offset + room * 4)[0]
                descriptor = decode_room_descriptor(rom, source)
                for index, entity in enumerate(
                        decode_entities(rom, descriptor["entities_pointer"])):
                    expected.append((
                        area, room, index, entity["entry_pointer"], entity["x"],
                        entity["y"], entity["persistent_index"], entity["kind"],
                        entity["entity_id"], entity["flags"],
                        *entity["parameters"],
                    ))
        result = subprocess.run(
            [str(BINARY), "--aria-entities", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = []
        for line in result.stdout.splitlines():
            fields = line.split("\t")
            self.assertEqual(fields[0], "entity")
            actual.append(tuple(int(value, 0) for value in fields[1:]))
        self.assertEqual(actual, expected)

    def test_audited_boss_rooms_match_python_reference(self):
        expected = []
        for index, boss in enumerate(decode_world(ROM.read_bytes())["boss_rooms"]):
            expected.append((
                index, boss["id"], boss["name"], boss["phase"],
                boss["engine_area"], boss["room"], boss["map_x"], boss["map_y"],
                int(boss["room_pointer"], 16), int(boss["entity_pointer"], 16),
                boss["entity_id"], int(boss["enemy_table_pointer"], 16),
                boss["health"],
            ))
        result = subprocess.run(
            [str(BINARY), "--aria-bosses", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = []
        for line in result.stdout.splitlines():
            fields = line.split("\t")
            self.assertEqual(fields[0], "boss")
            actual.append((
                int(fields[1]), fields[2], fields[3], fields[4],
                *(int(value, 0) for value in fields[5:]),
            ))
        self.assertEqual(actual, expected)

    def test_world_summary_matches_python_reference(self):
        world = decode_world(ROM.read_bytes())
        keys = (
            "native_room_count", "mapped_cells", "transition_count",
            "entity_count", "boss_room_count", "savepoint_count", "warp_count",
        )
        result = subprocess.run(
            [str(BINARY), "--aria-summary", str(ROM)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = dict(line.split("=", 1) for line in result.stdout.splitlines())
        self.assertEqual(set(actual), set(keys))
        self.assertEqual({key: int(actual[key]) for key in keys},
                         {key: world[key] for key in keys})

    def test_native_world_catalog_matches_complete_python_structure(self):
        expected = decode_world(ROM.read_bytes())
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "world.json"
            index = Path(directory) / "rooms.tsv"
            command = [str(BINARY), "--aria-catalog", str(ROM), str(output),
                       str(index)]
            result = subprocess.run(
                command, cwd=ROOT, text=True, capture_output=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            first = output.read_bytes()
            first_index = index.read_bytes()
            self.assertEqual(json.loads(first), expected)
            self.assertEqual(first_index,
                             ("\n".join(index_rows(expected)) + "\n").encode())
            result = subprocess.run(
                command, cwd=ROOT, text=True, capture_output=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(output.read_bytes(), first)
            self.assertEqual(index.read_bytes(), first_index)
            self.assertFalse(output.with_suffix(".json.tmp").exists())
            self.assertFalse(index.with_suffix(".tsv.tmp").exists())
