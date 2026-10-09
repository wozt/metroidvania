#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Versioned private entity placements for the GTK room editor (never ROM export)."""
from __future__ import annotations

import argparse
import json
import os
import re
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "metroidvania.project-room-entities"
VERSION = 1
KINDS = ("ENEMY", "ITEM", "OBJECT")
MZM_AREAS = ("Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia")
NATIVE_TYPE = re.compile(r"[A-Za-z0-9_.:-]{1,64}\Z", re.ASCII)
DOC_KEYS = {"schema", "version", "world", "area", "room", "width_px", "height_px", "next_id", "entities"}
ENTITY_KEYS = {"id", "kind", "x", "y", "label", "native_type"}
MAX_ENTITIES = 256


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
            "next_id": 1, "entities": []}


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
        if not isinstance(entity, dict) or entity.keys() != ENTITY_KEYS:
            raise ValueError("invalid project entity fields")
        eid = _int(entity["id"], 1, next_id - 1, "entity id")
        if eid in ids:
            raise ValueError("duplicate entity id")
        ids.add(eid)
        if entity["kind"] not in KINDS:
            raise ValueError("unknown entity kind")
        for axis, limit in (("x", width), ("y", height)):
            value = _int(entity[axis], 0, limit - 16, axis)
            if value % 16:
                raise ValueError(f"{axis} must be aligned to 16 pixels")
        _text(entity["label"], 80, "entity label")
        if not isinstance(entity["native_type"], str) or not NATIVE_TYPE.fullmatch(entity["native_type"]):
            raise ValueError("invalid native type token")
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


def load(root: Path, world: str, area: str, room: int,
         width: int, height: int) -> dict:
    path = path_for(root, world, area, room, width, height)
    _check_path(path, root)
    if not path.exists():
        return _new(world, area, room, width, height)
    if not path.is_file() or path.stat().st_size > 131072:
        raise ValueError("invalid project entity file")
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError("unreadable project entity document") from exc
    return validate(doc, world, area, room, width, height)


def save(root: Path, doc: dict) -> Path:
    validate(doc, doc["world"], doc["area"], doc["room"], doc["width_px"], doc["height_px"])
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
           native_type: str = "unassigned") -> dict:
    validate(doc, doc["world"], doc["area"], doc["room"], doc["width_px"], doc["height_px"])
    if len(doc["entities"]) >= MAX_ENTITIES:
        raise ValueError("project entity limit reached")
    entry = {"id": doc["next_id"], "kind": kind, "x": x, "y": y,
             "label": label, "native_type": native_type}
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
    add = sub.add_parser("create")
    add.add_argument("--kind", choices=KINDS, required=True)
    add.add_argument("--x", type=int, required=True)
    add.add_argument("--y", type=int, required=True)
    add.add_argument("--label", required=True)
    add.add_argument("--native-type", default="unassigned")
    moving = sub.add_parser("move")
    moving.add_argument("--id", type=int, required=True)
    moving.add_argument("--x", type=int, required=True)
    moving.add_argument("--y", type=int, required=True)
    removal = sub.add_parser("delete")
    removal.add_argument("--id", type=int, required=True)
    args = parser.parse_args(argv)
    try:
        doc = load(args.root, args.world, args.area, args.room, args.width, args.height)
        if args.action == "list":
            for e in doc["entities"]:
                print(f"{e['id']}\t{e['kind']}\t{e['x']}\t{e['y']}\t{e['label']}\t{e['native_type']}")
        elif args.action == "create":
            entity = create(doc, args.kind, args.x, args.y, args.label, args.native_type)
            save(args.root, doc)
            print(f"CREATED {entity['id']}")
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
