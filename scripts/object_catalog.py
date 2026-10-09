#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a shared, read-only catalog of decoded native object definitions."""
from __future__ import annotations

import argparse
import json
import re

from scripts.import_game_assets import OUTPUT, ROOT

MZM_ENUM = ROOT / 'third_party/mzm/include/constants/sprite.h'
MZM_STATS = ROOT / 'third_party/mzm/src/data/sprite_data.c'
PRIMARY_RE = re.compile(r'MAKE_ENUM\(u8, PrimarySprite\)\s*\{(.*?)\n\};', re.S)
STAT_BLOCK_RE = re.compile(r'\[(PSPRITE_[A-Z0-9_]+)\]\s*=\s*\{(.*?)\n\s*\},?', re.S)
STAT_RE = re.compile(r'^\s*\[(SPRITE_STATS_[A-Z0-9_]+)\]\s*=\s*(.+?)\s*$', re.M)


def _label(symbol: str) -> str:
    return symbol.removeprefix('PSPRITE_').replace('_', ' ').title()


def _category(symbol: str) -> str:
    upgrades = ('BEAM', 'BOMB', 'MORPH_BALL', 'POWER_GRIP', 'SPACE_JUMP',
                'HIGH_JUMP', 'SCREW', 'VARIA', 'CHOZO_STATUE')
    systems = ('SAVE_', 'MAP_STATION', 'ELEVATOR', 'DOOR', 'BANNER', 'GUNSHIP')
    if any(token in symbol for token in upgrades):
        return 'Upgrade / ability'
    if 'DROP' in symbol or 'TANK' in symbol:
        return 'Item / pickup'
    if any(token in symbol for token in systems):
        return 'World object'
    if 'UNUSED' in symbol:
        return 'Unused native type'
    return 'Enemy / actor'


def build_mzm() -> list[dict]:
    enum_source = MZM_ENUM.read_text(encoding='utf-8')
    match = PRIMARY_RE.search(enum_source)
    if not match:
        raise ValueError('MZM PrimarySprite enum not found')
    symbols = re.findall(r'^\s*(PSPRITE_[A-Z0-9_]+)\s*,', match.group(1), re.M)
    if len(symbols) < 200 or 'PSPRITE_COUNT' not in match.group(1):
        raise ValueError('incomplete MZM PrimarySprite enum')
    stats_source = MZM_STATS.read_text(encoding='utf-8')
    stats = {}
    for symbol, body in STAT_BLOCK_RE.findall(stats_source):
        stats[symbol] = {key: value.rstrip(',').strip()
                         for key, value in STAT_RE.findall(body)}
    records = []
    for native_id, symbol in enumerate(symbols):
        values = stats.get(symbol, {})
        health = values.get('SPRITE_STATS_HEALTH', 'not decoded')
        damage = values.get('SPRITE_STATS_DAMAGE', 'not decoded')
        weakness = values.get('SPRITE_STATS_WEAKNESSES', 'not decoded')
        records.append({
            'world': 'Zero Mission', 'native_id': native_id,
            'native_type': symbol, 'name': _label(symbol),
            'category': _category(symbol),
            'summary': f'health={health}; damage={damage}; weaknesses={weakness}',
            'placements': None,
            'editable': False,
        })
    return records


def build_aria(catalog: dict) -> list[dict]:
    if catalog.get('format') != 'MV_AOS_WORLD_2':
        raise ValueError('invalid Aria native world catalog')
    grouped = {}
    for room in catalog['rooms']:
        for entity in room['entities']:
            key = (entity['kind'], entity['entity_id'])
            entry = grouped.setdefault(key, {'placements': 0, 'bosses': [], 'health': set()})
            entry['placements'] += 1
            if 'boss_id' in entity:
                entry['bosses'].append(entity['boss_id'])
            if 'native_health' in entity:
                entry['health'].add(entity['native_health'])
    records = []
    for (kind, entity_id), info in sorted(grouped.items()):
        bosses = ', '.join(sorted(set(info['bosses'])))
        health = ','.join(map(str, sorted(info['health']))) if info['health'] else 'not decoded'
        name = bosses if bosses else f'Entity kind {kind}, ID {entity_id}'
        category = 'Enemy / boss' if kind == 1 else f'Native kind {kind}'
        records.append({
            'world': 'Aria of Sorrow', 'native_id': entity_id,
            'native_type': f'kind-{kind}:id-{entity_id}', 'name': name,
            'category': category,
            'summary': f'health={health}; native placements={info["placements"]}',
            'placements': info['placements'],
            'editable': False,
        })
    return records


def load_aria() -> dict:
    path = OUTPUT / 'rooms/aria/world.json'
    if path.is_file():
        return json.loads(path.read_text(encoding='utf-8'))
    from scripts.import_aos_world import DEFAULT_ROM, decode_world
    return decode_world(DEFAULT_ROM.read_bytes())


def tsv(records: list[dict]) -> str:
    lines = []
    for record in records:
        fields = [record['world'], str(record['native_id']), record['native_type'],
                  record['name'], record['category'], record['summary'],
                  str(record['placements'] if record['placements'] is not None else '-'),
                  '1' if record['editable'] else '0']
        if any('\t' in field or '\n' in field or len(field) > 500 for field in fields):
            raise ValueError('unsafe object catalog field')
        lines.append('\t'.join(fields))
    return '\n'.join(lines) + ('\n' if lines else '')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--format', choices=('tsv', 'json'), default='tsv')
    args = parser.parse_args()
    try:
        records = build_mzm() + build_aria(load_aria())
    except (OSError, ValueError, KeyError, IndexError) as exc:
        parser.error(str(exc))
    print(tsv(records) if args.format == 'tsv' else json.dumps(records, indent=2), end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
