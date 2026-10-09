#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Shared non-graphical editing backend used by CLI and GTK subprocesses."""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import tomllib
from typing import Any

from scripts import authored_rooms
from scripts import map_placements
from scripts import project_room_entities
from scripts import project_connection_graph
from scripts.import_game_assets import ROOT, verified_rom
from scripts.import_mzm_rooms import ROOM_SOURCE, decode_room_descriptors
from scripts.room_audit import audit_world, write_private_report
from scripts.validate_story_assets import validate_scene, validate_timeline

BACKEND_VERSION = "1.2.0"

CAPABILITIES = {
    "project-info": "available",
    "project-validate": "available",
    "project-audit": "available_structural_and_render",
    "list-worlds": "available",
    "list-areas": "available",
    "list-rooms": "available",
    "list-assets": "available_native_object_metadata",
    "room-list": "available",
    "room-inspect": "available",
    "room-open": "available_partial_native_workspace",
    "room-create": "available_draft_only",
    "room-validate": "available_draft_only",
    "room-render": "available_partial_native_render",
    "room-audit": "available_partial_native_render",
    "room-place": "available_draft_only",
    "room-move": "available_draft_only",
    "room-unplace": "available_draft_only",
    "placement-list": "available_draft_only",
    "entity-list": "available_project_markers_only",
    "entity-inspect": "available_project_markers_only",
    "entity-create": "available_project_markers_only",
    "entity-move": "available_project_markers_only",
    "entity-delete": "available_project_markers_only",
    "entity-assign": "available_project_markers_only",
    "entity-catalog": "available_native_reference_metadata",
    "entity-item-settings": "available_project_markers_only",
    "story-validate": "available_project_data_only",
    "story-save": "available_project_data_only",
    "layer-list": "available_private_native_workspace",
    "tile-get": "available_private_native_workspace",
    "tile-set": "available_private_native_workspace",
    "tile-fill": "available_private_native_workspace",
    "collision-list": "available_project_room_data_only",
    "collision-get": "available_project_room_data_only",
    "collision-set": "available_project_room_data_only",
    "collision-fill": "available_project_room_data_only",
    "collision-clear": "available_project_room_data_only",
    "collision-validate": "available_project_room_data_only",
    "door-list": "available_project_room_data_only",
    "door-target-list": "available_saved_project_door_targets_only",
    "connection-list": "available_saved_project_connections_only",
    "door-return-plan": "available_project_room_data_only",
    "door-inspect": "available_project_room_data_only",
    "door-create": "available_project_room_data_only",
    "door-adopt": "available_project_room_data_only",
    "door-update": "available_project_room_data_only",
    "door-delete": "available_project_room_data_only",
    "door-link": "available_project_room_data_only",
    "transition-list": "available_project_room_data_only",
    "transition-create": "available_project_room_data_only",
    "transition-update": "available_project_room_data_only",
    "transition-delete": "available_project_room_data_only",
    "transition-validate": "available_project_room_data_only",
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
    "project-audit": {"workers"},
    "list-worlds": set(),
    "list-areas": {"world"},
    "list-rooms": {"world", "area"},
    "list-assets": {"world"},
    "room-list": {"world", "area", "source"},
    "room-inspect": {"world", "area", "room", "source"},
    "room-open": {"world", "area", "room"},
    "room-create": {"world", "area", "slug", "name", "width_screens",
                    "height_screens"},
    "room-validate": {"room"},
    "room-render": {"world", "area", "room"},
    "room-audit": {"world", "area", "workers", "report"},
    "room-place": {"room", "x", "y"},
    "room-move": {"room", "x", "y"},
    "room-unplace": {"room", "confirm"},
    "placement-list": {"world"},
    "entity-list": {"world", "area", "room", "width", "height", "preview"},
    "entity-inspect": {"world", "area", "room", "width", "height", "id"},
    "entity-create": {"world", "area", "room", "width", "height", "kind",
                      "x", "y", "label", "native_type", "item_id",
                      "parameter_0", "parameter_1", "flags"},
    "entity-move": {"world", "area", "room", "width", "height", "id", "x", "y"},
    "entity-delete": {"world", "area", "room", "width", "height", "id", "confirm"},
    "entity-assign": {"world", "area", "room", "width", "height", "id",
                      "native_type", "item_id", "parameter_0", "parameter_1", "flags"},
    "entity-catalog": {"world", "kind"},
    "entity-item-settings": {"world", "area", "room", "width", "height", "id"},
    "story-validate": {"kind", "input"},
    "story-save": {"kind", "input", "target"},
    "layer-list": {"world", "area", "room"},
    "tile-get": {"world", "area", "room", "layer", "x", "y"},
    "tile-set": {"world", "area", "room", "layer", "x", "y", "tile_id"},
    "tile-fill": {"world", "area", "room", "layer", "x", "y", "tile_id"},
    "collision-list": {"world", "area", "room", "width", "height"},
    "collision-get": {"world", "area", "room", "width", "height", "x", "y"},
    "collision-set": {"world", "area", "room", "width", "height", "x", "y",
                      "collision_type"},
    "collision-fill": {"world", "area", "room", "width", "height", "x", "y",
                       "fill_width", "fill_height", "collision_type"},
    "collision-clear": {"world", "area", "room", "width", "height", "x", "y",
                        "fill_width", "fill_height", "confirm"},
    "collision-validate": {"world", "area", "room", "width", "height"},
    "door-list": {"world", "area", "room", "width", "height"},
    "door-target-list": {"target_world", "target_area", "target_room"},
    "connection-list": {"world", "area", "room"},
    "door-return-plan": {"world", "area", "room", "source_door_id"},
    "door-inspect": {"world", "area", "room", "width", "height", "id"},
    "door-create": {"world", "area", "room", "width", "height", "x", "y",
                    "door_width", "door_height", "label", "door_type", "facing"},
    "door-adopt": {"world", "area", "room", "width", "height",
                   "native_index", "native_variant", "native_type"},
    "door-update": {"world", "area", "room", "width", "height", "id", "x", "y",
                    "door_width", "door_height", "label", "door_type", "facing"},
    "door-delete": {"world", "area", "room", "width", "height", "id", "confirm"},
    "door-link": {"world", "area", "room", "width", "height",
                  "source_door_id", "target_world", "target_area",
                  "target_room", "target_door_id", "spawn_x", "spawn_y"},
    "transition-list": {"world", "area", "room", "width", "height"},
    "transition-create": {"world", "area", "room", "width", "height",
                          "source_door_id", "target_world", "target_area",
                          "target_room", "target_door_id", "spawn_x", "spawn_y"},
    "transition-update": {"world", "area", "room", "width", "height", "id",
                          "source_door_id", "target_world", "target_area",
                          "target_room", "target_door_id", "spawn_x", "spawn_y"},
    "transition-delete": {"world", "area", "room", "width", "height", "id", "confirm"},
    "transition-validate": {"world", "area", "room", "width", "height"},
}
MUTATING_COMMANDS = {
    "room-create", "room-open", "room-place", "room-move", "room-unplace",
    "entity-create", "entity-move", "entity-delete", "entity-assign", "story-save",
    "tile-set", "tile-fill",
    "collision-set", "collision-fill", "collision-clear",
    "door-create", "door-adopt", "door-update", "door-delete", "door-link",
    "transition-create", "transition-update", "transition-delete",
}


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


