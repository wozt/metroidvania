#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode original Zero Mission room DESCRIPTORS from pinned decomp source.

This does not decode compressed room graphics, collision maps, or enemies.
It creates private metadata only, referenced by GTK and the asset catalog.
"""
from __future__ import annotations

import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
ROOM_SOURCE = ROOT / "third_party/mzm/src/data/rooms_data.c"
REQUIRED = ("tileset", "pBg1Data", "pBg2Data", "pClipData",
            "pDefaultSpriteData", "defaultSpriteset", "mapX", "mapY", "musicTrack")
AREA_RE = re.compile(r"const struct RoomEntryRom s([A-Za-z0-9]+)RoomEntries\[(\d+)\]\s*=\s*\{(.*?)\n\};", re.S)
ENTRY_RE = re.compile(r"\[(\d+)\]\s*=\s*\{([^{}]*)\}", re.S)
FIELD_RE = re.compile(r"\.([A-Za-z0-9]+)\s*=\s*([^,\n}]+)")
FIELD_TOKEN = re.compile(r"^[A-Za-z0-9_]+$")


def decode_room_descriptors(source: str) -> list[dict]:
    """Fail closed if upstream's table shape differs from audited source."""
    rooms = []
    areas = set()
    for area in AREA_RE.finditer(source):
        name, count_text, section = area.groups()
        if name in areas:
            raise ValueError(f"duplicate room area: {name}")
        areas.add(name)
        expected = int(count_text)
        entries = list(ENTRY_RE.finditer(section))
        if len(entries) != expected:
            raise ValueError(f"incomplete {name} room table: {len(entries)} != {expected}")
        for expected_index, match in enumerate(entries):
            index = int(match.group(1))
            if index != expected_index:
                raise ValueError(f"nonconsecutive {name} room index {index}")
            fields = dict((k, v.strip()) for k, v in FIELD_RE.findall(match.group(2)))
            if not all(k in fields for k in REQUIRED):
                raise ValueError(f"{name} {index}: required decomp fields absent")
            if not all(FIELD_TOKEN.fullmatch(value) for value in fields.values()):
                raise ValueError(f"{name} {index}: unsupported field expression")
            rooms.append({"id": f"mzm:{name.lower()}:{index:03}",
                          "area": name, "index": index,
                          "source": "third_party/mzm/src/data/rooms_data.c",
                          "fields": fields,
                          "status": "DECOMP_ROOM_DESCRIPTOR_ONLY"})
    if not areas:
        raise ValueError("no original Zero Mission RoomEntryRom tables found")
    return rooms


def emit_catalog(rooms: list[dict]) -> dict:
    from scripts.import_game_assets import write_generated
    output = {"format": "MV_MZM_ROOM_DESCRIPTORS_1",
              "source": "pinned metroidret/mzm, RoomEntryRom declarations",
              "warning": "Original room graphics, tilemaps, enemy instances and collision data remain UNDECODED",
              "count": len(rooms), "rooms": rooms}
    rows = ["# MZM native descriptor metadata; NOT rendered/decoded room maps",
            "# area|index|tileset|music|bg1|bg2|clip|spriteset|mapX|mapY"]
    for room in rooms:
        f = room["fields"]
        rows.append("|".join((room["area"], str(room["index"]),
                              f["tileset"], f["musicTrack"], f["pBg1Data"],
                              f["pBg2Data"], f["pClipData"],
                              f["defaultSpriteset"], f["mapX"], f["mapY"])))
    write_generated("rooms/metroid/catalog.json", (json.dumps(output, indent=2) + "\n").encode())
    write_generated("rooms/metroid/rooms.tsv", ("\n".join(rows) + "\n").encode())
    return output


def run() -> dict:
    if not ROOM_SOURCE.is_file():
        raise ValueError("missing pinned Zero Mission source; run git submodule update --init")
    room_data = ROOM_SOURCE.read_text(encoding="utf-8")
    return emit_catalog(decode_room_descriptors(room_data))


if __name__ == "__main__":
    try:
        catalog = run()
    except (ValueError, OSError) as exc:
        raise SystemExit(f"MZM room import failed: {exc}") from exc
    print(f"Zero Mission original room descriptors indexed: {catalog['count']} (metadata only)")
