#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Import the pinned original MZM room positions and intra-area door topology.

A room's mapX/mapY is a minimap anchor, not an authored bounding rectangle.
DOOR_TYPE_AREA_CONNECTION routes across areas and is NOT assigned a destination.
All generated catalogs live under ignored assets/extracted/.
"""
from __future__ import annotations
import json
import re
from pathlib import Path
from scripts.import_mzm_rooms import ROOM_SOURCE, decode_room_descriptors
from scripts.import_game_assets import write_generated

DOOR_RE = re.compile(r'const struct Door s([A-Za-z0-9]+)Doors\[(\d+)\]\s*=\s*\{(.*?)\n\};', re.S)
ITEM_RE = re.compile(r'\{([^{}]*)\}', re.S)
FIELD_RE = re.compile(r'\.([A-Za-z0-9]+)\s*=\s*([^,\n}]+)')
REQUIRED = ('type', 'sourceRoom', 'destinationDoor', 'xStart', 'xEnd', 'yStart', 'yEnd')
# Normal game areas only; Test123Doors covers several test rooms with separate numbering.
AREAS = ('Brinstar', 'Kraid', 'Norfair', 'Ridley', 'Tourian', 'Crateria', 'Chozodia')


def decode_door_tables(source: str, rooms: list[dict]) -> dict[str, list[dict]]:
    """Decode every native door entry, including self and area connections."""
    counts = {r['area']: 0 for r in rooms}
    for room in rooms:
        counts[room['area']] += 1
    tables: dict[str, list[dict]] = {}
    for match in DOOR_RE.finditer(source):
        area, size, raw = match.groups()
        if area not in AREAS:
            continue
        if area in tables:
            raise ValueError(f'duplicate door table: {area}')
        entries = []
        for index, item in enumerate(ITEM_RE.finditer(raw)):
            fields = {k: v.strip() for k, v in FIELD_RE.findall(item.group(1))}
            if not all(k in fields for k in REQUIRED):
                raise ValueError(f'{area} door {index}: missing fields')
            if not re.fullmatch(r'[A-Z0-9_ |]+', fields['type']):
                raise ValueError(f'{area} door {index}: unsupported door type')
            door = {'index': index, 'type': fields['type']}
            for key in REQUIRED[1:]:
                val = fields[key]
                if not val.isdecimal():
                    raise ValueError(f'{area} door {index}: {key} is not a decimal integer')
                door[key] = int(val)
            if door['sourceRoom'] >= counts.get(area, 0):
                raise ValueError(f'{area} door {index}: unknown source room')
            entries.append(door)
        if len(entries) != int(size):
            raise ValueError(f'{area}: {len(entries)} door entries != declared {size}')
        tables[area] = entries
    if any(area not in tables for area in AREAS):
        raise ValueError('missing original area door table')
    return tables


def decode_doors(source: str, rooms: list[dict]) -> tuple[list[dict], dict]:
    tables = decode_door_tables(source, rooms)
    connections = []
    unresolved = 0
    for area in AREAS:
        doors = tables[area]
        for door in doors:
            # Never fake inter-area destinations: these use a separate runtime table.
            if 'DOOR_TYPE_AREA_CONNECTION' in door['type']:
                unresolved += 1
                continue
            index = door['destinationDoor']
            if index >= len(doors):
                unresolved += 1
                continue
            dest = doors[index]
            # A destinationDoor indexes the same area's door table for normal doors.
            # Only represent a room-to-room link when it is a non-self transition.
            if dest['sourceRoom'] == door['sourceRoom']:
                continue
            connections.append({'area': area, 'from': door['sourceRoom'],
                                'to': dest['sourceRoom'], 'door': door['index'],
                                'destination_door': index, 'type': door['type'],
                                'x': door['xStart'], 'y': door['yStart']})
    return connections, {'door_entries': sum(map(len, tables.values())),
                         'unresolved_area_or_unknown_doors': unresolved}


def make_atlas(rooms: list[dict], connections: list[dict], summary: dict) -> dict:
    output_rooms = []
    for room in rooms:
        if room['area'] not in AREAS:
            continue
        f = room['fields']
        for field in ('mapX', 'mapY', 'tileset'):
            if not f[field].isdecimal():
                raise ValueError(f'nondecimal {field} for {room["id"]}')
        x, y = int(f['mapX']), int(f['mapY'])
        if not 0 <= x < 128 or not 0 <= y < 128:
            raise ValueError(f'invalid minimap anchor: {room["id"]}')
        output_rooms.append({'area': room['area'], 'index': room['index'],
                             'x': x, 'y': y, 'tileset': int(f['tileset']),
                             'music': f['musicTrack'],
                             'save_related': ('SAVE' in f['musicTrack'] or
                                              'SaveRoom' in f.get('pBg3Data', ''))})
    return {'format': 'MV_MZM_WORLD_ATLAS_1', 'rooms': output_rooms,
            'connections': connections, 'summary': summary,
            'warnings': ['Only original intra-area door links resolved',
                         'Area connections and event-dependent redirects not resolved',
                         'Room minimap anchors are NOT room bounds',
                         'No Castlevania world map yet']}


def render_tsv(atlas: dict) -> bytes:
    lines = ['# MV_MZM_WORLD_ATLAS_1',
             '# R|area|index|mapX|mapY|tileset|music|saveRelated',
             '# D|area|sourceRoom|destinationRoom|sourceDoor|destinationDoor']
    for r in atlas['rooms']:
        lines.append(f"R|{r['area']}|{r['index']}|{r['x']}|{r['y']}|"
                     f"{r['tileset']}|{r['music']}|{int(r['save_related'])}")
    for e in atlas['connections']:
        lines.append(f"D|{e['area']}|{e['from']}|{e['to']}|{e['door']}|{e['destination_door']}")
    return ('\n'.join(lines) + '\n').encode('ascii')


def run() -> dict:
    src = ROOM_SOURCE.read_text(encoding='utf-8')
    rooms = decode_room_descriptors(src)
    edges, stats = decode_doors(src, rooms)
    atlas = make_atlas(rooms, edges, stats)
    write_generated('rooms/metroid/world_atlas.json',
                    (json.dumps(atlas, indent=2) + '\n').encode('utf-8'))
    write_generated('rooms/metroid/world_atlas.tsv', render_tsv(atlas))
    return atlas


if __name__ == '__main__':
    try:
        data = run()
    except (OSError, ValueError, KeyError) as exc:
        raise SystemExit(f'World atlas import failed: {exc}') from exc
    print(f"MZM global map: {len(data['rooms'])} game rooms, "
          f"{len(data['connections'])} intra-area directed door records")
    print('Area transitions intentionally unresolved; no ROM files modified')