def _object_records(world: str | None = None) -> list[dict]:
    from scripts import object_catalog
    if world is not None:
        world = _world(world)
    records = []
    if world in (None, "zero_mission"):
        records.extend(object_catalog.build_mzm())
    if world in (None, "aria"):
        records.extend(object_catalog.build_aria(
            object_catalog.load_aria(), object_catalog.load_aria_enemy_names()))
    records.sort(key=lambda record: (
        record["world"], record["category"] == "Unused native type",
        record["category"], record["name"], record["native_id"]))
    return records


def _entity_scope(options: dict[str, Any]) -> tuple[str, str, int, int, int]:
    public_world = _world(options.get("world"))
    world = "mzm" if public_world == "zero_mission" else "aria"
    area_value = options.get("area")
    if world == "mzm":
        if area_value is None:
            raise ValueError("area is required")
        if str(area_value).isdecimal():
            index = _integer(area_value, "area", 0, 6)
            area = authored_rooms.AREAS["zero_mission"][index]
        else:
            area = next((name for name in authored_rooms.AREAS["zero_mission"]
                         if name.lower() == str(area_value).lower()), None)
            if area is None:
                raise ValueError("unknown Zero Mission area")
    else:
        area = str(_integer(area_value, "area", 0, 11))
    return (
        world, area,
        _integer(options.get("room"), "room", 0, 999),
        _integer(options.get("width"), "width", 16, 16384),
        _integer(options.get("height"), "height", 16, 16384),
    )


def _room_document(root: Path, options: dict[str, Any]) -> tuple[dict, tuple[str, str, int, int, int]]:
    scope = _entity_scope(options)
    return project_room_entities.load(root, *scope), scope


# PATCH_0114_VERIFIED_TARGET_DOORS: only persisted project door IDs are
# accepted as non-zero destination IDs. Original ROM doors have independent
# native indices and cannot be silently reinterpreted as project door IDs.
def _saved_target_doors(root: Path, target: dict[str, Any]) -> list[dict]:
    world = target["target_world"]
    area = target["target_area"]
    room = target["target_room"]
    # path_for's width/height only validate the scope; file names are identity based.
    path = project_room_entities.path_for(root, world, area, room, 16, 16)
    project_room_entities._check_path(path, root)
    if not path.exists():
        return []
    if not path.is_file() or path.stat().st_size > 4_000_000:
        raise ValueError("invalid saved destination room document")
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError("invalid saved destination room document") from exc
    if not isinstance(raw, dict):
        raise ValueError("invalid saved destination room document")
    raw = project_room_entities.migrate(raw)
    width, height = raw.get("width_px"), raw.get("height_px")
    if type(width) is not int or type(height) is not int:
        raise ValueError("invalid saved destination room geometry")
    validated = project_room_entities.validate(raw, world, area, room, width, height)
    return sorted(validated["doors"], key=lambda door: door["id"])


