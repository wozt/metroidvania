# SPDX-License-Identifier: GPL-3.0-only
"""Read-only graph of SAVED project doors; native ROM doors are never inferred.

Ignored editor stages are deliberately NOT visible here. Only the diskette
commits of both rooms can establish a validated reciprocal connection.
"""
from __future__ import annotations

import json
from pathlib import Path

from scripts import project_room_entities as rooms

MAX_SAVED_ROOMS = 2048
MAX_SAVED_BYTES = 4_000_000


def _saved_room(root: Path, world: str, area: str, number: int) -> dict | None:
    path = rooms.path_for(root, world, area, number, 16, 16)
    rooms._check_path(path, root)
    if not path.exists():
        return None
    if not path.is_file() or path.stat().st_size > MAX_SAVED_BYTES:
        raise ValueError('invalid saved project room document')
    try:
        raw = json.loads(path.read_text(encoding='utf-8'))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError('invalid saved project room JSON') from exc
    if not isinstance(raw, dict):
        raise ValueError('invalid saved project room object')
    raw = rooms.migrate(raw)
    width, height = raw.get('width_px'), raw.get('height_px')
    if type(width) is not int or type(height) is not int:
        raise ValueError('invalid saved project room geometry')
    return rooms.validate(raw, world, area, number, width, height)


def connections(root: Path, world: str, area: str | None = None,
                room: int | None = None) -> list[dict]:
    if world not in ('mzm', 'aria'):
        raise ValueError('unsupported project world')
    if room is not None and area is None:
        raise ValueError('room filter requires area')
    root = Path(root)
    folder = root / 'assets/extracted/overrides' / ('metroid' if world == 'mzm' else 'aria') / 'entities'
    if folder.is_symlink():
        raise ValueError('symlink project room folder refused')
    paths: list[Path]
    if room is not None:
        paths = [rooms.path_for(root, world, area, room, 16, 16)]
    else:
        paths = sorted(folder.glob('*.json')) if folder.is_dir() else []
    if len(paths) > MAX_SAVED_ROOMS:
        raise ValueError('too many project room documents')
    cache: dict[tuple[str, str, int], dict | None] = {}

    def get(w: str, a: str, n: int) -> dict | None:
        key = (w, a, n)
        if key not in cache:
            cache[key] = _saved_room(root, w, a, n)
        return cache[key]

    result = []
    for path in paths:
        if not path.exists():
            continue
        if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_SAVED_BYTES:
            raise ValueError('invalid project room listing entry')
        try:
            metadata = json.loads(path.read_text(encoding='utf-8'))
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            raise ValueError('invalid project room listing JSON') from exc
        if not isinstance(metadata, dict) or metadata.get('world') != world:
            raise ValueError('project room world mismatch')
        a, n = metadata.get('area'), metadata.get('room')
        if not isinstance(a, str) or type(n) is not int:
            raise ValueError('invalid project room listing scope')
        expected = rooms.path_for(root, world, a, n, 16, 16)
        if path != expected:
            raise ValueError('project room filename does not match identity')
        if area is not None and a != area:
            continue
        doc = get(world, a, n)
        if doc is None:
            continue
        outgoing = {t['source_door_id']: t for t in doc['transitions']}
        for door in doc['doors']:
            link = outgoing.get(door['id'])
            reciprocal = False
            target = None
            if link is None:
                state = 'missing'
            else:
                if link['target_door_id']:
                    target = get(link['target_world'], link['target_area'], link['target_room'])
                    if not target or not any(x['id'] == link['target_door_id'] for x in target['doors']):
                        state = 'invalid'
                    else:
                        reciprocal = any(
                            t['source_door_id'] == link['target_door_id'] and
                            t['target_world'] == world and
                            t['target_area'] == a and
                            t['target_room'] == n and
                            t['target_door_id'] == door['id']
                            for t in target['transitions'])
                        state = ('interworld' if world != link['target_world'] else
                                 'reciprocal' if reciprocal else 'simple')
                else:
                    # A room-only transition has no verified target project door.
                    state = 'interworld' if world != link['target_world'] else 'simple'
            result.append({
                'world': world, 'area': a, 'room': n, 'door_id': door['id'],
                'target_world': link['target_world'] if link else '-',
                'target_area': link['target_area'] if link else '-',
                'target_room': link['target_room'] if link else 0,
                'target_door_id': link['target_door_id'] if link else 0,
                'state': state, 'reciprocal': reciprocal,
            })
    return sorted(result, key=lambda x: (x['area'], x['room'], x['door_id']))
