#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode Aria's room directory and save/warp flags from a verified local ROM.

Only structural metadata is emitted. No graphics, audio, map tiles, or other
ROM payload is copied into the repository.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

DEFAULT_ROM = ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba"
EXPECTED_SHA1 = "abd71fe01ebb201bcc133074db1dd8c5253776c7"
GBA_ROM_BASE = 0x08000000
MAP_TABLE_OFFSET = 0x116650
MAP_WIDTH = 64
MAP_HEIGHT = 35
AREA_DIRECTORY_OFFSET = 0x50EF08
AREA_COUNT = 12
SAVE_FLAG = 0x8000
WARP_FLAG = 0x4000

# These names pair the debug menu's engine regions with their published names.
# Engine area 7 contains all three connected underground map regions.
AREA_NAMES = (
    "Castle Corridor",
    "Chapel",
    "Study",
    "Dance Hall",
    "Inner Quarters",
    "Floating Garden",
    "Clock Tower",
    "Underground region",
    "The Arena",
    "Top Floor",
    "Chaotic Realm entrance",
    "Chaotic Realm boss",
)


def _u32(rom: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(rom):
        raise ValueError(f"32-bit read outside ROM at 0x{offset:x}")
    return struct.unpack_from("<I", rom, offset)[0]


def _rom_offset(pointer: int, rom_size: int) -> int:
    offset = pointer - GBA_ROM_BASE
    if pointer < GBA_ROM_BASE or offset < 0 or offset >= rom_size:
        raise ValueError(f"invalid GBA ROM pointer 0x{pointer:08x}")
    return offset


def decode_world(rom: bytes, *, verify_hash: bool = True) -> dict:
    """Return bounded room-directory and global-map metadata."""
    if verify_hash and hashlib.sha1(rom, usedforsecurity=False).hexdigest() != EXPECTED_SHA1:
        raise ValueError("expected unmodified Aria of Sorrow USA ROM")
    map_end = MAP_TABLE_OFFSET + MAP_WIDTH * MAP_HEIGHT * 2
    directory_end = AREA_DIRECTORY_OFFSET + AREA_COUNT * 4
    if len(rom) < max(map_end, directory_end):
        raise ValueError("ROM is too short for the audited Aria tables")

    area_pointers = [
        _u32(rom, AREA_DIRECTORY_OFFSET + index * 4)
        for index in range(AREA_COUNT)
    ]
    table_end = GBA_ROM_BASE + AREA_DIRECTORY_OFFSET
    table_bounds = list(zip(area_pointers, area_pointers[1:] + [table_end]))
    room_counts = []
    for area, (start, end) in enumerate(table_bounds):
        _rom_offset(start, len(rom))
        if end <= start or end > table_end or (end - start) % 4:
            raise ValueError(f"invalid room-pointer table bounds for area {area}")
        room_counts.append((end - start) // 4)

    rooms = []
    savepoints = []
    warps = []
    seen_locations = set()
    for map_y in range(MAP_HEIGHT):
        for map_x in range(MAP_WIDTH):
            offset = MAP_TABLE_OFFSET + 2 * (map_y * MAP_WIDTH + map_x)
            value = struct.unpack_from("<H", rom, offset)[0]
            if value == 0xFFFF:
                continue
            area = (value >> 6) & 0xF
            room = value & 0x3F
            if area >= AREA_COUNT or room >= room_counts[area]:
                raise ValueError(
                    f"map cell ({map_x},{map_y}) references invalid room {area}:{room}"
                )
            room_pointer = _u32(
                rom, _rom_offset(area_pointers[area], len(rom)) + room * 4
            )
            _rom_offset(room_pointer, len(rom))
            record = {
                "engine_area": area,
                "area": AREA_NAMES[area],
                "room": room,
                "map_x": map_x,
                "map_y": map_y,
                "room_pointer": f"0x{room_pointer:08x}",
                "save": bool(value & SAVE_FLAG),
                "warp": bool(value & WARP_FLAG),
            }
            rooms.append(record)
            if record["save"]:
                location = (area, room)
                if location in seen_locations:
                    raise ValueError(f"save room {area}:{room} occupies multiple map cells")
                seen_locations.add(location)
                savepoints.append({
                    "id": f"aria.save.{area}.{room}",
                    **record,
                })
            if record["warp"]:
                warps.append(record)

    return {
        "format": "MV_AOS_WORLD_1",
        "source_revision": f"Aria of Sorrow USA SHA-1 {EXPECTED_SHA1}",
        "map_table": f"0x{GBA_ROM_BASE + MAP_TABLE_OFFSET:08x}",
        "area_directory": f"0x{GBA_ROM_BASE + AREA_DIRECTORY_OFFSET:08x}",
        "room_counts": room_counts,
        "mapped_cells": len(rooms),
        "savepoint_count": len(savepoints),
        "warp_count": len(warps),
        "savepoints": savepoints,
        "warps": warps,
    }


def write_catalog(catalog: dict) -> Path:
    from scripts.import_game_assets import write_generated

    relative = "rooms/aria/world.json"
    write_generated(relative, (json.dumps(catalog, indent=2) + "\n").encode())
    return ROOT / "assets/extracted" / relative


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    parser.add_argument(
        "--stdout", action="store_true",
        help="print metadata JSON instead of writing the ignored private catalog",
    )
    args = parser.parse_args()
    try:
        catalog = decode_world(args.rom.read_bytes())
        if args.stdout:
            print(json.dumps(catalog, indent=2))
        else:
            output = write_catalog(catalog)
            print(
                f"Aria world metadata indexed: {catalog['savepoint_count']} save rooms, "
                f"{catalog['warp_count']} warp rooms -> {output}"
            )
    except (OSError, ValueError) as exc:
        raise SystemExit(f"Aria world import failed: {exc}") from exc
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
