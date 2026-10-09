#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Native ROM-derived Clipdata and annotation overlay for the SDL3 diagnostics.

The file is a private, read-only source sidecar. It is NOT a project export and
is not ingested as authorable collision or project entities. Nonzero Clipdata
IDs retain their original numeric value; no gameplay semantics are inferred.
"""
from __future__ import annotations

import os
from pathlib import Path
import tempfile
from scripts import mzm_room_render as native
from scripts import room_annotations

MAX_RECORDS = 32768
ANNOTATION_CODES = {
    'ENEMY': 1, 'ITEM': 2, 'OBJECT': 3, 'DOOR': 4,
    'EVENT': 5, 'TRIGGER': 6, 'OTHER': 7, 'ENTITY': 7,
}


def encode_overlay(area: str, room: int, width: int, height: int,
                   clip: tuple[int, int, tuple[int, ...]],
                   annotations: list[tuple]) -> tuple[bytes, dict]:
    """Create a strict, bounded viewer-only overlay from decoded native records."""
    if (area not in ('Brinstar', 'Kraid', 'Norfair', 'Ridley', 'Tourian',
                     'Crateria', 'Chozodia') or not 0 <= room <= 999 or
            width < 16 or height < 16 or width % 16 or height % 16 or
            width * height > 6144 * 256):
        raise ValueError('invalid native overlay scope')
    cw, ch, cells = clip
    if not (1 <= cw <= 255 and 1 <= ch <= 255 and
            len(cells) == cw * ch and cw * 16 == width and ch * 16 == height):
        raise ValueError('native Clipdata geometry differs from displayed BG1; overlay refused')
    lines = [f'MVROOM-SOURCE\t1\tmzm\t{area}\t{room}\t{width}\t{height}']
    counts = {'collision': 0, 'markers': 0, 'skipped_markers': 0}
    for y in range(ch):
        for x in range(cw):
            raw = cells[y * cw + x]
            if type(raw) is not int or not 0 <= raw <= 65535:
                raise ValueError('invalid original Clipdata value')
            if raw:
                lines.append(f'N\t{x*16}\t{y*16}\t16\t16\t{raw}')
                counts['collision'] += 1
    for record in annotations:
        if len(record) != 10 or record[0] not in ANNOTATION_CODES:
            raise ValueError('invalid original annotation record')
        x, y, w, h = record[2:6]
        if any(type(n) is not int for n in (x, y, w, h)):
            raise ValueError('invalid original annotation geometry')
        if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > width or y + h > height:
            counts['skipped_markers'] += 1
            continue  # Original metadata can refer to off-screen entities.
        lines.append(f'A\t{x}\t{y}\t{w}\t{h}\t{ANNOTATION_CODES[record[0]]}')
        counts['markers'] += 1
    if len(lines) - 1 > MAX_RECORDS:
        raise ValueError('native overlay record limit exceeded')
    lines.append('END')
    return ('\n'.join(lines) + '\n').encode('ascii'), counts


def write_overlay(root: Path, area: str, room: int, width: int, height: int) -> tuple[Path, dict]:
    """Use only ROM-derived metadata from the installed pinned source/extraction."""
    original = native.read_source_room(area, room)
    clip = native.rle_room(native.room_blob(original['fields']['pClipData']))
    annotations = room_annotations.build_mzm(area, room)
    blob, counts = encode_overlay(area, room, width, height, clip, annotations)
    folder = root / 'assets/extracted/native_source_overlays/mzm'
    current = root
    for part in folder.relative_to(root).parts:
        current = current / part
        if current.is_symlink() or (current.exists() and not current.is_dir()):
            raise ValueError('unsafe native source overlay directory')
    folder.mkdir(parents=True, exist_ok=True)
    target = folder / f'{area.lower()}_{room:03}.tsv'
    if target.is_symlink() or (target.exists() and not target.is_file()):
        raise ValueError('unsafe native source overlay target')
    temp = None
    try:
        with tempfile.NamedTemporaryFile(dir=folder, prefix='.native-overlay-',
                                         delete=False) as out:
            temp = Path(out.name)
            out.write(blob)
            out.flush()
            os.fsync(out.fileno())
        if target.is_symlink():
            raise ValueError('native overlay destination symlink refused')
        os.replace(temp, target)
    finally:
        if temp is not None and temp.exists():
            temp.unlink()
    return target, counts
