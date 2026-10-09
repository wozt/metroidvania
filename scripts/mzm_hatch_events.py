#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode native Zero Mission hatch lock conditions, without simulating a save.

The original tables index gHatchData[slot] (0..15), not the global door
numbers. Do not infer a slot from a door index or coordinates alone.
"""
from __future__ import annotations

from functools import lru_cache
import re
from pathlib import Path
from scripts.import_game_assets import ROOT

SOURCE = ROOT / 'third_party/mzm/src/data/hatch_data.c'
TABLE = re.compile(
    r'const struct HatchLockEvent sHatchLockEvents([A-Za-z]+)\[(\d+)\]\s*=\s*\{(.*?)\n\};',
    re.S,
)
ENTRY = re.compile(r'\{([^{}]*)\}', re.S)
FIELD = re.compile(r'\.([A-Za-z][A-Za-z0-9_]*)\s*=\s*([A-Za-z0-9_]+)')
TYPES = {
    'HATCH_LOCK_EVENT_TYPE_BEFORE': ('BEFORE', 'PERMANENT'),
    'HATCH_LOCK_EVENT_TYPE_AFTER': ('AFTER', 'PERMANENT'),
    'HATCH_LOCK_EVENT_TYPE_BEFORE_UNLOCKABLE': ('BEFORE', 'UNLOCKABLE'),
    'HATCH_LOCK_EVENT_TYPE_AFTER_UNLOCKABLE': ('AFTER', 'UNLOCKABLE'),
}


def parse_hatch_lock_events(source: str, area: str, room: int) -> dict[int, list[tuple[str, str, str]]]:
    """Map native hatch slots to (event enum, before/after, lock class)."""
    if not re.fullmatch(r'[A-Za-z]+', area) or not isinstance(room, int) or not 0 <= room < 256:
        raise ValueError('invalid native hatch room')
    tables = [match for match in TABLE.finditer(source) if match.group(1) == area]
    if not tables:
        return {}
    if len(tables) != 1:
        raise ValueError('duplicate native hatch event tables')
    count = int(tables[0].group(2))
    records = list(ENTRY.finditer(tables[0].group(3)))
    if len(records) != count or count > 256:
        raise ValueError('incomplete native hatch event table')
    result: dict[int, list[tuple[str, str, str]]] = {}
    for match in records:
        fields = dict(FIELD.findall(match.group(1)))
        if not fields.get('room', '').isdecimal():
            raise ValueError('invalid native hatch lock room')
        if int(fields['room']) != room:
            continue
        kind = TYPES.get(fields.get('type', ''))
        event = fields.get('event', '')
        if kind is None or not re.fullmatch(r'EVENT_[A-Z0-9_]+', event):
            raise ValueError('unsupported native hatch event condition')
        for slot in range(16):
            flag = fields.get(f'hatchesToLock_{slot}')
            if flag not in ('TRUE', 'FALSE'):
                raise ValueError(f'missing native hatch lock flag {slot}')
            if flag == 'TRUE':
                result.setdefault(slot, []).append((event, *kind))
    return result


@lru_cache(maxsize=128)
def load_hatch_lock_events(area: str, room: int) -> dict[int, list[tuple[str, str, str]]]:
    return parse_hatch_lock_events(SOURCE.read_text(encoding='utf-8'), area, room)


def preview_lock_class(rules: list[tuple[str, str, str]], events_on: bool) -> str | None:
    """Hypothetical all-events-on/off snapshot, never actual save progress."""
    active = [kind for _event, when, kind in rules
              if (events_on and when == 'AFTER') or
                 (not events_on and when == 'BEFORE')]
    if 'PERMANENT' in active:
        return 'PERMANENT'
    return 'UNLOCKABLE' if 'UNLOCKABLE' in active else None
