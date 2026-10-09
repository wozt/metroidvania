#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Versioned PRIVATE room draft placements on original minimap grids.

No ROM modification, native engine export, or invented original geometry.
Rooms remain explicitly unplayable project drafts.
"""
from __future__ import annotations

import argparse
import json
import os
import tempfile
from pathlib import Path
from scripts import authored_rooms

SCHEMA = 'metroidvania.map-placements'
VERSION = 1
LIMIT = 4096
GRID = {'zero_mission': (32, 32), 'aria': (64, 35)}


def path_for(root: Path = authored_rooms.ROOT) -> Path:
    return authored_rooms.private_root(root) / 'placements.json'


def _placements_file(root: Path) -> Path:
    dest = path_for(root)
    if dest.is_symlink() or (dest.exists() and not dest.is_file()):
        raise ValueError('unsafe placements path')
    if dest.exists() and dest.stat().st_size > 500_000:
        raise ValueError('oversized placements document')
    return dest


def _identity(room_id: str, rooms: dict[str, dict]) -> dict:
    if not isinstance(room_id, str) or room_id not in rooms:
        raise ValueError('placement must reference an existing private draft')
    return rooms[room_id]


def _rect(entry: dict, rooms: dict[str, dict]) -> tuple[str, int, int, int, int, int]:
    if not isinstance(entry, dict) or entry.keys() != {'id', 'x', 'y'}:
        raise ValueError('placement has missing/unknown fields')
    room = _identity(entry['id'], rooms)
    world = room['world']
    area = room['area']['index']
    x, y = entry['x'], entry['y']
    if type(x) is not int or type(y) is not int:
        raise ValueError('placement coordinates must be integers')
    width = room['geometry']['width_screens']
    height = room['geometry']['height_screens']
    xmax, ymax = GRID[world]
    if not (0 <= x and 0 <= y and x + width <= xmax and y + height <= ymax):
        raise ValueError('room footprint exceeds original minimap bounds')
    return world, area, x, y, width, height


def _occupancy(root: Path, world: str) -> set[tuple[int, int, int]]:
    """Refuse writes when the original map is missing or cannot be parsed.

    Aria uses a shared 64x35 world grid across all areas. MZM has individual
    32x32 grids per area, so the area is part of the collision key.
    """
    source = Path(root) / 'assets' / 'extracted' / 'world_overview' / (
        'aria.tsv' if world == 'aria' else 'mzm.tsv')
    if source.is_symlink() or not source.is_file() or source.stat().st_size > 2_000_000:
        raise ValueError(f'generate verified native overview first: {source}')
    occupied = set()
    count = 0
    for line in source.read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):
            continue
        parts = line.split('|')
        if len(parts) not in (6, 7, 8):
            raise ValueError('invalid original map index row')
        try:
            area, room, x, y, save, warp = (int(n) for n in parts[:6])
        except ValueError as exc:
            raise ValueError('invalid original map index coordinate') from exc
        bounds = GRID[world] if world == 'aria' else (128, 128)
        if not (0 <= area < len(authored_rooms.AREAS[world]) and
                0 <= x < bounds[0] and 0 <= y < bounds[1] and
                0 <= room < 1000 and save in (0, 1) and warp in (0, 1)):
            raise ValueError('out-of-bounds or corrupt original map index')
        occupied.add((area if world == 'zero_mission' else 0, x, y))
        count += 1
        if count > 16384:
            raise ValueError('oversized original map index')
    if not occupied:
        raise ValueError('empty original map index; placement refused')
    return occupied


def _footprint(entry: dict, rooms: dict[str, dict]) -> set[tuple[str, int, int, int]]:
    world, area, x, y, width, height = _rect(entry, rooms)
    return {(world, area if world == 'zero_mission' else 0, px, py)
            for px in range(x, x + width) for py in range(y, y + height)}


def validate(data: object, rooms: dict[str, dict]) -> list[dict]:
    if (not isinstance(data, dict) or data.keys() != {'schema', 'version', 'placements'}
            or data['schema'] != SCHEMA or type(data['version']) is not int
            or data['version'] != VERSION or type(data['placements']) is not list
            or len(data['placements']) > LIMIT):
        raise ValueError('unsupported private placement schema')
    seen_ids: set[str] = set()
    owned: set[tuple[str, int, int, int]] = set()
    for entry in data['placements']:
        footprint = _footprint(entry, rooms)
        if entry['id'] in seen_ids or footprint & owned:
            raise ValueError('duplicate draft or overlapping private footprints')
        seen_ids.add(entry['id'])
        owned.update(footprint)
    return data['placements']


def _drafts(root: Path) -> dict[str, dict]:
    return {r['id']: r for r in authored_rooms.list_rooms(root)}


def load(root: Path = authored_rooms.ROOT) -> tuple[list[dict], dict[str, dict]]:
    rooms = _drafts(root)
    file = _placements_file(root)
    if file.is_file():
        try:
            data = json.loads(file.read_text(encoding='utf-8'))
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            raise ValueError(f'invalid private placements document: {exc}') from exc
    else:
        data = {'schema': SCHEMA, 'version': VERSION, 'placements': []}
    return list(validate(data, rooms)), rooms


def _write(root: Path, entries: list[dict], rooms: dict[str, dict]) -> None:
    data = {'schema': SCHEMA, 'version': VERSION,
            'placements': sorted(entries, key=lambda item: item['id'])}
    validate(data, rooms)
    file = _placements_file(root)
    # Check ancestors BEFORE creating the destination; never follow symlinks.
    authored_rooms._safe_parents(file, root)
    descriptor, temporary = tempfile.mkstemp(prefix='.placements-', suffix='.tmp',
                                            dir=str(file.parent))
    try:
        with os.fdopen(descriptor, 'w', encoding='utf-8') as out:
            json.dump(data, out, ensure_ascii=False, indent=2)
            out.write('\n')
            out.flush()
            os.fsync(out.fileno())
        if file.is_symlink():
            raise ValueError('placements destination became symlink')
        os.replace(temporary, file)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def plan_place(room_id: str, x: int, y: int,
               root: Path = authored_rooms.ROOT) -> tuple[dict, list[dict], dict[str, dict]]:
    """Validate a placement completely without persisting it."""
    entries, rooms = load(root)
    room = _identity(room_id, rooms)
    candidate = {'id': room_id, 'x': x, 'y': y}
    world, area, _, _, _, _ = _rect(candidate, rooms)
    original = _occupancy(root, world)
    for _, grid_area, px, py in _footprint(candidate, rooms):
        if (grid_area, px, py) in original:
            raise ValueError(f'placement overlaps an original minimap tile at ({px},{py})')
    updated = [entry for entry in entries if entry['id'] != room_id]
    updated.append(candidate)
    validate({'schema': SCHEMA, 'version': VERSION, 'placements': updated}, rooms)
    return candidate, updated, rooms


def place(room_id: str, x: int, y: int, root: Path = authored_rooms.ROOT) -> dict:
    candidate, updated, rooms = plan_place(room_id, x, y, root)
    _write(root, updated, rooms)
    return candidate


def remove(room_id: str, root: Path = authored_rooms.ROOT) -> bool:
    entries, rooms = load(root)
    _identity(room_id, rooms)
    updated = [entry for entry in entries if entry['id'] != room_id]
    if len(updated) == len(entries):
        return False
    _write(root, updated, rooms)
    return True


def lines(root: Path = authored_rooms.ROOT, world: str | None = None) -> list[str]:
    entries, rooms = load(root)
    out = []
    for entry in entries:
        room = rooms[entry['id']]
        if world is not None and room['world'] != world:
            continue
        g = room['geometry']
        out.append('\t'.join((entry['id'], room['name'], room['world'],
                              str(room['area']['index']), str(entry['x']),
                              str(entry['y']), str(g['width_screens']),
                              str(g['height_screens']))))
    return out


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=authored_rooms.ROOT)
    sub = parser.add_subparsers(dest='command', required=True)
    placing = sub.add_parser('place', help='place or move an existing private draft')
    placing.add_argument('--id', required=True)
    placing.add_argument('--x', type=int, required=True)
    placing.add_argument('--y', type=int, required=True)
    removing = sub.add_parser('remove', help='unplace a draft; do not delete it')
    removing.add_argument('--id', required=True)
    listing = sub.add_parser('list', help='list already-placed private drafts')
    listing.add_argument('--world', choices=tuple(GRID))
    listing.add_argument('--format', choices=('human', 'tsv'), default='human')
    args = parser.parse_args(argv)
    try:
        if args.command == 'place':
            e = place(args.id, args.x, args.y, args.root)
            print(f"PLACED PRIVATE DRAFT {e['id']} ({e['x']},{e['y']}) — NOT PLAYABLE")
        elif args.command == 'remove':
            removed = remove(args.id, args.root)
            print('UNPLACED PRIVATE DRAFT' if removed else 'DRAFT WAS NOT PLACED')
        elif args.command == 'list':
            for line in lines(args.root, args.world):
                if args.format == 'tsv':
                    print(line)
                else:
                    parts = line.split('\t')
                    print(f'{parts[0]} ({parts[4]},{parts[5]}) — NOT PLAYABLE')
    except (ValueError, OSError, UnicodeError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
