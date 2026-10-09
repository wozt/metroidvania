#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Shared non-graphical editing backend used by CLI and GTK subprocesses."""
from __future__ import annotations

import json
from pathlib import Path
import re
import tomllib
from typing import Any

from scripts import authored_rooms
from scripts.import_game_assets import ROOT, verified_rom
from scripts.import_mzm_rooms import ROOM_SOURCE, decode_room_descriptors
from scripts.room_audit import audit_world, write_private_report
from scripts.validate_story_assets import validate_scene, validate_timeline

BACKEND_VERSION = "1.0.0"

CAPABILITIES = {
    "project-info": "available",
    "project-validate": "available",
    "list-worlds": "available",
    "list-areas": "available",
    "list-rooms": "available",
    "room-list": "available",
    "room-inspect": "available",
    "room-create": "available_draft_only",
    "room-validate": "available_draft_only",
    "room-render": "available_partial_native_render",
    "room-audit": "available_partial_native_render",
    "tile-set": "unavailable_backend_extraction_pending",
    "collision-set": "unavailable_schema_pending",
    "door-create": "unavailable_schema_pending",
    "object-definition-create": "unavailable_schema_pending",
    "event-create": "unavailable_backend_extraction_pending",
    "cutscene-create": "unavailable_backend_extraction_pending",
    "audio-list": "unavailable_native_inventory_pending",
    "audio-render": "unavailable_decoder_pending",
    "native-gameplay": "unavailable_engines_not_implemented",
}

COMMAND_FIELDS = {
    "capabilities": set(),
    "project-info": set(),
    "project-validate": set(),
    "list-worlds": set(),
    "list-areas": {"world"},
    "list-rooms": {"world", "area"},
    "room-list": {"world", "area", "source"},
    "room-inspect": {"world", "area", "room", "source"},
    "room-create": {"world", "area", "slug", "name", "width_screens",
                    "height_screens"},
    "room-validate": {"room"},
    "room-render": {"world", "area", "room"},
    "room-audit": {"world", "area", "workers", "report"},
}
MUTATING_COMMANDS = {"room-create"}


def _integer(value: Any, label: str, lower: int = 0, upper: int = 1_000_000) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{label} must be an integer")
    try:
        parsed = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{label} must be an integer") from exc
    if str(parsed) != str(value) and not isinstance(value, int):
        raise ValueError(f"{label} must be a canonical integer")
    if not lower <= parsed <= upper:
        raise ValueError(f"{label} must be in [{lower}, {upper}]")
    return parsed


def _world(value: Any) -> str:
    if value not in authored_rooms.AREAS:
        raise ValueError("world must be zero_mission or aria")
    return str(value)


def _validated_root(value: Path | str) -> Path:
    root = Path(value)
    if ".." in root.parts:
        raise ValueError("project root path traversal refused")
    absolute = root if root.is_absolute() else Path.cwd() / root
    current = Path(absolute.anchor)
    for part in absolute.parts[1:]:
        current /= part
        if current.is_symlink():
            raise ValueError(f"symlink project root component refused: {current}")
    if not absolute.is_dir():
        raise ValueError("project root must be an existing non-symlink directory")
    return absolute.resolve()


def _native_catalog(world: str) -> list[dict]:
    if world == "zero_mission":
        rooms = decode_room_descriptors(ROOM_SOURCE.read_text(encoding="utf-8"))
        return [{"world": world, "area": room["area"], "area_index": None,
                 "room": room["index"], "native_id": room["id"],
                 "native_reference": room["source"], "fields": room["fields"]}
                for room in rooms]
    from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1, decode_world
    catalog = decode_world(verified_rom(DEFAULT_ROM, EXPECTED_SHA1))
    return [{"world": world, "area": room["area"],
             "area_index": room["engine_area"], "room": room["room"],
             "native_id": f"aria:{room['engine_area']:02d}:{room['room']:03d}",
             "native_reference": room["source_pointer"],
             "backgrounds": room["backgrounds"],
             "entity_count": len(room["entities"]),
             "transition_count": len(room["transitions"])}
            for room in catalog["rooms"]]


def _filter_native_rooms(world: str, area: Any | None) -> list[dict]:
    rooms = _native_catalog(world)
    if area is None:
        return rooms
    if world == "aria":
        index = _integer(area, "area", 0, len(authored_rooms.AREAS[world]) - 1)
        return [room for room in rooms if room["area_index"] == index]
    if str(area).isdecimal():
        index = _integer(area, "area", 0, len(authored_rooms.AREAS[world]) - 1)
        name = authored_rooms.AREAS[world][index]
    else:
        name = next((candidate for candidate in authored_rooms.AREAS[world]
                     if candidate.lower() == str(area).lower()), None)
        if name is None:
            raise ValueError("unknown Zero Mission area")
    return [room for room in rooms if room["area"] == name]


def _draft_by_id(root: Path, identity: Any) -> dict:
    if not isinstance(identity, str) or not identity:
        raise ValueError("room draft identity is required")
    room = next((item for item in authored_rooms.list_rooms(root)
                 if item["id"] == identity), None)
    if room is None:
        raise ValueError("unknown project room draft")
    return room


