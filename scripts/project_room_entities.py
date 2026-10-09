#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Unified private room authoring data for the editor (never ROM export)."""
from __future__ import annotations

import argparse
import json
import os
import re
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "metroidvania.project-room-data"
VERSION = 2
LEGACY_SCHEMA = "metroidvania.project-room-entities"
KINDS = ("ENEMY", "ITEM", "OBJECT")
MZM_AREAS = ("Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia")
NATIVE_TYPE = re.compile(r"[A-Za-z0-9_.:-]{1,64}\Z", re.ASCII)
DOC_KEYS = {
    "schema", "version", "world", "area", "room", "width_px", "height_px",
    "next_id", "entities", "collision", "next_door_id", "doors",
    "next_transition_id", "transitions",
}
ENTITY_KEYS = {"id", "kind", "x", "y", "label", "native_type"}
COLLISION_KEYS = {"resolution_px", "cells"}
COLLISION_CELL_KEYS = {"x", "y", "type"}
COLLISION_TYPES = ("solid", "one_way", "hazard", "slope_up", "slope_down")
DOOR_KEYS = {"id", "x", "y", "width", "height", "label", "door_type", "facing"}
# Optional private override reference, preserving existing version-2 room docs.
NATIVE_DOOR_SOURCE_KEYS = {"index", "variant", "native_type"}
DOOR_TYPES = ("normal", "boss", "locked", "portal", "save")
DOOR_FACINGS = ("left", "right", "up", "down")
TRANSITION_KEYS = {
    "id", "source_door_id", "target_world", "target_area", "target_room",
    "target_door_id", "spawn_x", "spawn_y",
}
# PATCH_0081_ARIA_PICKUP_THUMBNAILS
ITEM_KEYS = {"item_id", "parameter_0", "parameter_1", "flags"}
ARIA_PICKUP_LIMITS = {0: 0, 1: 255, 2: 31, 3: 58, 4: 44, 5: 55,
                      6: 24, 7: 35, 8: 5}


def item_settings(native_type: str, item_id: int, parameter_0: int,
                  parameter_1: int, flags: int) -> dict:
    """Validate a source-model Aria pickup subtype and private 12-byte record fields.

    These are project metadata only. No native placement encoder exists.
    """
    import re as _re
    match = _re.fullmatch(r'(pickup|hard-mode-pickup|all-souls-reward):([0-9A-Fa-f]{2})', native_type)
    if not match:
        raise ValueError('choose a valid Aria pickup native type before setting item fields')
    subtype = int(match.group(2), 16)
    if subtype not in ARIA_PICKUP_LIMITS:
        raise ValueError('undocumented Aria pickup subtype')
    _int(item_id, 0, ARIA_PICKUP_LIMITS[subtype], 'item_id')
    _int(parameter_0, 0, 65535, 'parameter_0')
    _int(parameter_1, 0, 65535, 'parameter_1')
    _int(flags, 0, 255, 'flags')
    return {"item_id": item_id, "parameter_0": parameter_0,
            "parameter_1": parameter_1, "flags": flags}


def requested_item_settings(args: argparse.Namespace) -> dict | None:
    fields = (args.item_id, args.parameter_0, args.parameter_1, args.flags)
    if all(value is None for value in fields):
        return None
    if any(value is None for value in fields):
        raise ValueError('item settings need --item-id --parameter-0 --parameter-1 --flags together')
    return item_settings(args.native_type, *fields)

MAX_ENTITIES = 256
MAX_DOORS = 128
MAX_TRANSITIONS = 128


def _int(value: object, low: int, high: int, label: str) -> int:
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{label} must be an integer in [{low}, {high}]")
    return value


def _text(value: object, limit: int, label: str) -> str:
    if (not isinstance(value, str) or not 1 <= len(value) <= limit
            or value != value.strip() or "|" in value or "\t" in value
            or any(ord(c) < 32 or ord(c) == 127 for c in value)):
        raise ValueError(f"invalid {label}")
    return value


def _scope(world: str, area: str, room: int, width: int, height: int) -> None:
    if world not in ("mzm", "aria"):
        raise ValueError("unsupported game")
    if world == "mzm" and area not in MZM_AREAS:
        raise ValueError("invalid Zero Mission area")
    if world == "aria" and (not area.isascii() or not area.isdecimal()
                            or not 0 <= int(area) <= 11 or area != str(int(area))):
        raise ValueError("invalid Aria area")
    _int(room, 0, 999, "room")
    _int(width, 16, 16384, "width_px")
    _int(height, 16, 16384, "height_px")
    if width % 16 or height % 16 or width * height > 6144 * 256:
        raise ValueError("invalid room cell geometry")


