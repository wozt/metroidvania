#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode original Zero Mission room DESCRIPTORS from pinned decomp source.

This does not decode compressed room graphics, collision maps, or enemies.
The editor tools read the descriptors; the private rooms/metroid/rooms.tsv is
written by the native importer (``fusion_importer --raw-import``).
"""
from __future__ import annotations

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
