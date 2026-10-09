#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Export bounded native room entities and transitions for the shared editor.

The generated TSV is private ROM/decomp-derived metadata. It is read-only:
project-authored objects will use a separate override format once their engine
encoders are implemented.
"""
from __future__ import annotations

import re
from pathlib import Path

from scripts.import_game_assets import OUTPUT, ROOT, write_generated
from scripts.import_mzm_rooms import ROOM_SOURCE, decode_room_descriptors
from scripts.mzm_world_atlas import AREAS, DOOR_RE, FIELD_RE, ITEM_RE, REQUIRED

SPRITESET_SOURCE = ROOT / 'third_party/mzm/src/data/spriteset.c'
ROOMS_ROOT = ROOT / 'third_party/mzm/src/data/rooms'
SET_RE = re.compile(r'const u8 sSpriteset(\d+)\[[^]]+\]\s*=\s*\{(.*?)\n\s*\};', re.S)
SET_ITEM_RE = re.compile(r'(PSPRITE_[A-Z0-9_]+)\s*,\s*(\d+)')
PLACEMENT_RE = re.compile(
    r'const u8 s([A-Za-z0-9]+)_(\d+)_Spriteset(\d+)\[[^]]+\]\s*=\s*\{(.*?)\n\s*\};',
    re.S,
)
PLACEMENT_ITEM_RE = re.compile(r'(\d+)\s*,\s*(\d+)\s*,\s*SPRITESET_IDX\((\d+)\)')


def _safe(text: object) -> str:
    value = str(text).replace('\t', ' ').replace('\r', ' ').replace('\n', ' ')
    return value[:300]


def parse_mzm_spritesets(source: str) -> dict[int, list[tuple[str, int]]]:
    result = {}
    for match in SET_RE.finditer(source):
        number = int(match.group(1))
        if number in result:
            raise ValueError(f'duplicate MZM spriteset {number}')
        result[number] = [(name, int(slot))
                          for name, slot in SET_ITEM_RE.findall(match.group(2))]
    if len(result) < 100:
        raise ValueError('incomplete MZM spriteset table')
    return result


def parse_mzm_placements(source: str, expected_area: str,
                         expected_room: int) -> dict[int, list[tuple[int, int, int]]]:
    result = {}
    for match in PLACEMENT_RE.finditer(source):
        area, room, variant, body = match.groups()
        if area != expected_area or int(room) != expected_room:
            continue
        key = int(variant)
        if key in result:
            raise ValueError('duplicate MZM room placement variant')
        result[key] = [(int(y), int(x), int(index))
                       for y, x, index in PLACEMENT_ITEM_RE.findall(body)]
    return result


def _mzm_door_entries(source: str, area: str, room: int) -> list[dict]:
    matches = [match for match in DOOR_RE.finditer(source) if match.group(1) == area]
    if len(matches) != 1:
        raise ValueError(f'missing or duplicate {area} door table')
    _, declared, body = matches[0].groups()
    entries = []
    for index, item in enumerate(ITEM_RE.finditer(body)):
        fields = {key: value.strip() for key, value in FIELD_RE.findall(item.group(1))}
        if not all(key in fields for key in REQUIRED):
            raise ValueError(f'{area} door {index}: missing fields')
        if fields['sourceRoom'].isdecimal() and int(fields['sourceRoom']) == room:
            numeric = {key: int(fields[key]) for key in REQUIRED[1:]
                       if fields[key].isdecimal()}
            if len(numeric) != len(REQUIRED) - 1:
                raise ValueError(f'{area} door {index}: invalid coordinate')
            entries.append({'index': index, 'type': fields['type'], **numeric})
    if len(list(ITEM_RE.finditer(body))) != int(declared):
        raise ValueError(f'{area}: incomplete door table')
    return entries


def build_mzm(area: str, room_number: int) -> list[tuple]:
    if area not in AREAS or not 0 <= room_number < 256:
        raise ValueError('invalid MZM room identity')
    room = next((item for item in decode_room_descriptors(
        ROOM_SOURCE.read_text(encoding='utf-8'))
                 if item['area'] == area and item['index'] == room_number), None)
    if room is None:
        raise ValueError('unknown MZM room')
    room_path = ROOMS_ROOT / area.lower() / f'{area.lower()}_{room_number}.c'
    placements = parse_mzm_placements(room_path.read_text(encoding='utf-8'),
                                      area, room_number)
    spritesets = parse_mzm_spritesets(SPRITESET_SOURCE.read_text(encoding='utf-8'))
    fields = room['fields']
    variants = [
        ('default', 0, fields['pDefaultSpriteData'], fields['defaultSpriteset']),
        ('event-1', fields.get('firstSpritesetEvent', 'EVENT_NONE'),
         fields.get('pFirstSpriteData', 'sEnemyRoomData_Empty'),
         fields.get('firstSpriteset', fields['defaultSpriteset'])),
        ('event-2', fields.get('secondSpritesetEvent', 'EVENT_NONE'),
         fields.get('pSecondSpriteData', 'sEnemyRoomData_Empty'),
         fields.get('secondSpriteset', fields['defaultSpriteset'])),
    ]
    rows = []
    source_index = 0
    for variant_name, event, symbol, spriteset_text in variants:
        symbol_match = re.fullmatch(r's[A-Za-z0-9]+_\d+_Spriteset(\d+)', symbol)
        if not symbol_match:
            continue
        variant = int(symbol_match.group(1))
        if not str(spriteset_text).isdecimal():
            raise ValueError('non-numeric MZM spriteset ID')
        definitions = spritesets.get(int(spriteset_text))
        if definitions is None:
            raise ValueError('unknown MZM spriteset ID')
        for y, x, index in placements.get(variant, []):
            if index >= len(definitions):
                raise ValueError('MZM placement references absent spriteset entry')
            sprite_name, graphics_slot = definitions[index]
            kind = 'ENTITY' if variant_name == 'default' else 'EVENT'
            details = (f'native={sprite_name}; spriteset={spriteset_text}; '
                       f'graphics_slot={graphics_slot}; variant={variant_name}; event={event}')
            rows.append((kind, source_index, x * 16, y * 16, 16, 16,
                         variant_name, sprite_name,
                         sprite_name.removeprefix('PSPRITE_').replace('_', ' ').title(),
                         details))
            source_index += 1
    for door in _mzm_door_entries(ROOM_SOURCE.read_text(encoding='utf-8'),
                                  area, room_number):
        width = (door['xEnd'] - door['xStart'] + 1) * 16
        height = (door['yEnd'] - door['yStart'] + 1) * 16
        details = (f"native_door={door['index']}; type={door['type']}; "
                   f"destination_door={door['destinationDoor']}")
        rows.append(('DOOR', door['index'], door['xStart'] * 16,
                     door['yStart'] * 16, width, height, 'native',
                     str(door['index']), f"Door {door['index']}", details))
    return rows


def build_aria(room: dict) -> list[tuple]:
    rows = []
    for index, entity in enumerate(room['entities']):
        kind = int(entity['kind'])
        entity_id = int(entity['entity_id'])
        native = f'kind-{kind}:id-{entity_id}'
        details = (f"pointer={entity['entry_pointer']}; persistent={entity['persistent_index']}; "
                   f"flags=0x{entity['flags']:02x}; parameters={entity['parameters']}")
        rows.append(('ENTITY', index, max(0, entity['x'] - 8),
                     max(0, entity['y'] - 8), 16, 16,
                     'native', native, f'Aria entity {kind}:{entity_id}', details))
    for index, transition in enumerate(room['transitions']):
        x = max(0, int(transition['source_screen_x']) * 240)
        y = max(0, int(transition['source_screen_y']) * 160)
        details = (f"pointer={transition['entry_pointer']}; target="
                   f"{transition['target_engine_area']}:{transition['target_room']}; "
                   f"load=({transition['load_x']},{transition['load_y']})")
        rows.append(('DOOR', index, x, y, 16, 16, 'screen-anchor',
                     str(index), f'Transition {index}', details))
    return rows


def serialize(rows: list[tuple]) -> bytes:
    lines = ['# MV_ROOM_ANNOTATIONS_1',
             '# kind|index|x|y|width|height|variant|native_type|label|details']
    for row in rows:
        if len(row) != 10 or row[0] not in ('ENTITY', 'DOOR', 'EVENT', 'TRIGGER'):
            raise ValueError('invalid room annotation')
        if any(type(value) is not int for value in row[1:6]):
            raise ValueError('invalid room annotation geometry')
        lines.append('|'.join(_safe(value) for value in row))
    return ('\n'.join(lines) + '\n').encode('utf-8')


def export_mzm(area: str, room: int) -> str:
    path = f'rooms/metroid/annotations/{area.lower()}_{room:03}.tsv'
    write_generated(path, serialize(build_mzm(area, room)))
    return path


def export_aria(room: dict) -> str:
    path = f"rooms/aria/annotations/area_{room['engine_area']:02}_room_{room['room']:03}.tsv"
    write_generated(path, serialize(build_aria(room)))
    return path