# PATCH_0115_RECIPROCAL_PLAN. A read-only plan, never a two-room write.
# Both source and destination doors and the forward transition must already be
# saved. The GTK UI subsequently creates the reverse transition only in the
# destination room's own staged document; its diskette remains mandatory.
def _saved_room_document_0115(root: Path, identity: dict[str, Any]) -> dict:
    world, area, room = (identity[key] for key in
                         ('target_world', 'target_area', 'target_room'))
    path = project_room_entities.path_for(root, world, area, room, 16, 16)
    project_room_entities._check_path(path, root)
    if not path.is_file() or path.stat().st_size > 4_000_000:
        raise ValueError("save both project doors before preparing the return link")
    try:
        raw = json.loads(path.read_text(encoding='utf-8'))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError("invalid saved room document") from exc
    if not isinstance(raw, dict):
        raise ValueError("invalid saved room document")
    raw = project_room_entities.migrate(raw)
    width, height = raw.get('width_px'), raw.get('height_px')
    if type(width) is not int or type(height) is not int:
        raise ValueError("invalid saved room geometry")
    return project_room_entities.validate(raw, world, area, room, width, height)


def _reciprocal_plan_0115(root: Path, options: dict[str, Any]) -> dict[str, Any]:
    original = _transition_target({
        'target_world': options.get('world'),
        'target_area': options.get('area'),
        'target_room': options.get('room'),
        'target_door_id': options.get('source_door_id'),
    }, root)
    original_id = original['target_door_id']
    if original_id == 0:
        raise ValueError("a saved source project door is required")
    origin_doc = _saved_room_document_0115(root, original)
    source_door = next((door for door in origin_doc['doors']
                        if door['id'] == original_id), None)
    if source_door is None:
        raise ValueError("source project door is missing")
    forwards = [link for link in origin_doc['transitions']
                if link['source_door_id'] == original_id]
    if len(forwards) != 1:
        raise ValueError("save the forward destination and its source room first")
    forward = forwards[0]
    if forward['target_door_id'] == 0:
        raise ValueError("choose and save an exact destination project door first")
    target = _transition_target({
        'target_world': ('zero_mission' if forward['target_world'] == 'mzm' else 'aria'),
        'target_area': forward['target_area'],
        'target_room': forward['target_room'],
        'target_door_id': forward['target_door_id'],
    }, root)
    dest_doc = _saved_room_document_0115(root, target)
    if any(link['source_door_id'] == target['target_door_id']
           for link in dest_doc['transitions']):
        raise ValueError("destination project door already has a transition; no overwrite")
    return {
        'source_world': ('zero_mission' if target['target_world'] == 'mzm' else 'aria'),
        'source_area': target['target_area'],
        'source_room': target['target_room'],
        'source_door_id': target['target_door_id'],
        'target_world': ('zero_mission' if original['target_world'] == 'mzm' else 'aria'),
        'target_area': original['target_area'],
        'target_room': original['target_room'],
        'target_door_id': original_id,
        'spawn_x': source_door['x'],
        'spawn_y': source_door['y'],
        'status': 'prepared_only_not_persisted',
        'engine_adapter': 'unavailable',
    }


def _transition_target(options: dict[str, Any], root: Path = ROOT) -> dict[str, Any]:
    public_world = _world(options.get("target_world"))
    area_value = options.get("target_area")
    if public_world == "zero_mission":
        if area_value is None:
            raise ValueError("target_area is required")
        if str(area_value).isdecimal():
            index = _integer(area_value, "target_area", 0, 6)
            area = authored_rooms.AREAS[public_world][index]
        else:
            area = next((name for name in authored_rooms.AREAS[public_world]
                         if name.lower() == str(area_value).lower()), None)
            if area is None:
                raise ValueError("unknown Zero Mission target area")
        internal_world = "mzm"
    else:
        area = str(_integer(area_value, "target_area", 0, 11))
        internal_world = "aria"
    room = _integer(options.get("target_room"), "target_room", 0, 999)
    matches = [item for item in _filter_native_rooms(public_world, area)
               if item["room"] == room]
    if len(matches) != 1:
        raise ValueError("transition target room is missing or ambiguous")
    target = {
        "target_world": internal_world, "target_area": area,
        "target_room": room,
        "target_door_id": _integer(
            options.get("target_door_id", 0), "target_door_id", 0, 999999),
        "spawn_x": _integer(options.get("spawn_x", 0), "spawn_x", 0, 16383),
        "spawn_y": _integer(options.get("spawn_y", 0), "spawn_y", 0, 16383),
    }
    if target["target_door_id"] and not any(
        door["id"] == target["target_door_id"]
        for door in _saved_target_doors(root, target)
    ):
        raise ValueError("target project door ID is not present in the saved target room")
    return target


def _transition_validation(document: dict, root: Path = ROOT) -> list[dict]:
    results = []
    for transition in document["transitions"]:
        public_world = ("zero_mission" if transition["target_world"] == "mzm"
                        else "aria")
        matches = [item for item in _filter_native_rooms(
            public_world, transition["target_area"])
            if item["room"] == transition["target_room"]]
        door_id = transition["target_door_id"]
        found = (door_id == 0 or (len(matches) == 1 and any(
            door["id"] == door_id for door in _saved_target_doors(root, transition))))
        results.append({
            "id": transition["id"], "source_door_id": transition["source_door_id"],
            "target_exists": len(matches) == 1,
            "target": f"{public_world}:{transition['target_area']}:{transition['target_room']}",
            "target_door_status": ("unspecified" if door_id == 0 else
                                   "saved_project_door_verified" if found else
                                   "missing_target_project_door"),
            "engine_adapter": "unavailable",
        })
    return results