def _new(world: str, area: str, room: int, width: int, height: int) -> dict:
    _scope(world, area, room, width, height)
    return {"schema": SCHEMA, "version": VERSION, "world": world,
            "area": area, "room": room, "width_px": width, "height_px": height,
            "next_id": 1, "entities": [],
            "collision": {"resolution_px": 16 if world == "mzm" else 8,
                          "cells": []},
            "next_door_id": 1, "doors": [],
            "next_transition_id": 1, "transitions": []}


def migrate(document: object) -> object:
    """Upgrade the former entity-only room document without touching disk."""
    if (not isinstance(document, dict) or document.get("schema") != LEGACY_SCHEMA
            or document.get("version") != 1):
        return document
    legacy_keys = {
        "schema", "version", "world", "area", "room", "width_px", "height_px",
        "next_id", "entities",
    }
    if document.keys() != legacy_keys:
        raise ValueError("invalid legacy project room document fields")
    return {
        **document, "schema": SCHEMA, "version": VERSION,
        "collision": {"resolution_px": 16 if document.get("world") == "mzm" else 8,
                      "cells": []},
        "next_door_id": 1, "doors": [],
        "next_transition_id": 1, "transitions": [],
    }


def validate(doc: object, world: str, area: str, room: int,
             width: int, height: int) -> dict:
    _scope(world, area, room, width, height)
    if not isinstance(doc, dict) or doc.keys() != DOC_KEYS:
        raise ValueError("invalid project entity document fields")
    if (doc["schema"] != SCHEMA or type(doc["version"]) is not int
            or doc["version"] != VERSION or doc["world"] != world
            or doc["area"] != area or doc["room"] != room
            or doc["width_px"] != width or doc["height_px"] != height):
        raise ValueError("project entity document belongs to another room or geometry")
    next_id = _int(doc["next_id"], 1, 1000000, "next_id")
    entries = doc["entities"]
    if not isinstance(entries, list) or len(entries) > MAX_ENTITIES:
        raise ValueError("invalid project entity count")
    ids = set()
    for entity in entries:
        if (not isinstance(entity, dict) or
                entity.keys() not in (ENTITY_KEYS, ENTITY_KEYS | {"settings"})):
            raise ValueError("invalid project entity fields")
        if "settings" in entity:
            if doc["world"] != "aria" or entity["kind"] != "ITEM":
                raise ValueError('typed pickup fields only belong to Aria items')
            settings = entity["settings"]
            if not isinstance(settings, dict) or settings.keys() != ITEM_KEYS:
                raise ValueError('invalid Aria pickup settings')
            item_settings(entity["native_type"], **settings)
        eid = _int(entity["id"], 1, next_id - 1, "entity id")
        if eid in ids:
            raise ValueError("duplicate entity id")
        ids.add(eid)
        if entity["kind"] not in KINDS:
            raise ValueError("unknown entity kind")
        # Aria's native object-placement grid is 8px; Zero Mission uses 16px.
        # Keep the existing 16x16 project marker collision/bounds footprint.
        entity_step = 8 if world == "aria" else 16
        for axis, limit in (("x", width), ("y", height)):
            value = _int(entity[axis], 0, limit - 16, axis)
            if value % entity_step:
                raise ValueError(f"{axis} must align to {entity_step} pixels")
        _text(entity["label"], 80, "entity label")
        if not isinstance(entity["native_type"], str) or not NATIVE_TYPE.fullmatch(entity["native_type"]):
            raise ValueError("invalid native type token")
    collision = doc["collision"]
    resolution = 16 if world == "mzm" else 8
    if (not isinstance(collision, dict) or collision.keys() != COLLISION_KEYS
            or collision["resolution_px"] != resolution
            or not isinstance(collision["cells"], list)):
        raise ValueError("invalid project collision document")
    collision_cells = collision["cells"]
    maximum_cells = (width // resolution) * (height // resolution)
    if len(collision_cells) > maximum_cells:
        raise ValueError("invalid project collision cell count")
    coordinates = set()
    for cell in collision_cells:
        if not isinstance(cell, dict) or cell.keys() != COLLISION_CELL_KEYS:
            raise ValueError("invalid project collision cell fields")
        x = _int(cell["x"], 0, width // resolution - 1, "collision x")
        y = _int(cell["y"], 0, height // resolution - 1, "collision y")
        if (x, y) in coordinates:
            raise ValueError("duplicate project collision cell")
        coordinates.add((x, y))
        if cell["type"] not in COLLISION_TYPES:
            raise ValueError("unknown project collision type")
    next_door_id = _int(doc["next_door_id"], 1, 1000000, "next_door_id")
    doors = doc["doors"]
    if not isinstance(doors, list) or len(doors) > MAX_DOORS:
        raise ValueError("invalid project door count")
    door_ids = set()
    adopted_sources = set()
    for door in doors:
        if (not isinstance(door, dict) or
                door.keys() not in (DOOR_KEYS, DOOR_KEYS | {"native_source"})):
            raise ValueError("invalid project door fields")
        if "native_source" in door:
            source = door["native_source"]
            if not isinstance(source, dict) or source.keys() != NATIVE_DOOR_SOURCE_KEYS:
                raise ValueError("invalid native source reference")
            native_index = _int(source["index"], 0, 999999, "native door index")
            variant, native_type = source["variant"], source["native_type"]
            if (not isinstance(variant, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,47}", variant)
                    or not isinstance(native_type, str) or not NATIVE_TYPE.fullmatch(native_type)):
                raise ValueError("invalid native door source identity")
            identity = (native_index, variant, native_type)
            if identity in adopted_sources:
                raise ValueError("native door cannot have multiple project overrides")
            adopted_sources.add(identity)
        door_id = _int(door["id"], 1, next_door_id - 1, "door id")
        if door_id in door_ids:
            raise ValueError("duplicate project door id")
        door_ids.add(door_id)
        for axis, limit in (("x", width), ("y", height)):
            value = _int(door[axis], 0, limit - resolution, f"door {axis}")
            if value % resolution:
                raise ValueError(f"door {axis} must align to collision resolution")
        for extent, limit, origin in (("width", width, door["x"]),
                                      ("height", height, door["y"])):
            value = _int(door[extent], resolution, limit, f"door {extent}")
            if value % resolution or origin + value > limit:
                raise ValueError(f"invalid door {extent}")
        _text(door["label"], 80, "door label")
        if door["door_type"] not in DOOR_TYPES or door["facing"] not in DOOR_FACINGS:
            raise ValueError("unknown project door type or facing")
    next_transition_id = _int(
        doc["next_transition_id"], 1, 1000000, "next_transition_id")
    transitions = doc["transitions"]
    if not isinstance(transitions, list) or len(transitions) > MAX_TRANSITIONS:
        raise ValueError("invalid project transition count")
    transition_ids = set()
    transition_sources = set()
    for transition in transitions:
        if not isinstance(transition, dict) or transition.keys() != TRANSITION_KEYS:
            raise ValueError("invalid project transition fields")
        transition_id = _int(
            transition["id"], 1, next_transition_id - 1, "transition id")
        if transition_id in transition_ids:
            raise ValueError("duplicate project transition id")
        transition_ids.add(transition_id)
        source = _int(transition["source_door_id"], 1, next_door_id - 1,
                      "source_door_id")
        if source not in door_ids or source in transition_sources:
            raise ValueError("transition source door is missing or already linked")
        transition_sources.add(source)
        target_world = transition["target_world"]
        target_area = transition["target_area"]
        if target_world == "mzm":
            if target_area not in MZM_AREAS:
                raise ValueError("invalid transition target area")
        elif target_world == "aria":
            if (not isinstance(target_area, str) or not target_area.isdecimal()
                    or target_area != str(int(target_area))
                    or not 0 <= int(target_area) <= 11):
                raise ValueError("invalid transition target area")
        else:
            raise ValueError("invalid transition target world")
        _int(transition["target_room"], 0, 999, "target_room")
        _int(transition["target_door_id"], 0, 999999, "target_door_id")
        _int(transition["spawn_x"], 0, 16383, "spawn_x")
        _int(transition["spawn_y"], 0, 16383, "spawn_y")
    return doc


def path_for(root: Path, world: str, area: str, room: int,
             width: int, height: int) -> Path:
    _scope(world, area, room, width, height)
    folder = "metroid" if world == "mzm" else "aria"
    filename = f"{area.lower()}_{room:03d}.json" if world == "mzm" else f"area_{int(area):02d}_room_{room:03d}.json"
    return Path(root) / "assets" / "extracted" / "overrides" / folder / "entities" / filename


def _check_path(path: Path, root: Path, create: bool = False) -> None:
    current = Path(root)
    if current.is_symlink():
        raise ValueError("project root symlink refused")
    for part in path.relative_to(current).parts[:-1]:
        current = current / part
        if current.is_symlink():
            raise ValueError("project entity parent symlink refused")
        if current.exists() and not current.is_dir():
            raise ValueError("entity parent is not a directory")
        if create:
            current.mkdir(exist_ok=True)
    if path.is_symlink():
        raise ValueError("project entity symlink refused")


# PATCH_0109_DEFER_ROOM_SAVE: private, per-editor-tab staging document.
def _editor_stage_path(root: Path) -> Path | None:
    token = os.environ.get("MV_EDITOR_ROOM_STAGE")
    if token is None:
        return None
    if not re.fullmatch(r"[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}", token):
        raise ValueError("invalid private room editor stage token")
    return Path(root) / "assets" / "extracted" / ".editor_staging" / (token + ".json")


def load(root: Path, world: str, area: str, room: int,
         width: int, height: int) -> dict:
    path = path_for(root, world, area, room, width, height)
    _check_path(path, root)
    staged = _editor_stage_path(root)
    if staged is not None:
        _check_path(staged, root)
        if staged.exists():
            path = staged
    if not path.exists():
        return _new(world, area, room, width, height)
    if not path.is_file() or path.stat().st_size > 4_000_000:
        raise ValueError("invalid project room data file")
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError("unreadable project entity document") from exc
    return validate(migrate(doc), world, area, room, width, height)


def save(root: Path, doc: dict) -> Path:
    validate(doc, doc["world"], doc["area"], doc["room"], doc["width_px"], doc["height_px"])
    path = _editor_stage_path(root)
    if path is None:
        path = path_for(root, doc["world"], doc["area"], doc["room"], doc["width_px"], doc["height_px"])
    _check_path(path, root, create=True)
    temporary = None
    try:
        fd, temporary = tempfile.mkstemp(prefix=".project-entities-", dir=path.parent)
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(doc, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        _check_path(path, root)
        os.replace(temporary, path)
    finally:
        if temporary and os.path.exists(temporary):
            os.unlink(temporary)
    return path


def create(doc: dict, kind: str, x: int, y: int, label: str,
           native_type: str = "unassigned", settings: dict | None = None) -> dict:
    validate(doc, doc["world"], doc["area"], doc["room"], doc["width_px"], doc["height_px"])
    if not isinstance(native_type, str) or not NATIVE_TYPE.fullmatch(native_type):
        raise ValueError("invalid native type token")
    if native_type != "unassigned" and not any(
        record["native_type"] == native_type
        for record in catalog_options(doc["world"], kind)
    ):
        raise ValueError("native constructor absent or incompatible with world/role")
    if len(doc["entities"]) >= MAX_ENTITIES:
        raise ValueError("project entity limit reached")
    entry = {"id": doc["next_id"], "kind": kind, "x": x, "y": y,
             "label": label, "native_type": native_type}
    if settings is not None:
        entry["settings"] = settings
    candidate = {**doc, "next_id": doc["next_id"] + 1,
                 "entities": [*doc["entities"], entry]}
    validate(candidate, candidate["world"], candidate["area"], candidate["room"],
             candidate["width_px"], candidate["height_px"])
    doc.update(candidate)
    return entry


def move(doc: dict, eid: int, x: int, y: int) -> None:
    validate(doc, doc["world"], doc["area"], doc["room"], doc["width_px"], doc["height_px"])
    original = next((e for e in doc["entities"] if e["id"] == eid), None)
    if original is None:
        raise ValueError("unknown project entity id")
    modified = [{**e, "x": x, "y": y} if e["id"] == eid else e for e in doc["entities"]]
    candidate = {**doc, "entities": modified}
    validate(candidate, candidate["world"], candidate["area"], candidate["room"],
             candidate["width_px"], candidate["height_px"])
    doc.update(candidate)


def delete(doc: dict, eid: int) -> None:
    validate(doc, doc["world"], doc["area"], doc["room"], doc["width_px"], doc["height_px"])
    entries = [e for e in doc["entities"] if e["id"] != eid]
    if len(entries) == len(doc["entities"]):
        raise ValueError("unknown project entity id")
    doc["entities"] = entries


def _validated_candidate(doc: dict, **changes: object) -> dict:
    candidate = {**doc, **changes}
    validate(candidate, candidate["world"], candidate["area"], candidate["room"],
             candidate["width_px"], candidate["height_px"])
    return candidate


def collision_get(doc: dict, x: int, y: int) -> str:
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    resolution = doc["collision"]["resolution_px"]
    _int(x, 0, doc["width_px"] // resolution - 1, "collision x")
    _int(y, 0, doc["height_px"] // resolution - 1, "collision y")
    cell = next((cell for cell in doc["collision"]["cells"]
                 if cell["x"] == x and cell["y"] == y), None)
    return cell["type"] if cell else "empty"


def collision_fill(doc: dict, x: int, y: int, width: int, height: int,
                   collision_type: str) -> int:
    """Set a rectangular sparse project collision region, measured in cells."""
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    if collision_type not in COLLISION_TYPES:
        raise ValueError("unknown project collision type")
    resolution = doc["collision"]["resolution_px"]
    grid_width = doc["width_px"] // resolution
    grid_height = doc["height_px"] // resolution
    _int(x, 0, grid_width - 1, "collision x")
    _int(y, 0, grid_height - 1, "collision y")
    _int(width, 1, grid_width - x, "collision width")
    _int(height, 1, grid_height - y, "collision height")
    cells = {(cell["x"], cell["y"]): cell["type"]
             for cell in doc["collision"]["cells"]}
    changed = 0
    for cell_y in range(y, y + height):
        for cell_x in range(x, x + width):
            key = (cell_x, cell_y)
            if cells.get(key) != collision_type:
                cells[key] = collision_type
                changed += 1
    collision = {"resolution_px": resolution, "cells": [
        {"x": cell_x, "y": cell_y, "type": value}
        for (cell_x, cell_y), value in sorted(
            cells.items(), key=lambda item: (item[0][1], item[0][0]))
    ]}
    doc.update(_validated_candidate(doc, collision=collision))
    return changed


def collision_clear(doc: dict, x: int, y: int, width: int, height: int) -> int:
    """Clear a rectangular project collision region, measured in cells."""
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    resolution = doc["collision"]["resolution_px"]
    grid_width = doc["width_px"] // resolution
    grid_height = doc["height_px"] // resolution
    _int(x, 0, grid_width - 1, "collision x")
    _int(y, 0, grid_height - 1, "collision y")
    _int(width, 1, grid_width - x, "collision width")
    _int(height, 1, grid_height - y, "collision height")
    retained = [cell for cell in doc["collision"]["cells"]
                if not (x <= cell["x"] < x + width and
                        y <= cell["y"] < y + height)]
    changed = len(doc["collision"]["cells"]) - len(retained)
    collision = {"resolution_px": resolution, "cells": retained}
    doc.update(_validated_candidate(doc, collision=collision))
    return changed


def door_create(doc: dict, x: int, y: int, width: int, height: int,
                label: str, door_type: str, facing: str) -> dict:
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    if len(doc["doors"]) >= MAX_DOORS:
        raise ValueError("project door limit reached")
    door = {"id": doc["next_door_id"], "x": x, "y": y,
            "width": width, "height": height, "label": label,
            "door_type": door_type, "facing": facing}
    candidate = _validated_candidate(
        doc, next_door_id=doc["next_door_id"] + 1,
        doors=[*doc["doors"], door])
    doc.update(candidate)
    return door


def door_adopt(doc: dict, source: dict, x: int, y: int,
               width: int, height: int) -> dict:
    """Link an immutable native door to a separately editable project door.

    Calling twice returns the same override; original native records are never
    mutated, and deleting this door restores their visible source overlays.
    """
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    for door in doc["doors"]:
        if door.get("native_source") == source:
            return door
    if len(doc["doors"]) >= MAX_DOORS:
        raise ValueError("project door limit reached")
    door = {"id": doc["next_door_id"], "x": x, "y": y,
            "width": width, "height": height,
            "label": f"Native door {source['index']} override",
            "door_type": "normal", "facing":
                "left" if x == 0 else "right" if x + width == doc["width_px"]
                else "up" if y == 0 else "down" if y + height == doc["height_px"]
                else "left",
            "native_source": source}
    candidate = _validated_candidate(doc,
        next_door_id=doc["next_door_id"] + 1,
        doors=[*doc["doors"], door])
    doc.update(candidate)
    return door


def door_update(doc: dict, door_id: int, **changes: object) -> dict:
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    allowed = DOOR_KEYS - {"id"}
    if not changes or not set(changes) <= allowed:
        raise ValueError("door update requires only editable door fields")
    current = next((door for door in doc["doors"] if door["id"] == door_id), None)
    if current is None:
        raise ValueError("unknown project door id")
    changed = {**current, **changes}
    doors = [changed if door["id"] == door_id else door for door in doc["doors"]]
    doc.update(_validated_candidate(doc, doors=doors))
    return changed


def door_delete(doc: dict, door_id: int) -> None:
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    if any(item["source_door_id"] == door_id for item in doc["transitions"]):
        raise ValueError("delete the linked transition before deleting its door")
    doors = [door for door in doc["doors"] if door["id"] != door_id]
    if len(doors) == len(doc["doors"]):
        raise ValueError("unknown project door id")
    doc.update(_validated_candidate(doc, doors=doors))


def transition_create(doc: dict, source_door_id: int, target_world: str,
                      target_area: str, target_room: int, target_door_id: int,
                      spawn_x: int, spawn_y: int) -> dict:
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    if len(doc["transitions"]) >= MAX_TRANSITIONS:
        raise ValueError("project transition limit reached")
    transition = {
        "id": doc["next_transition_id"], "source_door_id": source_door_id,
        "target_world": target_world, "target_area": target_area,
        "target_room": target_room, "target_door_id": target_door_id,
        "spawn_x": spawn_x, "spawn_y": spawn_y,
    }
    candidate = _validated_candidate(
        doc, next_transition_id=doc["next_transition_id"] + 1,
        transitions=[*doc["transitions"], transition])
    doc.update(candidate)
    return transition


def transition_update(doc: dict, transition_id: int, **changes: object) -> dict:
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    allowed = TRANSITION_KEYS - {"id"}
    if not changes or not set(changes) <= allowed:
        raise ValueError("transition update requires only editable transition fields")
    current = next((item for item in doc["transitions"]
                    if item["id"] == transition_id), None)
    if current is None:
        raise ValueError("unknown project transition id")
    changed = {**current, **changes}
    transitions = [changed if item["id"] == transition_id else item
                   for item in doc["transitions"]]
    doc.update(_validated_candidate(doc, transitions=transitions))
    return changed


def transition_delete(doc: dict, transition_id: int) -> None:
    validate(doc, doc["world"], doc["area"], doc["room"],
             doc["width_px"], doc["height_px"])
    transitions = [item for item in doc["transitions"]
                   if item["id"] != transition_id]
    if len(transitions) == len(doc["transitions"]):
        raise ValueError("unknown project transition id")
    doc.update(_validated_candidate(doc, transitions=transitions))


# PATCH_0080_NATIVE_CATALOG_BINDING
def catalog_options(world: str, kind: str, records: list[dict] | None = None) -> list[dict]:
    """Read only verified native definitions. A role never becomes an engine encoder.

    `records` permits ROM-free tests. Aria special actors and doors follow the
    same native kind/id semantics as the room annotation exporter.
    """
    if world not in ("mzm", "aria") or kind not in KINDS:
        raise ValueError("unsupported native catalog scope")
    if records is None:
        from scripts import object_catalog as native
        records = (native.build_mzm() if world == "mzm" else
                   native.build_aria(native.load_aria(), native.load_aria_enemy_names()))
    result = []
    seen = set()
    for record in records:
        token = record.get("native_type", "")
        name = record.get("name", "")
        category = record.get("category", "")
        if not isinstance(token, str) or not NATIVE_TYPE.fullmatch(token):
            continue
        if world == "mzm":
            # mzm catalog categories are based on pinned sprite ID + native stats.
            role = {"Enemy / actor": "ENEMY", "Item / pickup": "ITEM",
                    "Upgrade / ability": "ITEM", "World object": "OBJECT"}.get(category)
        else:
            # The catalog token contains the native Aria kind, not a graphics ID.
            group, separator, identifier = token.partition(":")
            if not separator:
                continue
            try:
                entity_id = int(identifier, 16)
            except ValueError:
                continue
            if group == "enemy" or (group == "special-object" and entity_id in (10, 11)):
                role = "ENEMY"
            elif group in ("pickup", "hard-mode-pickup", "all-souls-reward"):
                role = "ITEM"
            elif group == "special-object" and entity_id in (0, 2, 3, 4, 5, 6):
                role = "DOOR"  # Dedicated door editor is not implemented yet.
            elif group in ("special-object", "generic-candle"):
                role = "OBJECT"
            else:
                role = None
        if role != kind or token in seen:
            continue
        try:
            _text(name[:80], 80, "catalog display name")
            _text(str(category)[:160], 160, "catalog category")
        except ValueError:
            continue
        seen.add(token)
        result.append({"native_type": token, "name": name[:80],
                       "category": str(category)[:160]})
    if world == "aria" and kind == "ITEM":
        # Publish documented subtype families even if never placed in the
        # original map. These are native role IDs, NOT invented item sprites.
        from scripts.object_catalog import ARIA_PICKUP_NAMES
        for family, prefix in (("Pickup", "pickup"),
                               ("Hard Mode", "hard-mode-pickup"),
                               ("All Souls", "all-souls-reward")):
            for subtype in ARIA_PICKUP_LIMITS:
                token = f"{prefix}:{subtype:02X}"
                if token not in seen:
                    result.append({"native_type": token,
                                   "name": f"{family} / {ARIA_PICKUP_NAMES[subtype]}",
                                   "category": f"Aria {family} / subtype {subtype:02X}"})
                    seen.add(token)
    return sorted(result, key=lambda e: (e["name"].casefold(), e["native_type"]))


def assign(doc: dict, eid: int, native_type: str,
           records: list[dict] | None = None,
           settings: dict | None = None) -> None:
    """Assign a source-catalog identity to ONE authored marker, atomically in memory."""
    validate(doc, doc["world"], doc["area"], doc["room"], doc["width_px"], doc["height_px"])
    old = next((e for e in doc["entities"] if e["id"] == eid), None)
    if old is None:
        raise ValueError("unknown project entity id")
    if native_type == "unassigned":
        name = old["label"]  # Do not invent an identity for legacy project markers.
    else:
        candidate = next((e for e in catalog_options(doc["world"], old["kind"], records)
                          if e["native_type"] == native_type), None)
        if candidate is None:
            raise ValueError("native type missing or category does not match this game")
        name = candidate["name"]
        if records is None and doc["world"] == "aria" and old["kind"] == "ITEM" and settings:
            try:
                from scripts.aria_item_details import item_name
                from scripts.import_game_assets import verified_rom
                from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1
                subtype = int(native_type.rsplit(":", 1)[1], 16)
                name = item_name(verified_rom(DEFAULT_ROM, EXPECTED_SHA1),
                                 subtype, settings["item_id"])
            except (OSError, ValueError, IndexError):
                pass  # Existing project labels remain valid without a verified ROM.
    edited = []
    for e in doc["entities"]:
        if e["id"] != eid:
            edited.append(e)
            continue
        updated = {**e, "native_type": native_type, "label": name}
        if settings is not None:
            updated["settings"] = settings
        elif native_type != e["native_type"]:
            updated.pop("settings", None)  # No stale subtype-specific metadata.
        edited.append(updated)
    changed = {**doc, "entities": edited}
    validate(changed, changed["world"], changed["area"], changed["room"],
             changed["width_px"], changed["height_px"])
    doc.update(changed)


def editor_catalog_options(world: str, kind: str) -> list[dict]:
    """Return the same named definition rows used by GTK and headless clients."""
    if world == "aria" and kind == "ITEM":
        try:
            from scripts.import_game_assets import verified_rom
            from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1
            from scripts.aria_item_details import named_options
            rom = verified_rom(DEFAULT_ROM, EXPECTED_SHA1)
            records = named_options(rom)
            from scripts.native_sprite_thumbnails import generate_aria_item_thumbnails
            try:
                generate_aria_item_thumbnails(rom=rom)
            except (OSError, ValueError, IndexError):
                pass
            return records
        except (OSError, ValueError, IndexError):
            pass
    return catalog_options(world, kind)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--world", choices=("mzm", "aria"), required=True)
    parser.add_argument("--area", required=True)
    parser.add_argument("--room", required=True, type=int)
    parser.add_argument("--width", required=True, type=int, help="room width in pixels")
    parser.add_argument("--height", required=True, type=int, help="room height in pixels")
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("list")
    # PATCH_0085_ROOM_SPRITE_OVERLAYS: extra item identity for PRIVATE GTK previews; original list unchanged.
    sub.add_parser("list-previews")
    add = sub.add_parser("create")
    add.add_argument("--kind", choices=KINDS, required=True)
    add.add_argument("--x", type=int, required=True)
    add.add_argument("--y", type=int, required=True)
    add.add_argument("--label", required=True)
    add.add_argument("--native-type", default="unassigned")
    for flag in ("item-id", "parameter-0", "parameter-1", "flags"):
        add.add_argument("--" + flag, type=int)
    moving = sub.add_parser("move")
    moving.add_argument("--id", type=int, required=True)
    moving.add_argument("--x", type=int, required=True)
    moving.add_argument("--y", type=int, required=True)
    removal = sub.add_parser("delete")
    removal.add_argument("--id", type=int, required=True)
    catalog = sub.add_parser("catalog", help="list available native definitions by role")
    catalog.add_argument("--kind", choices=KINDS, required=True)
    assigning = sub.add_parser("assign", help="bind one authored entity to a validated native ID")
    assigning.add_argument("--id", type=int, required=True)
    assigning.add_argument("--native-type", required=True)
    for flag in ("item-id", "parameter-0", "parameter-1", "flags"):
        assigning.add_argument("--" + flag, type=int)
    selected = sub.add_parser("item-settings", help="read private Aria pickup settings")
    selected.add_argument("--id", type=int, required=True)
    args = parser.parse_args(argv)
    try:
        doc = load(args.root, args.world, args.area, args.room, args.width, args.height)
        if args.action == "list":
            for e in doc["entities"]:
                print(f"{e['id']}\t{e['kind']}\t{e['x']}\t{e['y']}\t{e['label']}\t{e['native_type']}")
        elif args.action == "list-previews":
            for e in doc["entities"]:
                item_id = e.get("settings", {}).get("item_id", -1)
                print(f"{e['id']}\t{e['kind']}\t{e['x']}\t{e['y']}\t"
                      f"{e['label']}\t{e['native_type']}\t{item_id}")
        elif args.action == "catalog":
            records = editor_catalog_options(args.world, args.kind)
            for record in records:
                print(f"{record['native_type']}\t{record['name']}\t{record['category']}\t{record.get('item_id', -1)}")
        elif args.action == "create":
            if args.native_type != "unassigned":
                # Do not permit arbitrary constructor text in project documents.
                choices = catalog_options(args.world, args.kind)
                if not any(e["native_type"] == args.native_type for e in choices):
                    raise ValueError("native type missing or incompatible with entity kind")
            settings = requested_item_settings(args)
            if settings is not None and (args.world != "aria" or args.kind != "ITEM"):
                raise ValueError('pickup parameters require an Aria item')
            entity = create(doc, args.kind, args.x, args.y, args.label,
                            args.native_type, settings)
            save(args.root, doc)
            print(f"CREATED {entity['id']}")
        elif args.action == "assign":
            settings = requested_item_settings(args)
            if settings is not None and args.world != "aria":
                raise ValueError('pickup parameters require Aria')
            assign(doc, args.id, args.native_type, settings=settings)
            save(args.root, doc)
            print(f"ASSIGNED {args.id}")
        elif args.action == "item-settings":
            entry = next((e for e in doc["entities"] if e["id"] == args.id), None)
            if entry is None or entry["kind"] != "ITEM" or args.world != "aria":
                raise ValueError('unknown Aria project item')
            fields = entry.get("settings", {})
            print("\t".join(str(fields.get(key, 0)) for key in
                            ("item_id", "parameter_0", "parameter_1", "flags")))
        elif args.action == "move":
            move(doc, args.id, args.x, args.y)
            save(args.root, doc)
            print(f"MOVED {args.id}")
        elif args.action == "delete":
            delete(doc, args.id)
            save(args.root, doc)
            print(f"DELETED {args.id}")
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