def _project_validation(root: Path) -> dict:
    timeline_path = root / "data/story/timeline.toml"
    timeline = tomllib.loads(timeline_path.read_text(encoding="utf-8"))
    timeline_count = validate_timeline(timeline)
    scenes = []
    for path in sorted((root / "data/cutscenes").glob("*.toml")):
        if path.is_symlink():
            raise ValueError(f"symlink cutscene refused: {path}")
        count = validate_scene(tomllib.loads(path.read_text(encoding="utf-8")))
        scenes.append({"path": str(path.relative_to(root)), "step_count": count})
    drafts = authored_rooms.list_rooms(root)
    return {"timeline_events": timeline_count, "cutscenes": scenes,
            "draft_rooms": [room["id"] for room in drafts],
            "engine_adapters": {"zero_mission": "unavailable",
                                "aria": "unavailable"}}


def execute(command: str, options: dict[str, Any], *, root: Path | str = ROOT,
            dry_run: bool = False) -> dict:
    """Execute one validated backend operation without formatting its result."""
    if command not in COMMAND_FIELDS:
        capability = CAPABILITIES.get(command)
        if capability:
            raise NotImplementedError(capability)
        raise ValueError(f"unknown command: {command}")
    unknown = set(options) - COMMAND_FIELDS[command]
    if unknown:
        raise ValueError(f"unknown option(s) for {command}: {', '.join(sorted(unknown))}")
    root_path = _validated_root(root)
    if command == "capabilities":
        return {"backend_version": BACKEND_VERSION, "commands": CAPABILITIES}
    if command == "project-info":
        return {"name": "Metroid Vania", "backend_version": BACKEND_VERSION,
                "root": str(root_path), "gameplay_status": "not_implemented",
                "worlds": list(authored_rooms.AREAS)}
    if command == "project-validate":
        return _project_validation(root_path)
    if command == "list-worlds":
        return {"worlds": [{"id": world, "areas": len(areas),
                            "engine_adapter": "unavailable"}
                           for world, areas in authored_rooms.AREAS.items()]}
    if command == "list-areas":
        world = _world(options.get("world"))
        return {"world": world, "areas": [
            {"index": index, "name": name}
            for index, name in enumerate(authored_rooms.AREAS[world])]}
    if command in ("list-rooms", "room-list"):
        world = _world(options.get("world"))
        source = options.get("source", "native")
        if source == "native":
            rooms = _filter_native_rooms(world, options.get("area"))
        elif source == "draft":
            rooms = authored_rooms.list_rooms(root_path, world)
        else:
            raise ValueError("source must be native or draft")
        return {"world": world, "source": source, "count": len(rooms), "rooms": rooms}
    if command == "room-inspect":
        source = options.get("source", "native")
        if source == "draft":
            return {"source": source, "room": _draft_by_id(root_path, options.get("room"))}
        world = _world(options.get("world"))
        room_id = _integer(options.get("room"), "room", 0, 999)
        rooms = _filter_native_rooms(world, options.get("area"))
        matches = [room for room in rooms if room["room"] == room_id]
        if len(matches) != 1:
            raise ValueError("native room identity is missing or ambiguous")
        return {"source": source, "room": matches[0]}
    if command == "room-create":
        world = _world(options.get("world"))
        room = authored_rooms.new_room(
            world=world,
            area=_integer(options.get("area"), "area", 0,
                          len(authored_rooms.AREAS[world]) - 1),
            slug=options.get("slug"), name=options.get("name"),
            width_screens=_integer(options.get("width_screens", 1),
                                   "width_screens", 1, 8),
            height_screens=_integer(options.get("height_screens", 1),
                                    "height_screens", 1, 8))
        if dry_run:
            return {"room": room, "persisted": False}
        path = authored_rooms.save_room(room, root_path)
        return {"room": room, "persisted": True, "path": str(path)}
    if command == "room-validate":
        room = _draft_by_id(root_path, options.get("room"))
        return {"room": room["id"], "status": "valid_draft_not_playable"}
    if command == "room-render":
        world = _world(options.get("world"))
        room_id = _integer(options.get("room"), "room", 0, 999)
        if world == "zero_mission":
            rooms = _filter_native_rooms(world, options.get("area"))
            matches = [room for room in rooms if room["room"] == room_id]
            if len(matches) != 1:
                raise ValueError("Zero Mission room identity is missing or ambiguous")
            from scripts.mzm_room_render import decode_room
            result = decode_room(matches[0]["area"], room_id,
                                 write_outputs=not dry_run)
        else:
            area = _integer(options.get("area"), "area", 0, 11)
            from scripts.aos_room_render import render_room
            from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1
            result = render_room(verified_rom(DEFAULT_ROM, EXPECTED_SHA1), area,
                                 room_id, write_outputs=not dry_run)
        return {"render": result, "persisted": not dry_run}
    if command == "room-audit":
        world = _world(options.get("world"))
        workers = _integer(options.get("workers", 1), "workers", 1, 8)
        report = audit_world(world, area=options.get("area"), workers=workers)
        report_name = options.get("report")
        if report_name is not None:
            if not isinstance(report_name, str) or not re.fullmatch(
                    r"[a-z0-9][a-z0-9_-]{0,63}", report_name):
                raise ValueError("report must be a lowercase safe identifier")
            if not dry_run:
                path = write_private_report(report, f"{report_name}.json")
                report["report_path"] = str(path)
        return report
    raise AssertionError(f"unhandled backend command: {command}")
