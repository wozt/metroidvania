#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a shared, read-only catalog of decoded native object definitions."""
from __future__ import annotations

import argparse
import json
import re
import struct

from scripts.import_game_assets import OUTPUT, ROOT, verified_rom
from scripts.import_aos_world import (
    DEFAULT_ROM as ARIA_ROM,
    ENEMY_TABLE_ENTRY_SIZE,
    ENEMY_TABLE_POINTER,
    EXPECTED_SHA1 as ARIA_SHA1,
    GBA_ROM_BASE,
)

MZM_ENUM = ROOT / 'third_party/mzm/include/constants/sprite.h'
MZM_STATS = ROOT / 'third_party/mzm/src/data/sprite_data.c'
PRIMARY_RE = re.compile(r'MAKE_ENUM\(u8, PrimarySprite\)\s*\{(.*?)\n\};', re.S)
STAT_BLOCK_RE = re.compile(r'\[(PSPRITE_[A-Z0-9_]+)\]\s*=\s*\{(.*?)\n\s*\},?', re.S)
STAT_RE = re.compile(r'^\s*\[(SPRITE_STATS_[A-Z0-9_]+)\]\s*=\s*(.+?)\s*$', re.M)
ARIA_CODE = ROOT / 'third_party/cvaos/asm/code'
ARIA_SYMBOL_RE = re.compile(
    r'^([A-Za-z_][A-Za-z0-9_]*):\s*@\s*0x([0-9A-Fa-f]+)', re.M)
ARIA_ENEMY_COUNT = 0x71

ARIA_KIND_NAMES = {
    0: 'Nothing',
    1: 'Enemy',
    2: 'Special object',
    3: 'Generic candle',
    4: 'Pickup',
    5: 'Hard-mode pickup',
    6: 'All-souls reward',
}

# These native IDs are behavioral constructors, not graphics IDs. Semantic
# roles were audited against LagoLunatic/DSVEdit's Aria entity documentation;
# names stay deliberately literal where the reverse-engineered role is uncertain.
ARIA_SPECIAL_OBJECT_NAMES = {
    0x00: 'Wooden door',
    0x01: 'Pushable crates',
    0x02: 'Boss door',
    0x03: 'Boss Rush boss door',
    0x04: 'Forced-entry boss door',
    0x05: 'Arena door',
    0x06: 'Darkness door',
    0x07: 'Room visual effect',
    0x08: 'Breakable wall',
    0x09: 'Event-destroyed wall',
    0x0A: 'Boss-death-conditional enemy',
    0x0B: 'Event-conditional enemy',
    0x0C: 'Waterfall current',
    0x0D: 'Bell',
    0x0E: 'Destructible prop',
    0x0F: 'Flame',
    0x10: 'Boss Rush reward',
    0x11: 'Boss Rush start',
    0x12: 'Background visual',
    0x13: 'Ball and chain',
    0x14: 'Boat',
    0x15: 'Chaotic Realm moon portal',
    0x16: 'Chaos portal',
    0x17: 'Elevator',
    0x18: 'Chest',
    0x19: 'Patrolling skull',
    0x1A: 'Unused special object 1A',
    0x1B: 'Conveyor reversal button',
    0x1C: 'Save point',
    0x1D: 'Save/warp room walls',
    0x1E: 'Unused special object 1E',
    0x1F: 'Warp point',
    0x20: 'Story event actor',
    0x21: 'Falling spike trap',
    0x22: 'Arena D-pad puzzle',
    0x23: 'Bathroom water flow',
    0x24: 'Bathroom mist',
    0x25: 'Legion background members',
    0x26: 'Arena background statue',
    0x27: 'Arena statue hand',
    0x28: 'Arena statue foot',
    0x29: 'Vertical moving platform',
    0x2A: 'Horizontal moving platform',
    0x2B: 'Floating Garden fountain',
    0x2C: 'Floating Garden clouds',
    0x2D: 'Throne stairway statue',
    0x2E: 'Skull-eye flames',
    0x2F: 'Julius fight object 2F',
    0x30: 'Julius fight object 30',
    0x31: 'Julius fight object 31',
    0x32: 'Crumbling platform',
    0x33: 'Waterfall splash',
    0x34: 'Swinging pendulum',
    0x35: 'Metal gate and button',
    0x36: 'Cog',
    0x37: 'Demon-head gate',
}

ARIA_PICKUP_NAMES = {
    0: 'Empty pickup',
    1: 'Money pickup',
    2: 'Consumable pickup',
    3: 'Weapon pickup',
    4: 'Armor/accessory pickup',
    5: 'Red soul candle',
    6: 'Blue soul candle',
    7: 'Yellow soul candle',
    8: 'Ability soul candle',
}


def _label(symbol: str) -> str:
    label = symbol.removeprefix('PSPRITE_').replace('_', ' ').title()
    label = re.sub(r'(?<=[A-Za-z])(?=\d)', ' ', label)
    return label.replace(' Ii', ' II').replace(' Iii', ' III')


