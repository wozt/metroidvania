#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode Aria's native room structures from a verified local ROM.

Only structural metadata and ROM references are emitted. No graphics, audio,
map tiles, or other ROM payload is copied into the repository.
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
ROOM_DESCRIPTOR_SIZE = 0x24
BACKGROUND_DESCRIPTOR_SIZE = 0xC
BACKGROUND_COUNT = 3
LOAD_ENTRY_SIZE = 0x8
ENTITY_ENTRY_SIZE = 0xC
TRANSITION_ENTRY_SIZE = 0x10
ENEMY_TABLE_POINTER = 0x080E9644
ENEMY_TABLE_ENTRY_SIZE = 0x24
MAX_VARIANTS = 16
MAX_LOAD_ENTRIES = 64
MAX_ENTITY_ENTRIES = 512

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

# These identities combine the room directory, entity placements, the enemy
# function table, and named cvaos update/create symbols. Repeated ordinary
# enemies elsewhere in the castle are intentionally not marked as bosses.
BOSS_ROOM_IDENTITIES = {
    (0, 10): ("aria.creaking_skull", "Creaking Skull", 0x21, "fight"),
    (1, 6): ("aria.manticore", "Manticore", 0x36, "fight"),
    (2, 15): ("aria.great_armor", "Great Armor", 0x3C, "fight"),
    (3, 19): ("aria.big_golem", "Big Golem", 0x45, "fight"),
    (4, 11): ("aria.headhunter", "Headhunter", 0x6A, "fight"),
    (5, 6): ("aria.julius", "Julius Belmont", 0x6E, "fight"),
    (6, 7): ("aria.death", "Death", 0x6B, "fight"),
    (7, 37): ("aria.legion", "Legion", 0x6C, "fight"),
    (8, 1): ("aria.balore", "Balore", 0x6D, "fight"),
    (9, 4): ("aria.graham", "Graham Jones", 0x6F, "fight"),
    (11, 20): ("aria.chaos", "Chaos", 0x70, "first_phase"),
    (11, 22): ("aria.chaos", "Chaos", 0x70, "second_phase"),
}