def _entity_settings(options: dict[str, Any], native_type: Any) -> dict | None:
    keys = ("item_id", "parameter_0", "parameter_1", "flags")
    values = [options.get(key) for key in keys]
    if all(value is None for value in values):
        return None
    if any(value is None for value in values):
        raise ValueError("item settings require item_id, parameter_0, parameter_1 and flags")
    return project_room_entities.item_settings(
        native_type,
        _integer(values[0], "item_id", 0, 65535),
        _integer(values[1], "parameter_0", 0, 65535),
        _integer(values[2], "parameter_1", 0, 65535),
        _integer(values[3], "flags", 0, 255),
    )


def _input_toml(path_value: Any) -> tuple[Path, str, dict]:
    if not isinstance(path_value, str) or not path_value:
        raise ValueError("input TOML path is required")
    path = Path(path_value)
    absolute = path if path.is_absolute() else Path.cwd() / path
    current = Path(absolute.anchor)
    for part in absolute.parts[1:]:
        current /= part
        if current.is_symlink():
            raise ValueError(f"symlink input TOML component refused: {current}")
    if not absolute.is_file() or absolute.stat().st_size > 2_000_000:
        raise ValueError("input TOML must be a bounded non-symlink file")
    text = absolute.read_text(encoding="utf-8")
    return absolute, text, tomllib.loads(text)


def _require_repository_root(root: Path) -> None:
    if root != ROOT.resolve():
        raise ValueError("native workspace operations require the repository root")


def _native_workspace_paths(root: Path, room: dict) -> tuple[Path, Path]:
    extracted = root / "assets/extracted"
    if room["world"] == "zero_mission":
        basename = f"{room['area'].lower()}_{room['room']:03d}.mvnative"
        base = extracted / "rooms/metroid/workrooms" / basename
        override = extracted / "overrides/metroid" / basename
    else:
        basename = f"area_{room['area_index']:02d}_room_{room['room']:03d}.mvnative"
        base = extracted / "rooms/aria/workrooms" / basename
        override = extracted / "overrides/aria" / basename
    return base, override


def _check_native_path(root: Path, path: Path, *, existing: bool) -> None:
    if root not in path.parents:
        raise ValueError("native workspace path escaped the repository")
    current = root
    for part in path.relative_to(root).parts:
        current /= part
        if current.is_symlink():
            raise ValueError(f"symlink native workspace component refused: {current}")
    if existing and (not path.is_file() or path.stat().st_size > 1_000_000):
        raise ValueError("native workspace must be a bounded regular file")
    if not existing and path.exists() and not path.is_file():
        raise ValueError("native workspace target must be a regular file")


def _native_room(root: Path, options: dict[str, Any]) -> dict:
    _require_repository_root(root)
    world = _world(options.get("world"))
    room_id = _integer(options.get("room"), "room", 0, 999)
    matches = [room for room in _filter_native_rooms(world, options.get("area"))
               if room["room"] == room_id]
    if len(matches) != 1:
        raise ValueError("native room identity is missing or ambiguous")
    return matches[0]


def _native_map_tool(root: Path, arguments: list[str]) -> list[str]:
    configured = os.environ.get("FUSION_NATIVE_MAP_TOOL")
    tool = Path(configured) if configured else root / "build/fusion_native_map_cli"
    absolute = tool if tool.is_absolute() else root / tool
    current = Path(absolute.anchor)
    for part in absolute.parts[1:]:
        current /= part
        if current.is_symlink():
            raise ValueError(f"symlink native map adapter component refused: {current}")
    if (not absolute.is_file() or absolute.stat().st_size > 20_000_000 or
            not os.access(absolute, os.X_OK)):
        raise ValueError("native map adapter is not executable; build the project first")
    completed = subprocess.run(
        [str(absolute), *arguments], cwd=root, text=True, capture_output=True,
        check=False, timeout=15)
    if completed.returncode != 0:
        message = completed.stderr.strip() or "native map adapter failed"
        raise ValueError(message)
    return completed.stdout.rstrip("\n").split("\t")


def _validate_story(kind: Any, data: dict) -> int:
    if kind == "timeline":
        return validate_timeline(data)
    if kind == "cutscene":
        return validate_scene(data)
    raise ValueError("kind must be timeline or cutscene")


def _story_target(root: Path, kind: str, value: Any) -> Path:
    if not isinstance(value, str) or "\\" in value or ".." in Path(value).parts:
        raise ValueError("unsafe story target")
    if kind == "timeline" and value != "data/story/timeline.toml":
        raise ValueError("timeline target must be data/story/timeline.toml")
    if kind == "cutscene" and not re.fullmatch(
            r"data/cutscenes/[a-z][a-z0-9_]{0,79}\.toml", value):
        raise ValueError("cutscene target must be a safe data/cutscenes TOML path")
    target = root / value
    current = root
    for part in target.relative_to(root).parts:
        current /= part
        if current.is_symlink():
            raise ValueError(f"symlink story target refused: {current}")
    if not target.parent.is_dir():
        raise ValueError("story target parent is missing")
    return target