def _camel_label(symbol: str) -> str:
    label = re.sub(r'(?<=[a-z0-9])(?=[A-Z])', ' ', symbol)
    return {'Belmont': 'Julius Belmont', 'Graham': 'Graham Jones'}.get(label, label)


def parse_aria_symbols(sources: list[str]) -> dict[int, str]:
    """Index named cvaos functions by their original ROM address."""
    symbols = {}
    for source in sources:
        for name, address in ARIA_SYMBOL_RE.findall(source):
            pointer = int(address, 16)
            previous = symbols.setdefault(pointer, name)
            if previous != name:
                raise ValueError(f'duplicate Aria symbol address 0x{pointer:08x}')
    return symbols


def decode_aria_enemy_names(rom: bytes, symbols: dict[int, str]) -> dict[int, str]:
    """Resolve enemy IDs through the ROM constructor table and named symbols."""
    offset = ENEMY_TABLE_POINTER - GBA_ROM_BASE
    size = ARIA_ENEMY_COUNT * ENEMY_TABLE_ENTRY_SIZE
    if offset < 0 or offset + size > len(rom):
        raise ValueError('Aria enemy table is outside the ROM')
    names = {}
    for enemy_id in range(ARIA_ENEMY_COUNT):
        constructor = struct.unpack_from(
            '<I', rom, offset + enemy_id * ENEMY_TABLE_ENTRY_SIZE)[0] & ~1
        symbol = symbols.get(constructor)
        match = re.fullmatch(r'Enemy([A-Za-z0-9]+)Create', symbol or '')
        if not match:
            raise ValueError(
                f'unnamed Aria enemy constructor 0x{constructor:08x} for ID 0x{enemy_id:02x}')
        names[enemy_id] = _camel_label(match.group(1))
    return names


def load_aria_enemy_names() -> dict[int, str]:
    """Load enemy identities from the verified ROM and pinned disassembly."""
    rom = verified_rom(ARIA_ROM, ARIA_SHA1)
    if not ARIA_CODE.is_dir():
        raise OSError(f'missing pinned Aria code directory: {ARIA_CODE}')
    sources = [path.read_text(encoding='utf-8')
               for path in sorted(ARIA_CODE.glob('*.s'))]
    if not sources:
        raise OSError(f'empty pinned Aria code directory: {ARIA_CODE}')
    return decode_aria_enemy_names(rom, parse_aria_symbols(sources))


def aria_entity_identity(kind: int, entity_id: int,
                         enemy_names: dict[int, str] | None = None) -> tuple[str, str]:
    """Return a truthful display name and category for one native entity type."""
    if kind == 1:
        name = (enemy_names or {}).get(entity_id, f'Unknown enemy 0x{entity_id:02X}')
        return name, 'Boss' if entity_id >= 0x6A else 'Enemy'
    if kind == 2:
        name = ARIA_SPECIAL_OBJECT_NAMES.get(
            entity_id, f'Unknown special object 0x{entity_id:02X}')
        return name, 'World object / event'
    if kind == 3:
        return 'Generic candle', 'Candle / breakable'
    pickup = ARIA_PICKUP_NAMES.get(entity_id, f'Unknown pickup 0x{entity_id:02X}')
    if kind == 4:
        return pickup, 'Item / soul pickup'
    if kind == 5:
        return f'Hard-mode {pickup.lower()}', 'Conditional pickup'
    if kind == 6:
        return f'All-souls reward: {pickup.lower()}', 'Conditional pickup'
    return (f'Unknown {ARIA_KIND_NAMES.get(kind, "entity kind")} 0x{entity_id:02X}',
            'Unknown native type')


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


def build_aria(catalog: dict, enemy_names: dict[int, str] | None = None) -> list[dict]:
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
        health = ','.join(map(str, sorted(info['health']))) if info['health'] else 'not decoded'
        name, category = aria_entity_identity(kind, entity_id, enemy_names)
        kind_name = ARIA_KIND_NAMES.get(kind, f'Unknown kind {kind}')
        boss_refs = ','.join(sorted(set(info['bosses']))) or '-'
        records.append({
            'world': 'Aria of Sorrow', 'native_id': entity_id,
            'native_type': f'{kind_name.lower().replace(" ", "-")}:{entity_id:02X}',
            'name': name,
            'category': category,
            'summary': (f'kind={kind} (0x{kind:02X}); id={entity_id} (0x{entity_id:02X}); '
                        f'health={health}; native placements={info["placements"]}; '
                        f'boss references={boss_refs}'),
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
        records = build_mzm() + build_aria(load_aria(), load_aria_enemy_names())
        records.sort(key=lambda record: (
            record['world'], record['category'] == 'Unused native type',
            record['category'], record['name'], record['native_id']))
    except (OSError, ValueError, KeyError, IndexError) as exc:
        parser.error(str(exc))
    print(tsv(records) if args.format == 'tsv' else json.dumps(records, indent=2), end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