def _u32(rom: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(rom):
        raise ValueError(f"32-bit read outside ROM at 0x{offset:x}")
    return struct.unpack_from("<I", rom, offset)[0]


def _rom_offset(pointer: int, rom_size: int, size: int = 1) -> int:
    offset = pointer - GBA_ROM_BASE
    if size < 0 or pointer < GBA_ROM_BASE or offset < 0 or offset + size > rom_size:
        raise ValueError(f"invalid GBA ROM pointer 0x{pointer:08x}")
    return offset


def _pointer(pointer: int) -> str:
    return f"0x{pointer:08x}"


def _decode_load_list(rom: bytes, pointer: int) -> list[dict]:
    """Decode a zero-terminated eight-byte resource loading list."""
    start = _rom_offset(pointer, len(rom), LOAD_ENTRY_SIZE)
    entries = []
    for index in range(MAX_LOAD_ENTRIES):
        offset = start + index * LOAD_ENTRY_SIZE
        if offset + LOAD_ENTRY_SIZE > len(rom):
            raise ValueError(f"unterminated resource list at {_pointer(pointer)}")
        resource, parameter_0, parameter_1, parameter_2 = struct.unpack_from(
            "<IBBB", rom, offset
        )
        if resource == 0:
            return entries
        _rom_offset(resource, len(rom))
        entries.append({
            "entry_pointer": _pointer(GBA_ROM_BASE + offset),
            "resource_pointer": _pointer(resource),
            "parameters": [parameter_0, parameter_1, parameter_2],
        })
    raise ValueError(f"resource list exceeds safety cap at {_pointer(pointer)}")


def _decode_backgrounds(rom: bytes, pointer: int) -> list[dict]:
    start = _rom_offset(
        pointer, len(rom), BACKGROUND_COUNT * BACKGROUND_DESCRIPTOR_SIZE
    )
    backgrounds = []
    for index in range(BACKGROUND_COUNT):
        offset = start + index * BACKGROUND_DESCRIPTOR_SIZE
        field_0, field_1, control, field_4, field_6, metadata = struct.unpack_from(
            "<BBHHHI", rom, offset
        )
        if metadata:
            metadata_offset = _rom_offset(metadata, len(rom), 2)
            width_screens, height_screens = struct.unpack_from(
                "<BB", rom, metadata_offset
            )
        else:
            width_screens = height_screens = None
        backgrounds.append({
            "layer": index + 1,
            "descriptor_pointer": _pointer(GBA_ROM_BASE + offset),
            "field_0": field_0,
            "field_1": field_1,
            "control": control,
            "field_4": field_4,
            "field_6": field_6,
            "metadata_pointer": _pointer(metadata) if metadata else None,
            "width_screens": width_screens,
            "height_screens": height_screens,
        })
    return backgrounds


def _decode_entities(rom: bytes, pointer: int) -> list[dict]:
    start = _rom_offset(pointer, len(rom), ENTITY_ENTRY_SIZE)
    entities = []
    for index in range(MAX_ENTITY_ENTRIES):
        offset = start + index * ENTITY_ENTRY_SIZE
        if offset + ENTITY_ENTRY_SIZE > len(rom):
            raise ValueError(f"unterminated entity list at {_pointer(pointer)}")
        x, y, persistent_index, kind, entity_id, flags, parameter_0, parameter_1 = (
            struct.unpack_from("<hhBBBBHH", rom, offset)
        )
        if x >= 0x7FFF:
            return entities
        entities.append({
            "entry_pointer": _pointer(GBA_ROM_BASE + offset),
            "x": x,
            "y": y,
            "persistent_index": persistent_index,
            "kind": kind,
            "entity_id": entity_id,
            "flags": flags,
            "parameters": [parameter_0, parameter_1],
        })
    raise ValueError(f"entity list exceeds safety cap at {_pointer(pointer)}")


def _decode_room(rom: bytes, source_pointer: int) -> dict:
    """Resolve and decode one room descriptor with bounded linked structures."""
    pointer = source_pointer
    variants = []
    seen = set()
    for _ in range(MAX_VARIANTS):
        if pointer in seen:
            raise ValueError(f"room variant cycle at {_pointer(pointer)}")
        seen.add(pointer)
        offset = _rom_offset(pointer, len(rom), ROOM_DESCRIPTOR_SIZE)
        selector = struct.unpack_from("<H", rom, offset + 2)[0]
        variants.append({"pointer": _pointer(pointer), "selector": selector})
        if selector == 0xFFFF:
            break
        pointer = _u32(rom, offset + 4)
    else:
        raise ValueError(f"room variant chain exceeds safety cap at {_pointer(source_pointer)}")

    offset = _rom_offset(pointer, len(rom), ROOM_DESCRIPTOR_SIZE)
    (
        display_control, selector, next_pointer, backgrounds_pointer,
        graphics_pointer, palette_pointer, entities_pointer, transitions_pointer,
        blend_control, coordinates,
    ) = struct.unpack_from("<HHIIIIII2xH2xH", rom, offset)
    if selector != 0xFFFF:
        raise ValueError(f"unresolved room descriptor at {_pointer(pointer)}")
    if transitions_pointer > pointer or (pointer - transitions_pointer) % TRANSITION_ENTRY_SIZE:
        raise ValueError(f"invalid transition table at {_pointer(transitions_pointer)}")
    transition_count = (pointer - transitions_pointer) // TRANSITION_ENTRY_SIZE
    transition_offset = _rom_offset(
        transitions_pointer, len(rom), transition_count * TRANSITION_ENTRY_SIZE
    )
    transitions = []
    for index in range(transition_count):
        entry_offset = transition_offset + index * TRANSITION_ENTRY_SIZE
        (
            target, source_screen_x, source_screen_y, field_6, field_8,
            load_x, load_y, field_e,
        ) = struct.unpack_from("<IbbHHHHH", rom, entry_offset)
        _rom_offset(target, len(rom), ROOM_DESCRIPTOR_SIZE)
        transitions.append({
            "entry_pointer": _pointer(GBA_ROM_BASE + entry_offset),
            "target_pointer": _pointer(target),
            "source_screen_x": source_screen_x,
            "source_screen_y": source_screen_y,
            "field_6": field_6,
            "field_8": field_8,
            "load_x": load_x,
            "load_y": load_y,
            "field_e": field_e,
        })

    entities = [] if entities_pointer == 0 else _decode_entities(rom, entities_pointer)
    return {
        "source_pointer": _pointer(source_pointer),
        "resolved_pointer": _pointer(pointer),
        "variant_chain": variants,
        "display_control": display_control,
        "blend_control": blend_control,
        "map_x": coordinates & 0x7F,
        "map_y": (coordinates >> 7) & 0x7F,
        "flags": [(coordinates >> 14) & 1, (coordinates >> 15) & 1],
        "backgrounds": _decode_backgrounds(rom, backgrounds_pointer),
        "graphics_loads": _decode_load_list(rom, graphics_pointer),
        "palette_loads": _decode_load_list(rom, palette_pointer),
        "entity_list_pointer": _pointer(entities_pointer) if entities_pointer else None,
        "entities": entities,
        "transition_list_pointer": _pointer(transitions_pointer),
        "transitions": transitions,
        "next_pointer": _pointer(next_pointer) if next_pointer else None,
    }


def decode_world(rom: bytes, *, verify_hash: bool = True) -> dict:
    """Return bounded native-room and global-map metadata."""
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

    native_rooms = []
    room_by_location = {}
    location_by_pointer = {}
    for area, (table_pointer, room_count) in enumerate(zip(area_pointers, room_counts)):
        table_offset = _rom_offset(table_pointer, len(rom), room_count * 4)
        for room in range(room_count):
            room_pointer = _u32(rom, table_offset + room * 4)
            if room_pointer in location_by_pointer:
                previous = location_by_pointer[room_pointer]
                raise ValueError(
                    f"room pointer {_pointer(room_pointer)} is shared by "
                    f"{previous[0]}:{previous[1]} and {area}:{room}"
                )
            decoded = {
                "engine_area": area,
                "area": AREA_NAMES[area],
                "room": room,
                **_decode_room(rom, room_pointer),
                "map_cells": [],
            }
            native_rooms.append(decoded)
            room_by_location[(area, room)] = decoded
            location_by_pointer[room_pointer] = (area, room)

    map_cells = []
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
            map_cells.append(record)
            room_by_location[(area, room)]["map_cells"].append({
                "map_x": map_x,
                "map_y": map_y,
                "save": record["save"],
                "warp": record["warp"],
            })
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

    transition_count = 0
    for room in native_rooms:
        for transition in room["transitions"]:
            target_pointer = int(transition["target_pointer"], 16)
            try:
                target_area, target_room = location_by_pointer[target_pointer]
            except KeyError as exc:
                raise ValueError(
                    f"transition targets unknown room {_pointer(target_pointer)}"
                ) from exc
            transition["target_engine_area"] = target_area
            transition["target_area"] = AREA_NAMES[target_area]
            transition["target_room"] = target_room
            transition_count += 1

    boss_rooms = []
    for location, (boss_id, name, enemy_id, phase) in BOSS_ROOM_IDENTITIES.items():
        room = room_by_location.get(location)
        if room is None:
            if verify_hash:
                raise ValueError(f"missing audited boss room {location[0]}:{location[1]}")
            continue
        matches = [
            entity for entity in room["entities"]
            if entity["kind"] == 1 and entity["entity_id"] == enemy_id
        ]
        if not matches:
            if verify_hash:
                raise ValueError(f"missing {name} entity in room {location[0]}:{location[1]}")
            continue
        if len(matches) != 1:
            raise ValueError(f"ambiguous {name} entity in room {location[0]}:{location[1]}")
        entity = matches[0]
        enemy_table_pointer = ENEMY_TABLE_POINTER + enemy_id * ENEMY_TABLE_ENTRY_SIZE
        enemy_offset = _rom_offset(
            enemy_table_pointer, len(rom), ENEMY_TABLE_ENTRY_SIZE
        )
        health = struct.unpack_from("<H", rom, enemy_offset + 0xC)[0]
        entity["boss_id"] = boss_id
        entity["boss_phase"] = phase
        boss_rooms.append({
            "id": boss_id,
            "name": name,
            "phase": phase,
            "engine_area": location[0],
            "area": AREA_NAMES[location[0]],
            "room": location[1],
            "map_x": room["map_x"],
            "map_y": room["map_y"],
            "room_pointer": room["source_pointer"],
            "entity_pointer": entity["entry_pointer"],
            "entity_id": enemy_id,
            "enemy_table_pointer": _pointer(enemy_table_pointer),
            "health": health,
        })

    return {
        "format": "MV_AOS_WORLD_2",
        "source_revision": f"Aria of Sorrow USA SHA-1 {EXPECTED_SHA1}",
        "map_table": f"0x{GBA_ROM_BASE + MAP_TABLE_OFFSET:08x}",
        "area_directory": f"0x{GBA_ROM_BASE + AREA_DIRECTORY_OFFSET:08x}",
        "room_counts": room_counts,
        "native_room_count": len(native_rooms),
        "mapped_cells": len(map_cells),
        "transition_count": transition_count,
        "entity_count": sum(len(room["entities"]) for room in native_rooms),
        "boss_room_count": len(boss_rooms),
        "savepoint_count": len(savepoints),
        "warp_count": len(warps),
        "rooms": native_rooms,
        "boss_rooms": boss_rooms,
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
                f"Aria world metadata indexed: {catalog['native_room_count']} rooms, "
                f"{catalog['entity_count']} entities, "
                f"{catalog['savepoint_count']} save rooms -> {output}"
            )
    except (OSError, ValueError) as exc:
        raise SystemExit(f"Aria world import failed: {exc}") from exc
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
