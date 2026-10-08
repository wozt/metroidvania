#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Strict, project-owned room drafts for both game worlds (NOT gameplay data).

No ROM inputs, no extracted graphics, no original-room inferred geometry.
Drafts are private under the ignored assets/extracted/authored_rooms tree.
A validated draft has NOT passed native engine export/runtime validation.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "metroidvania.authored-room"
VERSION = 1
SLUG = re.compile(r"[a-z][a-z0-9_-]{0,39}\Z", re.ASCII)
AREAS = {
    "zero_mission": (
        "Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia",
    ),
    "aria": (
        "Castle Corridor", "Chapel", "Study", "Dance Hall", "Inner Quarters",
        "Floating Garden", "Clock Tower", "Underground", "The Arena", "Top Floor",
        "Chaotic Realm entrance", "Chaotic Realm boss",
    ),
}
KEYS = frozenset({
    "schema", "version", "id", "world", "area", "name", "slug", "origin",
    "status", "geometry", "layers", "collision", "spawn", "doors", "transitions",
    "entities", "events", "music", "engine_adapter",
})
EMPTY_COLLECTIONS = ("layers", "doors", "transitions", "entities", "events")
EMPTY_FIELDS = ("collision", "spawn", "music", "engine_adapter")


def _number(value: object, lower: int, upper: int, label: str) -> int:
    if type(value) is not int or not lower <= value <= upper:
        raise ValueError(f"{label} must be an integer in [{lower}, {upper}]")
    return value


def _identity(world: str, area: int, slug: str) -> str:
    if world not in AREAS:
        raise ValueError("unknown world")
    _number(area, 0, len(AREAS[world]) - 1, "area")
    if not isinstance(slug, str) or not SLUG.fullmatch(slug):
        raise ValueError("slug must be lowercase ASCII, start with a letter and be <=40 chars")
    return f"{world}:{area:02d}:{slug}"


def _name(value: object) -> str:
    if (not isinstance(value, str) or not 1 <= len(value) <= 80
            or value != value.strip() or any(ord(c) < 32 or ord(c) == 127 for c in value)
            or "|" in value):
        raise ValueError("name must be 1..80 printable characters without separators")
    return value


def new_room(*, world: str, area: int, slug: str, name: str,
             width_screens: int, height_screens: int) -> dict:
    """Create an unplayable authoring draft; never clone or infer ROM geometry."""
    identity = _identity(world, area, slug)
    _name(name)
    _number(width_screens, 1, 8, "width_screens")
    _number(height_screens, 1, 8, "height_screens")
    room = {
        "schema": SCHEMA,
        "version": VERSION,
        "id": identity,
        "world": world,
        "area": {"index": area, "name": AREAS[world][area]},
        "name": name,
        "slug": slug,
        "origin": "project_authored",
        "status": "draft_not_playable",
        "geometry": {"unit": "gba_screen", "width_screens": width_screens,
                     "height_screens": height_screens,
                     "screen_width_px": 240, "screen_height_px": 160},
        "layers": [],
        "collision": None,
        "spawn": None,
        "doors": [],
        "transitions": [],
        "entities": [],
        "events": [],
        "music": None,
        "engine_adapter": None,
    }
    validate_room(room)
    return room


def validate_room(room: object) -> str:
    """Strict v1 draft validation; never claim engine/playability validation."""
    if not isinstance(room, dict) or room.keys() != KEYS:
        raise ValueError("authored-room v1 has missing or unexpected fields")
    if room["schema"] != SCHEMA or type(room["version"]) is not int or room["version"] != VERSION:
        raise ValueError("unsupported authored-room schema/version")
    world, area = room["world"], room["area"]
    if not isinstance(area, dict) or area.keys() != {"index", "name"}:
        raise ValueError("invalid area descriptor")
    identity = _identity(world, area["index"], room["slug"])
    if area["name"] != AREAS[world][area["index"]] or room["id"] != identity:
        raise ValueError("room id or area name does not match its world")
    _name(room["name"])
    if room["origin"] != "project_authored" or room["status"] != "draft_not_playable":
        raise ValueError("draft provenance/status cannot claim engine readiness")
    geometry = room["geometry"]
    if not isinstance(geometry, dict) or geometry.keys() != {
        "unit", "width_screens", "height_screens", "screen_width_px", "screen_height_px",
    }:
        raise ValueError("invalid authored room geometry descriptor")
    if (geometry["unit"] != "gba_screen" or type(geometry["screen_width_px"]) is not int
            or type(geometry["screen_height_px"]) is not int
            or geometry["screen_width_px"] != 240 or geometry["screen_height_px"] != 160):
        raise ValueError("GBA screen geometry must be 240x160")
    _number(geometry["width_screens"], 1, 8, "width_screens")
    _number(geometry["height_screens"], 1, 8, "height_screens")
    for key in EMPTY_COLLECTIONS:
        if type(room[key]) is not list or room[key]:
            raise ValueError(f"{key} requires a future versioned authoring adapter")
    for key in EMPTY_FIELDS:
        if room[key] is not None:
            raise ValueError(f"{key} requires a future versioned authoring adapter")
    return identity


