#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Write an Aria room browser index from verified, private decoded metadata.

This file lists only structural facts (no ROM payload). Outputs are always
private and ignored, below assets/extracted/rooms/aria/.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from scripts.import_game_assets import OUTPUT, write_generated

INDEX_PATH = "rooms/aria/rooms.tsv"


def _text(value: object) -> str:
    if not isinstance(value, str) or not value or any(ch in value for ch in "\r\n\t|"):
        raise ValueError("invalid Aria area label")
    return value


def index_rows(catalog: dict) -> list[str]:
    if catalog.get("format") != "MV_AOS_WORLD_2":
        raise ValueError("expected decoded Aria native-world catalog")
    rooms = catalog.get("rooms")
    if not isinstance(rooms, list) or not rooms:
        raise ValueError("Aria catalog has no native rooms")
    saves = {(int(s["engine_area"]), int(s["room"]))
             for s in catalog.get("savepoints", [])}
    bosses = {(int(s["engine_area"]), int(s["room"]))
              for s in catalog.get("boss_rooms", [])}
    seen: set[tuple[int, int]] = set()
    rows = ["# VERIFIED ARIA STRUCTURAL INDEX; NOT a decoded gameplay map",
            "# area|room|name|width_screens|height_screens|entities|transitions|save|boss"]
    for room in sorted(rooms, key=lambda item: (item["engine_area"], item["room"])):
        area, number = int(room["engine_area"]), int(room["room"])
        if not (0 <= area < 12 and 0 <= number < 1000) or (area, number) in seen:
            raise ValueError("invalid or duplicate Aria room identity")
        seen.add((area, number))
        backgrounds = room.get("backgrounds", [])
        if not isinstance(backgrounds, list) or len(backgrounds) != 3:
            raise ValueError("native room requires exactly three backgrounds")
        primary = backgrounds[0]
        width = primary.get("width_screens")
        height = primary.get("height_screens")
        for value in (width, height):
            if value is not None and (not isinstance(value, int) or not 1 <= value <= 16):
                raise ValueError("invalid Aria room dimensions")
        entities = room.get("entities", [])
        transitions = room.get("transitions", [])
        if not isinstance(entities, list) or not isinstance(transitions, list):
            raise ValueError("invalid Aria room entity or transition list")
        rows.append("|".join((str(area), str(number), _text(room["area"]),
                              str(width or 0), str(height or 0),
                              str(len(entities)), str(len(transitions)),
                              str(int((area, number) in saves)),
                              str(int((area, number) in bosses)))))
    if catalog.get("native_room_count") != len(seen):
        raise ValueError("Aria room count mismatch")
    return rows


def write_index(catalog: dict) -> Path:
    write_generated(INDEX_PATH, ("\n".join(index_rows(catalog)) + "\n").encode("utf-8"))
    return OUTPUT / INDEX_PATH


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--world", type=Path, default=OUTPUT / "rooms/aria/world.json")
    args = parser.parse_args()
    if args.world.is_symlink() or not args.world.is_file():
        parser.error("private Aria catalog unavailable; run scripts/import_aos_world.py")
    try:
        catalog = json.loads(args.world.read_text(encoding="utf-8"))
        result = write_index(catalog)
    except (OSError, ValueError, TypeError, KeyError) as exc:
        parser.error(str(exc))
    print(f"Indexed {catalog['native_room_count']} Aria rooms -> {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
