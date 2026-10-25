# SPDX-License-Identifier: GPL-3.0-only
"""Compare the native Zero Mission ROM-symbol reader with Python."""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from scripts import mzm_runtime_room
from scripts.mzm_c_tables import enum_values, evaluate
from scripts.mzm_bg_layers import (
    metatile_sheet,
    render_block_background,
    select_bg12,
    symbol_bytes,
    text_background,
    tileset_entry,
    tileset_metatiles,
    tileset_video_memory,
)
from scripts.mzm_rom_bank import elf_symbols, produce, symbol_lines
from scripts.mzm_room_render import bmp24, lz77, rle_room, source_rooms


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/fusion_importer"
ROM = ROOT / "roms/Metroid - Zero Mission (USA).gba"
ELF = ROOT / "third_party/mzm/mzm_us.elf"


@unittest.skipUnless(BINARY.exists() and ROM.exists() and ELF.exists(),
                     "native importer, local Zero Mission ROM or ELF is unavailable")
class FusionAssetsMzmEquivalenceTests(unittest.TestCase):
    def test_rom_symbols_match_python_reference(self):
        symbols = elf_symbols(ELF.read_bytes())
        expected, duplicates = symbol_lines(symbols, ROM.stat().st_size)
        result = subprocess.run(
            [str(BINARY), "--mzm-symbols", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(lines[:-1], expected)
        self.assertEqual(lines[-1],
                         f"summary\t{len(expected)}\t{len(duplicates)}")

    def test_serialized_bank_and_manifest_match_python_byte_for_byte(self):
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            python_root = temporary / "python"
            native_output = temporary / "native"
            python_root.mkdir()
            native_output.mkdir()
            result = produce(python_root, ROM, ELF.parent)
            native = subprocess.run(
                [str(BINARY), "--mzm-rom-bank", str(ROM), str(ELF),
                 str(native_output)],
                cwd=ROOT, text=True, capture_output=True, check=False,
            )
            self.assertEqual(native.returncode, 0, native.stderr)
            for name in ("rom_bank.bin", "rom_symbols.tsv"):
                self.assertEqual((native_output / name).read_bytes(),
                                 (result["folder"] / name).read_bytes())
                self.assertFalse((native_output / f"{name}.tmp").exists())

    def test_room_entries_match_all_source_descriptors(self):
        symbol_rows, _ = symbol_lines(elf_symbols(ELF.read_bytes()),
                                      ROM.stat().st_size)
        symbols = {parts[1]: int(parts[2])
                   for row in symbol_rows
                   if (parts := row.split("\t"))[0] == "symbol"}
        names = enum_values()
        scalar_fields = (
            "tileset", "bg0Prop", "bg1Prop", "bg2Prop", "bg3Prop",
        )
        tail_fields = (
            "bg3Scrolling", "transparency", "defaultSpriteset",
            "firstSpritesetEvent", "firstSpriteset", "secondSpritesetEvent",
            "secondSpriteset", "mapX", "mapY", "effect", "effectY",
            "musicTrack",
        )
        pointer_fields = (
            "pBg0Data", "pBg1Data", "pBg2Data", "pClipData", "pBg3Data",
            "pDefaultSpriteData", "pFirstSpriteData", "pSecondSpriteData",
        )
        expected = []
        for room in source_rooms():
            fields = room["fields"]
            table = symbols[f"s{room['area']}RoomEntries"]
            scalars = [evaluate(fields.get(field, "0"), names)
                       for field in scalar_fields]
            pointers = [symbols[fields[field]] for field in pointer_fields]
            tails = [evaluate(fields.get(field, "0"), names)
                     for field in tail_fields]
            values = [table + room["index"] * 60, *scalars,
                      *pointers[:5], *tails[:2], pointers[5],
                      tails[2], tails[3], pointers[6], tails[4], tails[5],
                      pointers[7], *tails[6:]]
            expected.append((room["area"], room["index"], values))

        result = subprocess.run(
            [str(BINARY), "--mzm-rooms", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        area_rows = []
        for line in lines:
            parts = line.split("\t")
            if parts[0] == "area":
                area_rows.append((parts[2], int(parts[3]), int(parts[4])))
            elif parts[0] == "room":
                actual.append((parts[1], int(parts[2]),
                               [int(value) for value in parts[3:]]))
        self.assertEqual(actual, expected)
        grouped = []
        for area, _, _ in expected:
            if not grouped or grouped[-1] != area:
                grouped.append(area)
        expected_areas = []
        for area in grouped:
            rooms = [room for room in expected if room[0] == area]
            expected_areas.append((area, len(rooms),
                                   symbols[f"s{area}RoomEntries"]))
        self.assertEqual(area_rows, expected_areas)
        self.assertEqual(lines[-1], f"summary\t{len(expected)}")

    def test_clipdata_maps_match_python_reference(self):
        rom = ROM.read_bytes()
        symbol_rows, _ = symbol_lines(elf_symbols(ELF.read_bytes()), len(rom))
        symbols = {parts[1]: int(parts[2])
                   for row in symbol_rows
                   if (parts := row.split("\t"))[0] == "symbol"}
        expected = []
        expected_cores = []
        expected_doors = []
        expected_hatches = []
        total_cells = 0
        total_clipdata = total_transitions = total_movements = 0
        total_doors = total_exits = total_locations = 0
        total_hatches = total_hatch_locks = 0
        total_core_bytes = 0
        total_door_bytes = 0
        total_hatch_bytes = 0
        tables = mzm_runtime_room.collision_tables()
        rooms = source_rooms()
        doors_by_area = mzm_runtime_room.door_tables()
        targets_by_area = {
            area: mzm_runtime_room.area_targets(area)
            for area in {room["area"] for room in rooms}
        }
        hatch_types, hatch_behaviors = mzm_runtime_room.hatch_tables()
        hatch_metadata = mzm_runtime_room.hatch_animation_lines(
            tables, hatch_behaviors,
        )
        for room in rooms:
            pointer = symbols[room["fields"]["pClipData"]]
            width, height, cells = rle_room(rom[pointer - 0x08000000:])
            canonical = struct.pack(f"<{len(cells)}H", *cells)
            expected.append((room["area"], room["index"], pointer,
                             width, height, len(cells),
                             hashlib.sha1(canonical).hexdigest()))
            total_cells += len(cells)
            payload = mzm_runtime_room.encode_room(
                room["area"], room["index"], width * 16, height * 16,
                (width, height, cells), tables,
            ).encode()
            lines = payload.splitlines()
            clipdata_count = sum(line.startswith(b"C\t") for line in lines)
            transition_count = sum(line.startswith(b"B\t") for line in lines)
            movement_count = sum(line.startswith(b"M\t") for line in lines)
            expected_cores.append((
                room["area"], room["index"], width, height, len(cells),
                clipdata_count, transition_count, movement_count, len(payload),
                hashlib.sha1(payload).hexdigest(),
            ))
            total_clipdata += clipdata_count
            total_transitions += transition_count
            total_movements += movement_count
            total_core_bytes += len(payload)

            door_payload = mzm_runtime_room.encode_room(
                room["area"], room["index"], width * 16, height * 16,
                (width, height, cells), tables,
                doors=doors_by_area.get(room["area"], []),
                targets=targets_by_area[room["area"]],
            ).encode()
            door_lines = door_payload.splitlines()
            door_count = sum(line.startswith(b"D\t") for line in door_lines)
            exit_count = sum(line.startswith(b"X\t") for line in door_lines)
            location_count = sum(line.startswith(b"Y\t") for line in door_lines)
            expected_doors.append((
                room["area"], room["index"], width, height, len(cells),
                clipdata_count, transition_count, movement_count,
                door_count, exit_count, location_count, len(door_payload),
                hashlib.sha1(door_payload).hexdigest(),
            ))
            total_doors += door_count
            total_exits += exit_count
            total_locations += location_count
            total_door_bytes += len(door_payload)

            room_doors = doors_by_area.get(room["area"], [])
            room_hatches = mzm_runtime_room.room_hatches(
                room_doors, room["index"], (width, height, cells), tables,
                hatch_types, hatch_behaviors,
            )
            lock_lines = mzm_runtime_room.hatch_lock_lines(
                room["area"], room["index"], room_doors,
            ) if room["area"].isalpha() else []
            metadata = [*hatch_metadata, *lock_lines]
            hatch_payload = mzm_runtime_room.encode_room(
                room["area"], room["index"], width * 16, height * 16,
                (width, height, cells), tables, doors=room_doors,
                hatches=room_hatches, targets=targets_by_area[room["area"]],
                backgrounds=metadata,
            ).encode()
            hatch_lines = hatch_payload.splitlines()
            hatch_count = sum(line.startswith(b"H\t") for line in hatch_lines)
            hatch_lock_count = sum(
                line.startswith(b"Q\t") for line in hatch_lines
            )
            expected_hatches.append((
                room["area"], room["index"], width, height, len(cells),
                clipdata_count, transition_count, movement_count,
                door_count, exit_count, location_count, hatch_count,
                hatch_lock_count, len(hatch_payload),
                hashlib.sha1(hatch_payload).hexdigest(),
            ))
            total_hatches += hatch_count
            total_hatch_locks += hatch_lock_count
            total_hatch_bytes += len(hatch_payload)

        result = subprocess.run(
            [str(BINARY), "--mzm-clipdata", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "clipdata")
            self.assertGreaterEqual(int(fields[7]), 6)
            actual.append((fields[1], int(fields[2]), int(fields[3], 0),
                           int(fields[4]), int(fields[5]), int(fields[6]),
                           fields[8]))
        self.assertEqual(actual, expected)
        self.assertEqual(lines[-1], f"summary\t{len(expected)}\t{total_cells}")

        result = subprocess.run(
            [str(BINARY), "--mzm-runtime-room-cores", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        output_lines = result.stdout.splitlines()
        actual_cores = []
        for line in output_lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "room")
            actual_cores.append((fields[1], *(int(value) for value in fields[2:10]),
                                 fields[10]))
        self.assertEqual(actual_cores, expected_cores)
        self.assertEqual(
            output_lines[-1],
            f"summary\t{len(expected_cores)}\t{total_cells}\t"
            f"{total_clipdata}\t{total_transitions}\t{total_movements}\t"
            f"{total_core_bytes}",
        )

        result = subprocess.run(
            [str(BINARY), "--mzm-runtime-room-doors", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        output_lines = result.stdout.splitlines()
        actual_doors = []
        for line in output_lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "room")
            actual_doors.append((
                fields[1], *(int(value) for value in fields[2:13]), fields[13],
            ))
        self.assertEqual(actual_doors, expected_doors)
        self.assertEqual(
            output_lines[-1],
            f"summary\t{len(expected_doors)}\t{total_cells}\t"
            f"{total_clipdata}\t{total_transitions}\t{total_movements}\t"
            f"{total_doors}\t{total_exits}\t{total_locations}\t"
            f"{total_door_bytes}",
        )

        result = subprocess.run(
            [str(BINARY), "--mzm-runtime-room-hatches", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        output_lines = result.stdout.splitlines()
        actual_hatches = []
        for line in output_lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "room")
            actual_hatches.append((
                fields[1], *(int(value) for value in fields[2:15]), fields[15],
            ))
        self.assertEqual(actual_hatches, expected_hatches)
        self.assertEqual(
            output_lines[-1],
            f"summary\t{len(expected_hatches)}\t{total_cells}\t"
            f"{total_clipdata}\t{total_transitions}\t{total_movements}\t"
            f"{total_doors}\t{total_exits}\t{total_locations}\t"
            f"{total_hatches}\t{total_hatch_locks}\t{total_hatch_bytes}",
        )

    def test_room_background_payloads_match_python_reference(self):
        rom = ROM.read_bytes()
        symbol_rows, _ = symbol_lines(elf_symbols(ELF.read_bytes()), len(rom))
        symbols = {parts[1]: int(parts[2])
                   for row in symbol_rows
                   if (parts := row.split("\t"))[0] == "symbol"}
        symbol_sizes = {parts[1]: int(parts[3])
                        for row in symbol_rows
                        if (parts := row.split("\t"))[0] == "symbol"}
        names = enum_values()
        expected = []
        expected_renders = []
        expected_composites = []
        sheets = {}
        counts = {"rle": 0, "text": 0, "none": 0, "unsupported": 0}
        total_cells = 0
        total_rendered_pixels = 0
        selection_count = missing_bg1 = mismatched_composites = 0
        bg12_count = bg1_only_count = 0
        composite_pixels = composite_bg1 = composite_bg2 = 0
        composite_unresolved = composite_bitmap_bytes = composite_mask_bytes = 0
        for room in source_rooms():
            fields = room["fields"]
            tileset = evaluate(fields.get("tileset", "0"), names)
            room_renders = {}
            room_statuses = {}
            for layer in range(4):
                property_value = evaluate(fields.get(f"bg{layer}Prop", "0"), names)
                symbol = fields[f"pBg{layer}Data"]
                pointer = symbols[symbol]
                offset = pointer - 0x08000000
                size_flag = -1
                width = height = cell_count = 0
                digest = "-"
                if property_value & 0x10:
                    status = "rle"
                    width, height, cells = rle_room(rom[offset:])
                    payload = struct.pack(f"<{len(cells)}H", *cells)
                    cell_count = len(cells)
                    digest = hashlib.sha1(payload).hexdigest()
                    if tileset not in sheets:
                        sheets[tileset] = metatile_sheet(tileset)
                    rgba = render_block_background(
                        width, height, cells, tileset,
                        sheet=sheets[tileset],
                    )
                    pixel_width, pixel_height = width * 16, height * 16
                elif property_value & 0x40:
                    status = "text"
                    size_flag = rom[offset]
                    width = 32 if size_flag in (0, 2) else 64
                    height = 64 if size_flag == 2 else 32
                    payload = lz77(rom[offset + 4:
                                       offset + symbol_sizes[symbol]])
                    cell_count = len(payload) // 2
                    self.assertEqual(len(payload), width * height * 2)
                    digest = hashlib.sha1(payload).hexdigest()
                    _, pixel_width, pixel_height, rgba = text_background(
                        symbol, tileset)
                else:
                    status = "none" if property_value == 0 else "unsupported"
                if status in ("rle", "text"):
                    pixels = pixel_width * pixel_height
                    room_renders[layer] = (pixel_width, pixel_height, rgba)
                    expected_renders.append((
                        room["area"], room["index"], layer, status,
                        pixel_width, pixel_height, pixels, 0,
                        hashlib.sha1(rgba).hexdigest(),
                    ))
                    total_rendered_pixels += pixels
                counts[status] += 1
                room_statuses[layer] = status
                total_cells += cell_count
                expected.append((
                    room["area"], room["index"], layer, status,
                    property_value, pointer, size_flag, width, height,
                    cell_count, digest,
                ))
            if 1 not in room_renders:
                expected_composites.append(
                    f"skipped\t{room['area']}\t{room['index']}\tmissing-bg1\t"
                    f"{room_statuses[1]}\t{room_statuses[2]}"
                )
                missing_bg1 += 1
            else:
                width, height, bg1 = room_renders[1]
                bg2 = None
                if 2 in room_renders:
                    if room_renders[2][:2] == (width, height):
                        _, _, bg2 = room_renders[2]
                    else:
                        mismatched_composites += 1
                rgb, visibility, coverage = select_bg12(bg1, bg2)
                mode = "bg12" if bg2 is not None else "bg1"
                pixels = width * height
                bitmap = bmp24(width, height, rgb)
                expected_composites.append(
                    f"background\t{room['area']}\t{room['index']}\t{mode}\t"
                    f"{width}\t{height}\t{pixels}\t"
                    f"{coverage['bg1_visible_pixels']}\t"
                    f"{coverage['bg2_visible_pixels']}\t"
                    f"{coverage['unresolved_pixels']}\t"
                    f"{hashlib.sha1(rgb).hexdigest()}\t"
                    f"{len(bitmap)}\t{hashlib.sha1(bitmap).hexdigest()}\t"
                    f"{len(visibility)}\t"
                    f"{hashlib.sha1(visibility).hexdigest()}"
                )
                selection_count += 1
                if bg2 is None:
                    bg1_only_count += 1
                else:
                    bg12_count += 1
                composite_pixels += pixels
                composite_bg1 += coverage["bg1_visible_pixels"]
                composite_bg2 += coverage["bg2_visible_pixels"]
                composite_unresolved += coverage["unresolved_pixels"]
                composite_bitmap_bytes += len(bitmap)
                composite_mask_bytes += len(visibility)

        result = subprocess.run(
            [str(BINARY), "--mzm-backgrounds", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "background")
            encoded_size = int(fields[11])
            if fields[4] == "rle":
                self.assertGreaterEqual(encoded_size, 6)
            else:
                self.assertEqual(encoded_size, 0)
            actual.append((
                fields[1], int(fields[2]), int(fields[3]), fields[4],
                *(int(value, 0) for value in fields[5:11]), fields[12],
            ))
        self.assertEqual(actual, expected)
        self.assertEqual(
            lines[-1],
            "summary\t{}\t{}\t{}\t{}\t{}\t{}\t{}".format(
                len(source_rooms()), len(expected), counts["rle"],
                counts["text"], counts["none"], counts["unsupported"],
                total_cells,
            ),
        )

        result = subprocess.run(
            [str(BINARY), "--mzm-background-renders", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual_renders = []
        for line in lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "render")
            actual_renders.append((
                fields[1], int(fields[2]), int(fields[3]), fields[4],
                *(int(value) for value in fields[5:9]), fields[9],
            ))
        self.assertEqual(actual_renders, expected_renders)
        self.assertEqual(
            lines[-1],
            f"summary\t{len(expected_renders)}\t{counts['rle']}\t"
            f"{counts['text']}\t{counts['none'] + counts['unsupported']}\t"
            f"{total_rendered_pixels}\t0",
        )

        result = subprocess.run(
            [str(BINARY), "--mzm-background-composites", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(lines[:-1], expected_composites)
        self.assertEqual(
            lines[-1],
            f"summary\t{selection_count}\t{missing_bg1}\t{bg12_count}\t"
            f"{bg1_only_count}\t{mismatched_composites}\t{composite_pixels}\t"
            f"{composite_bg1}\t{composite_bg2}\t{composite_unresolved}\t"
            f"{composite_bitmap_bytes}\t{composite_mask_bytes}",
        )

    def test_tileset_video_memory_matches_python_reference(self):
        symbol_rows, _ = symbol_lines(elf_symbols(ELF.read_bytes()),
                                      ROM.stat().st_size)
        symbols = {parts[1]: int(parts[2])
                   for row in symbol_rows
                   if (parts := row.split("\t"))[0] == "symbol"}
        expected = []
        total_graphics = 0
        total_background_graphics = 0
        for index in range(79):
            entry = tileset_entry(index)
            vram, palette = tileset_video_memory(index)
            graphics_size = len(lz77(symbol_bytes(entry["pTileGraphics"])))
            background_size = len(
                lz77(symbol_bytes(entry["pBackgroundGraphics"])))
            expected.append((
                index,
                symbols[entry["pTileGraphics"]],
                symbols[entry["pPalette"]],
                symbols[entry["pBackgroundGraphics"]],
                symbols[entry["pTilemap"]],
                int(entry["animatedTileset"]),
                int(entry["animatedPalette"]),
                len(symbol_bytes("sCommonTilesGfx")),
                graphics_size,
                background_size,
                hashlib.sha1(vram).hexdigest(),
                hashlib.sha1(palette).hexdigest(),
            ))
            total_graphics += graphics_size
            total_background_graphics += background_size

        result = subprocess.run(
            [str(BINARY), "--mzm-tilesets", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "tileset")
            actual.append(tuple(int(value) for value in fields[1:11]) +
                          (fields[11], fields[12]))
        self.assertEqual(actual, expected)
        self.assertEqual(
            lines[-1],
            f"summary\t{len(expected)}\t{total_graphics}\t"
            f"{total_background_graphics}",
        )

    def test_tileset_metatiles_match_python_reference(self):
        symbol_rows, _ = symbol_lines(elf_symbols(ELF.read_bytes()),
                                      ROM.stat().st_size)
        symbols = {parts[1]: int(parts[2])
                   for row in symbol_rows
                   if (parts := row.split("\t"))[0] == "symbol"}
        expected = []
        total_tileset_words = 0
        for index in range(79):
            entry = tileset_entry(index)
            blocks = tileset_metatiles(index)
            words = [word for block in blocks for word in block]
            canonical = struct.pack(f"<{len(words)}H", *words)
            source_words = min(
                (len(symbol_bytes(entry["pTilemap"])) - 2) // 2,
                1024 * 4,
            )
            expected.append((
                index,
                symbols[entry["pTilemap"]],
                source_words,
                208 * 4,
                1024 + 208,
                hashlib.sha1(canonical).hexdigest(),
            ))
            total_tileset_words += source_words

        result = subprocess.run(
            [str(BINARY), "--mzm-metatiles", str(ROM), str(ELF)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        actual = []
        for line in lines[:-1]:
            fields = line.split("\t")
            self.assertEqual(fields[0], "metatiles")
            actual.append(tuple(int(value) for value in fields[1:6]) +
                          (fields[6],))
        self.assertEqual(actual, expected)
        self.assertEqual(
            lines[-1],
            f"summary\t{len(expected)}\t{total_tileset_words}\t"
            f"{len(expected) * 208 * 4}\t"
            f"{len(expected) * (1024 + 208)}",
        )