def _root(root: Path) -> Path:
    return Path(root).resolve()


def private_root(root: Path = ROOT) -> Path:
    return _root(root) / "assets" / "extracted" / "authored_rooms"


def room_path(room: dict, root: Path = ROOT) -> Path:
    validate_room(room)
    return (private_root(root) / room["world"] /
            f"{room['area']['index']:02d}" / f"{room['slug']}.json")


def _safe_parents(path: Path, root: Path) -> None:
    current = _root(root)
    if current.is_symlink():
        raise ValueError("project root may not be a symlink")
    for part in path.relative_to(current).parts[:-1]:
        current = current / part
        if current.is_symlink():
            raise ValueError(f"symlink parent refused: {current}")
        if current.exists() and not current.is_dir():
            raise ValueError(f"not a directory: {current}")
        current.mkdir(exist_ok=True)


def save_room(room: dict, root: Path = ROOT) -> Path:
    """Create-only persistence: refuses duplicate IDs, symlinks and overwrites."""
    path = room_path(room, root)
    _safe_parents(path, root)
    document = json.dumps(room, indent=2, ensure_ascii=False) + "\n"
    # Exclusive creation also refuses an existing symlink at the destination.
    with path.open("x", encoding="utf-8") as stream:
        stream.write(document)
    return path


def load_room(path: Path, root: Path = ROOT) -> dict:
    path = Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 16384:
        raise ValueError("refuse symlink, missing or oversized authored room")
    try:
        room = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"invalid authored room document: {exc}") from exc
    if path.resolve() != room_path(room, root):
        raise ValueError("authored room path and identity disagree")
    return room


def list_rooms(root: Path = ROOT, world: str | None = None) -> list[dict]:
    if world is not None and world not in AREAS:
        raise ValueError("unknown world")
    base = private_root(root)
    if base.is_symlink():
        raise ValueError("private room directory is a symlink")
    result = []
    for candidate in sorted(base.glob("*/*/*.json")):
        if world is not None and candidate.parts[-3] != world:
            continue
        result.append(load_room(candidate, root))
        if len(result) > 4096:
            raise ValueError("too many authored rooms")
    return result


def export_for_engine(room: dict) -> None:
    """Fail closed until both native engines can validate and consume data."""
    validate_room(room)
    raise NotImplementedError("native room engine adapters are not implemented")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT, help="project root (test fixtures only)")
    sub = ap.add_subparsers(dest="command", required=True)
    create = sub.add_parser("create", help="create private project-authored draft")
    create.add_argument("--world", choices=tuple(AREAS), required=True)
    create.add_argument("--area", type=int, required=True, help="zero-based engine area index")
    create.add_argument("--slug", required=True)
    create.add_argument("--name", required=True)
    create.add_argument("--width-screens", type=int, default=1)
    create.add_argument("--height-screens", type=int, default=1)
    check = sub.add_parser("validate", help="validate a private authored draft")
    check.add_argument("path", type=Path)
    listed = sub.add_parser("list", help="list validated private authored drafts")
    listed.add_argument("--world", choices=tuple(AREAS))
    args = ap.parse_args(argv)
    try:
        if args.command == "create":
            room = new_room(world=args.world, area=args.area, slug=args.slug,
                            name=args.name, width_screens=args.width_screens,
                            height_screens=args.height_screens)
            path = save_room(room, args.root)
            print(f"CREATED DRAFT {room['id']} -> {path} (NOT playable)")
        elif args.command == "validate":
            room = load_room(args.path, args.root)
            print(f"VALID DRAFT {room['id']} (NOT engine-ready)")
        elif args.command == "list":
            for room in list_rooms(args.root, args.world):
                print(f"{room['id']} | {room['name']} | draft_not_playable")
    except (OSError, ValueError) as exc:
        ap.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