def _atomic_text(path: Path, text: str) -> None:
    temporary: str | None = None
    try:
        descriptor, temporary = tempfile.mkstemp(
            prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        if path.is_symlink():
            raise ValueError("story target became a symlink")
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None and os.path.exists(temporary):
            os.unlink(temporary)


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
    placements, _draft_index = map_placements.load(root)
    entity_documents = []
    entity_root = root / "assets/extracted/overrides"
    if entity_root.is_symlink():
        raise ValueError("project entity root symlink refused")
    for path in sorted(entity_root.glob("*/entities/*.json")):
        if path.is_symlink() or path.stat().st_size > 4_000_000:
            raise ValueError(f"unsafe project room document: {path}")
        document = project_room_entities.migrate(
            json.loads(path.read_text(encoding="utf-8")))
        project_room_entities.validate(
            document, document.get("world"), document.get("area"),
            document.get("room"), document.get("width_px"), document.get("height_px"))
        entity_documents.append({
            "path": str(path.relative_to(root)),
            "entity_count": len(document["entities"]),
            "collision_cell_count": len(document["collision"]["cells"]),
            "door_count": len(document["doors"]),
            "transition_count": len(document["transitions"]),
        })
    return {"timeline_events": timeline_count, "cutscenes": scenes,
            "draft_rooms": [room["id"] for room in drafts],
            "draft_placements": placements,
            "project_room_documents": entity_documents,
            # Compatibility key retained for automation written against backend 1.1.
            "project_entity_documents": entity_documents,
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
    if command == "project-audit":
        workers = _integer(options.get("workers", 1), "workers", 1, 8)
        validation = _project_validation(root_path)
        render_audits = {
            world: audit_world(world, workers=workers) for world in authored_rooms.AREAS
        }
        return {"validation": validation, "render_audits": render_audits}
    if command == "list-worlds":
        return {"worlds": [{"id": world, "areas": len(areas),
                            "engine_adapter": "unavailable"}
                           for world, areas in authored_rooms.AREAS.items()]}
    if command == "list-areas":
        world = _world(options.get("world"))
        return {"world": world, "areas": [
            {"index": index, "name": name}
            for index, name in enumerate(authored_rooms.AREAS[world])]}
    if command == "list-assets":
        world = options.get("world")
        records = _object_records(world)
        return {"world": world, "kind": "native_object_metadata",
                "count": len(records), "assets": records}
    if command in ("list-rooms", "room-list"):
        source = options.get("source", "native")
        if source == "native":
            world = _world(options.get("world"))
            rooms = _filter_native_rooms(world, options.get("area"))
        elif source == "draft":
            world = (_world(options.get("world"))
                     if options.get("world") is not None else None)
            rooms = authored_rooms.list_rooms(root_path, world)
            if options.get("area") is not None:
                area = _integer(options["area"], "area", 0, 99)
                rooms = [room for room in rooms if room["area"]["index"] == area]
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
    if command == "room-open":
        world = _world(options.get("world"))
        room_id = _integer(options.get("room"), "room", 0, 999)
        matches = _filter_native_rooms(world, options.get("area"))
        matches = [room for room in matches if room["room"] == room_id]
        if len(matches) != 1:
            raise ValueError("native room identity is missing or ambiguous")
        if dry_run:
            return {"room": matches[0]["native_id"], "persisted": False,
                    "status": "validated_native_workspace_import"}
        _require_repository_root(root_path)
        if world == "zero_mission":
            from scripts.mzm_native_workspace import export
            workspace = export(matches[0]["area"], room_id)
        else:
            from scripts.aos_native_workspace import export
            from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1
            workspace = export(verified_rom(DEFAULT_ROM, EXPECTED_SHA1),
                               matches[0]["area_index"], room_id)
        paths = [workspace[key] for key in (
            "atlas", "base", "workroom", "override", "collision", "annotations"
        ) if workspace.get(key) and Path(workspace[key]).exists()]
        return {"room": matches[0]["native_id"], "workspace": workspace,
                "persisted": True, "created_paths": paths}
    if command in ("layer-list", "tile-get", "tile-set", "tile-fill"):
        room = _native_room(root_path, options)
        base, override = _native_workspace_paths(root_path, room)
        _check_native_path(root_path, override, existing=False)
        source = override if override.exists() else base
        _check_native_path(root_path, source, existing=True)
        if command == "layer-list":
            fields = _native_map_tool(root_path, [
                "--command=inspect", f"--input={source}"])
            if len(fields) != 8 or fields[0] != "MAP":
                raise ValueError("invalid native map inspection response")
            return {
                "room": fields[1], "tile_count": _integer(fields[2], "tile_count", 1, 1024),
                "tileset": _integer(fields[7], "tileset", 0, 78),
                "layers": [
                    {"id": "bg1", "width": _integer(fields[3], "width", 1, 255),
                     "height": _integer(fields[4], "height", 1, 255)},
                    {"id": "bg2", "width": _integer(fields[5], "width", 1, 255),
                     "height": _integer(fields[6], "height", 1, 255)},
                ],
                "source": str(source),
            }
        layer = options.get("layer")
        if layer not in ("bg1", "bg2"):
            raise ValueError("layer must be bg1 or bg2")
        arguments = [
            f"--command={'get' if command == 'tile-get' else command[5:]}",
            f"--input={source}", f"--layer={layer}",
            f"--x={_integer(options.get('x'), 'x', 0, 254)}",
            f"--y={_integer(options.get('y'), 'y', 0, 254)}",
        ]
        if command == "tile-get":
            fields = _native_map_tool(root_path, arguments)
            if len(fields) != 2 or fields[0] != "VALUE":
                raise ValueError("invalid native tile response")
            return {"room": room["native_id"], "layer": layer,
                    "x": _integer(options.get("x"), "x", 0, 254),
                    "y": _integer(options.get("y"), "y", 0, 254),
                    "tile_id": _integer(fields[1], "tile_id", 0, 1023),
                    "source": str(source)}
        tile_id = _integer(options.get("tile_id"), "tile_id", 0, 1023)
        arguments.extend([f"--tile={tile_id}",
                          f"--dry-run={'true' if dry_run else 'false'}"])
        if not dry_run:
            override.parent.mkdir(parents=True, exist_ok=True)
            _check_native_path(root_path, override, existing=False)
            arguments.append(f"--output={override}")
        fields = _native_map_tool(root_path, arguments)
        if len(fields) != 2 or fields[0] != "CHANGED" or fields[1] not in ("0", "1"):
            raise ValueError("invalid native tile mutation response")
        changed = fields[1] == "1"
        result = {"room": room["native_id"], "layer": layer,
                  "x": _integer(options.get("x"), "x", 0, 254),
                  "y": _integer(options.get("y"), "y", 0, 254),
                  "tile_id": tile_id, "changed": changed,
                  "persisted": changed and not dry_run}
        if changed and not dry_run:
            result["path"] = str(override)
        return result
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
    if command in ("room-place", "room-move"):
        identity = options.get("room")
        if not isinstance(identity, str) or not identity:
            raise ValueError("room draft identity is required")
        x = _integer(options.get("x"), "x", 0, 127)
        y = _integer(options.get("y"), "y", 0, 127)
        placement, _entries, _rooms = map_placements.plan_place(
            identity, x, y, root_path)
        if dry_run:
            return {"placement": placement,
                    "persisted": False}
        placement = map_placements.place(identity, x, y, root_path)
        return {"placement": placement, "persisted": True,
                "path": str(map_placements.path_for(root_path))}
    if command == "room-unplace":
        identity = options.get("room")
        if options.get("confirm") is not True:
            raise ValueError("room-unplace requires --confirm=true")
        _draft_by_id(root_path, identity)
        placements, _rooms = map_placements.load(root_path)
        would_remove = any(entry["id"] == identity for entry in placements)
        if dry_run:
            return {"room": identity, "would_remove": would_remove,
                    "persisted": False}
        removed = map_placements.remove(identity, root_path)
        return {"room": identity, "removed": removed, "persisted": True,
                "path": str(map_placements.path_for(root_path))}
    if command == "placement-list":
        world = options.get("world")
        if world is not None:
            world = _world(world)
        entries, rooms = map_placements.load(root_path)
        placements = []
        for entry in entries:
            room = rooms[entry["id"]]
            if world is not None and room["world"] != world:
                continue
            placements.append({**entry, "name": room["name"],
                               "world": room["world"],
                               "area": room["area"]["index"],
                               "width_screens": room["geometry"]["width_screens"],
                               "height_screens": room["geometry"]["height_screens"]})
        return {"world": world, "count": len(placements), "placements": placements}
    if command.startswith("entity-"):
        if command == "entity-catalog":
            public_world = _world(options.get("world"))
            world = "mzm" if public_world == "zero_mission" else "aria"
            kind = options.get("kind")
            records = project_room_entities.editor_catalog_options(world, kind)
            return {"world": public_world, "kind": kind,
                    "count": len(records), "definitions": records}
        world, area, room, width, height = _entity_scope(options)
        document = project_room_entities.load(
            root_path, world, area, room, width, height)
        if command == "entity-list":
            return {"world": "zero_mission" if world == "mzm" else "aria",
                    "area": area, "room": room,
                    "preview": options.get("preview") is True,
                    "count": len(document["entities"]),
                    "entities": document["entities"]}
        entity_id = None if command == "entity-create" else _integer(
            options.get("id"), "id", 1, 999999)
        current = next((entry for entry in document["entities"]
                        if entry["id"] == entity_id), None)
        if command in ("entity-inspect", "entity-item-settings"):
            if current is None:
                raise ValueError("unknown project entity id")
            if command == "entity-item-settings" and (
                    world != "aria" or current["kind"] != "ITEM"):
                raise ValueError("entity is not an Aria project item")
            return {"entity": current,
                    "settings": current.get("settings", {})}
        changed = None
        if command == "entity-create":
            native_type = options.get("native_type", "unassigned")
            settings = _entity_settings(options, native_type)
            changed = project_room_entities.create(
                document, options.get("kind"),
                _integer(options.get("x"), "x", 0, 16383),
                _integer(options.get("y"), "y", 0, 16383),
                options.get("label"), native_type, settings)
        elif command == "entity-move":
            project_room_entities.move(
                document, entity_id,
                _integer(options.get("x"), "x", 0, 16383),
                _integer(options.get("y"), "y", 0, 16383))
            changed = next(entry for entry in document["entities"]
                           if entry["id"] == entity_id)
        elif command == "entity-delete":
            if options.get("confirm") is not True:
                raise ValueError("entity-delete requires --confirm=true")
            project_room_entities.delete(document, entity_id)
            changed = {"id": entity_id, "deleted": True}
        elif command == "entity-assign":
            native_type = options.get("native_type")
            settings = _entity_settings(options, native_type)
            project_room_entities.assign(
                document, entity_id, native_type, settings=settings)
            changed = next(entry for entry in document["entities"]
                           if entry["id"] == entity_id)
        else:
            raise AssertionError(f"unhandled entity command: {command}")
        if dry_run:
            return {"entity": changed, "persisted": False}
        path = project_room_entities.save(root_path, document)
        return {"entity": changed, "persisted": True, "path": str(path)}
    if command.startswith("collision-"):
        document, _scope = _room_document(root_path, options)
        resolution = document["collision"]["resolution_px"]
        if command == "collision-list":
            return {"resolution_px": resolution,
                    "grid_width": document["width_px"] // resolution,
                    "grid_height": document["height_px"] // resolution,
                    "count": len(document["collision"]["cells"]),
                    "cells": document["collision"]["cells"],
                    "engine_adapter": "unavailable"}
        if command == "collision-validate":
            return {"status": "valid_project_data_not_playable",
                    "resolution_px": resolution,
                    "cell_count": len(document["collision"]["cells"]),
                    "accepted_types": list(project_room_entities.COLLISION_TYPES),
                    "engine_adapter": "unavailable"}
        x = _integer(options.get("x"), "x", 0, 2047)
        y = _integer(options.get("y"), "y", 0, 2047)
        if command == "collision-get":
            return {"x": x, "y": y,
                    "type": project_room_entities.collision_get(document, x, y),
                    "resolution_px": resolution}
        default_extent = None if command == "collision-fill" else 1
        fill_width = _integer(
            options.get("fill_width", default_extent), "fill_width", 1, 2048)
        fill_height = _integer(
            options.get("fill_height", default_extent), "fill_height", 1, 2048)
        if command == "collision-clear":
            if options.get("confirm") is not True:
                raise ValueError("collision-clear requires --confirm=true")
            changed = project_room_entities.collision_clear(
                document, x, y, fill_width, fill_height)
            collision_type = "empty"
        else:
            collision_type = options.get("collision_type")
            changed = project_room_entities.collision_fill(
                document, x, y, fill_width, fill_height, collision_type)
        result = {"x": x, "y": y, "width": fill_width, "height": fill_height,
                  "type": collision_type, "changed_cells": changed,
                  "persisted": False, "engine_adapter": "unavailable"}
        if not dry_run and changed:
            path = project_room_entities.save(root_path, document)
            result.update({"persisted": True, "path": str(path)})
        return result
    if command == "door-return-plan":
        return _reciprocal_plan_0115(root_path, options)
    if command == "connection-list":
        public_world = _world(options.get("world"))
        source_world = "aria" if public_world == "aria" else "mzm"
        area_opt = options.get("area")
        source_area = None
        if area_opt is not None:
            if source_world == "aria":
                source_area = str(_integer(area_opt, "area", 0, 11))
            elif str(area_opt).isdecimal():
                source_area = authored_rooms.AREAS["zero_mission"][
                    _integer(area_opt, "area", 0, 6)]
            else:
                source_area = next((name for name in authored_rooms.AREAS["zero_mission"]
                                    if name.lower() == str(area_opt).lower()), None)
                if source_area is None:
                    raise ValueError("unknown Zero Mission area")
        room_opt = options.get("room")
        source_room = (_integer(room_opt, "room", 0, 999)
                       if room_opt is not None else None)
        data = project_connection_graph.connections(root_path, source_world,
                                                    source_area, source_room)
        return {"connections": data, "count": len(data),
                "source": "saved_project_rooms_only", "engine_adapter": "unavailable"}
    if command == "door-target-list":
        target = _transition_target(options, root_path)
        return {"target": target, "doors": _saved_target_doors(root_path, target),
                "engine_adapter": "unavailable"}
    if command.startswith("door-") and command != "door-link":
        document, _scope = _room_document(root_path, options)
        if command == "door-adopt":
            native_index = _integer(options.get("native_index"), "native_index", 0, 999999)
            native_variant = options.get("native_variant")
            native_type = options.get("native_type")
            if (not isinstance(native_variant, str)
                    or not re.fullmatch(r"[A-Za-z0-9_-]{1,47}", native_variant)
                    or not isinstance(native_type, str)
                    or not project_room_entities.NATIVE_TYPE.fullmatch(native_type)):
                raise ValueError("invalid original door identity")
            world, area, room, width, height = _entity_scope(options)
            family = "metroid" if world == "mzm" else "aria"
            filename = (f"{area.lower()}_{room:03}.tsv" if world == "mzm" else
                        f"area_{int(area):02}_room_{room:03}.tsv")
            original = (root_path / "assets/extracted/rooms" / family / "annotations" /
                        filename)
            if original.is_symlink() or not original.is_file() or original.stat().st_size > 2_000_000:
                raise ValueError("import original room annotations before editing native doors")
            matching = []
            for line in original.read_text(encoding="utf-8").splitlines()[:4096]:
                fields = line.split("|", 9)
                if (len(fields) != 10 or fields[0] != "DOOR" or
                    fields[1] != str(native_index) or fields[6] != native_variant or
                    fields[7] != native_type):
                    continue
                try:
                    x, y, dw, dh = (int(value) for value in fields[2:6])
                except ValueError as exc:
                    raise ValueError("invalid original door geometry") from exc
                matching.append((x, y, dw, dh))
            if len(matching) != 1:
                raise ValueError("source door missing or ambiguous in private original annotations")
            source = {"index": native_index, "variant": native_variant,
                      "native_type": native_type}
            door = project_room_entities.door_adopt(document, source, *matching[0])
            result = {"door": door, "persisted": False, "engine_adapter": "unavailable"}
            if not dry_run:
                path = project_room_entities.save(root_path, document)
                result.update({"persisted": True, "path": str(path)})
            return result
        if command == "door-list":
            return {"count": len(document["doors"]), "doors": document["doors"],
                    "engine_adapter": "unavailable"}
        door_id = None if command == "door-create" else _integer(
            options.get("id"), "id", 1, 999999)
        if command == "door-inspect":
            door = next((item for item in document["doors"]
                         if item["id"] == door_id), None)
            if door is None:
                raise ValueError("unknown project door id")
            transition = next((item for item in document["transitions"]
                               if item["source_door_id"] == door_id), None)
            return {"door": door, "transition": transition,
                    "engine_adapter": "unavailable"}
        if command == "door-delete":
            if options.get("confirm") is not True:
                raise ValueError("door-delete requires --confirm=true")
            project_room_entities.door_delete(document, door_id)
            changed = {"id": door_id, "deleted": True}
        else:
            values = {
                "x": _integer(options.get("x"), "x", 0, 16383),
                "y": _integer(options.get("y"), "y", 0, 16383),
                "width": _integer(options.get("door_width"), "door_width", 8, 16384),
                "height": _integer(options.get("door_height"), "door_height", 8, 16384),
                "label": options.get("label"), "door_type": options.get("door_type"),
                "facing": options.get("facing"),
            }
            if command == "door-create":
                changed = project_room_entities.door_create(document, **values)
            else:
                changed = project_room_entities.door_update(document, door_id, **values)
        result = {"door": changed, "persisted": False,
                  "engine_adapter": "unavailable"}
        if not dry_run:
            path = project_room_entities.save(root_path, document)
            result.update({"persisted": True, "path": str(path)})
        return result
    if command.startswith("transition-") or command == "door-link":
        document, _scope = _room_document(root_path, options)
        if command == "transition-list":
            return {"count": len(document["transitions"]),
                    "transitions": document["transitions"],
                    "engine_adapter": "unavailable"}
        if command == "transition-validate":
            records = _transition_validation(document, root_path)
            return {"status": ("valid_project_data_not_playable"
                               if all(item["target_exists"] and
                                      item["target_door_status"] != "missing_target_project_door"
                                      for item in records)
                               else "invalid_target"),
                    "count": len(records), "transitions": records,
                    "engine_adapter": "unavailable"}
        transition_id = None if command in ("transition-create", "door-link") else _integer(
            options.get("id"), "id", 1, 999999)
        if command == "transition-delete":
            if options.get("confirm") is not True:
                raise ValueError("transition-delete requires --confirm=true")
            project_room_entities.transition_delete(document, transition_id)
            changed = {"id": transition_id, "deleted": True}
        else:
            values = {
                "source_door_id": _integer(
                    options.get("source_door_id"), "source_door_id", 1, 999999),
                **_transition_target(options, root_path),
            }
            if command in ("transition-create", "door-link"):
                changed = project_room_entities.transition_create(document, **values)
            else:
                changed = project_room_entities.transition_update(
                    document, transition_id, **values)
        result = {"transition": changed, "persisted": False,
                  "engine_adapter": "unavailable"}
        if not dry_run:
            path = project_room_entities.save(root_path, document)
            result.update({"persisted": True, "path": str(path)})
        return result
    if command in ("story-validate", "story-save"):
        kind = options.get("kind")
        _path, text, data = _input_toml(options.get("input"))
        count = _validate_story(kind, data)
        result = {"kind": kind, "record_count": count, "persisted": False}
        if command == "story-save":
            target = _story_target(root_path, kind, options.get("target"))
            if not dry_run:
                _atomic_text(target, text)
                result.update({"persisted": True, "path": str(target)})
            else:
                result["target"] = str(target)
        return result
    raise AssertionError(f"unhandled backend command: {command}")
