#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build verified case-grid overview and bounded native preview batches.

The MZM positions are original minimap ANCHORS (not room extents).
The Aria positions are native 64x35 minimap CELLS. No synthetic geometry.
ROM-derived pixels are generated privately, and never staged for Git.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path
from scripts.import_game_assets import OUTPUT, ROOT, write_generated
from scripts.import_mzm_rooms import ROOM_SOURCE, decode_room_descriptors
from scripts.mzm_world_atlas import decode_door_tables

MZM_AREAS = ('Brinstar','Kraid','Norfair','Ridley','Tourian','Crateria','Chozodia')
MZM_ROOMS_ROOT = ROOT / 'third_party/mzm/src/data/rooms'
MZM_SCROLL_RE = re.compile(
    r'const u8 s([A-Za-z]+)_\d+_Scrolls\[SCROLL_DATA_SIZE\((\d+)\)\]\s*=\s*'
    r'\{(.*?)\n\};', re.S)


def build_mzm(tsv: str) -> list[tuple[int,int,int,int,int,int]]:
    out=[]
    for line in tsv.splitlines():
        if not line or line.startswith('#'): continue
        cols=line.split('|')
        if cols[0]!='R': continue
        if len(cols)!=8 or cols[1] not in MZM_AREAS: raise ValueError('bad MZM room row')
        area=MZM_AREAS.index(cols[1])
        room,x,y=int(cols[2]),int(cols[3]),int(cols[4])
        save=int(cols[7])
        if not(0<=room<256 and 0<=x<128 and 0<=y<128 and save in (0,1)):
            raise ValueError('invalid MZM room coordinate')
        out.append((area,room,x,y,save,0))
    if not out: raise ValueError('empty MZM room atlas')
    return out


def build_aria(catalog: dict) -> list[tuple[int,int,int,int,int,int]]:
    """Use verified Aria minimap cells from the imported native room catalog.

    MV_AOS_WORLD_2 stores cells under rooms[*].map_cells. Also accept a
    top-level map_cells array for compatible catalogs, without synthesizing
    room footprints, coordinates, or unverified connections.
    """
    if not isinstance(catalog, dict) or catalog.get('format') != 'MV_AOS_WORLD_2':
        raise ValueError('wrong Aria world format')
    cells = catalog.get('map_cells')
    if cells is None:
        rooms = catalog.get('rooms')
        if not isinstance(rooms, list):
            raise ValueError('Aria catalog has no native room map cells')
        cells = []
        for room in rooms:
            if not isinstance(room, dict) or not isinstance(room.get('map_cells'), list):
                raise ValueError('invalid Aria room map_cells')
            for cell in room['map_cells']:
                if not isinstance(cell, dict):
                    raise ValueError('invalid Aria native map cell')
                cells.append({
                    'engine_area': room.get('engine_area'),
                    'room': room.get('room'),
                    **cell,
                })
    if not isinstance(cells, list):
        raise ValueError('Aria map_cells must be a list')
    out = []
    occupied = set()
    for cell in cells:
        if not isinstance(cell, dict):
            raise ValueError('invalid Aria native map cell')
        keys = ('engine_area', 'room', 'map_x', 'map_y')
        if any(type(cell.get(key)) is not int for key in keys):
            raise ValueError('Aria native map coordinates must be integers')
        area, room, x, y = (cell[key] for key in keys)
        if not (0 <= area < 12 and 0 <= room < 1000 and
                0 <= x < 64 and 0 <= y < 35):
            raise ValueError('invalid Aria original minimap cell')
        if type(cell.get('save')) is not bool or type(cell.get('warp')) is not bool:
            raise ValueError('Aria native save/warp flags must be booleans')
        if (x, y) in occupied:
            raise ValueError('duplicate Aria original minimap coordinate')
        occupied.add((x, y))
        out.append((area, room, x, y, int(cell['save']), int(cell['warp'])))
    if not out:
        raise ValueError('empty Aria map')
    count = catalog.get('mapped_cells')
    if count is not None and (type(count) is not int or count != len(out)):
        raise ValueError('Aria mapped_cells count differs from verified native cells')
    return out


def native_mzm_clip_dimensions() -> dict[tuple[int, int], tuple[int, int]]:
    """Only use decoded, project-private native clipdata; no guessed footprints.

    The original minimap derives tile coordinates from local room coordinates
    plus RoomEntryRom.mapX/mapY (pinned metroidret/mzm src/minimap.c).
    One minimap unit is 15x10 blocks. Clipdata RLE gives room bounds in
    16-pixel blocks; background images are NOT a substitute for collision bounds.
    """
    from scripts.mzm_room_render import room_blob, rle_room
    catalog = OUTPUT / 'rooms/metroid/rooms.tsv'
    if not catalog.is_file():
        return {}
    found = {}
    for line in catalog.read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):
            continue
        cols = line.split('|')
        if len(cols) != 10:
            raise ValueError('malformed verified MZM descriptor catalog')
        # The shared descriptor catalog also contains developer test rooms.
        # They have no production-area minimap and are intentionally ignored.
        if cols[0] not in MZM_AREAS:
            continue
        area = MZM_AREAS.index(cols[0])
        try:
            room = int(cols[1])
        except ValueError as exc:
            raise ValueError('invalid MZM room ID in descriptor catalog') from exc
        if not 0 <= room < 256:
            raise ValueError('MZM descriptor room index out of bounds')
        try:
            width, height, _ = rle_room(room_blob(cols[6]))
        except (ValueError, OSError, IndexError):
            # Unavailable or not yet decoded original clipdata -> anchor only.
            continue
        found[(area, room)] = (width, height)
    return found


def parse_mzm_scroll_regions(source: str, expected_area: str) -> dict[int, list[tuple[int, ...]]]:
    """Decode room scroll rectangles from one or more pinned room sources."""
    found = {}
    for match in MZM_SCROLL_RE.finditer(source):
        area, declared, body = match.groups()
        if area != expected_area:
            raise ValueError(f'unexpected MZM scroll area {area}, wanted {expected_area}')
        clean = re.sub(r'//[^\n]*', '', body)
        tokens = re.findall(r'UCHAR_MAX|\b\d+\b', clean)
        values = [255 if token == 'UCHAR_MAX' else int(token) for token in tokens]
        count = int(declared)
        if len(values) != 2 + count * 8 or values[1] != count:
            raise ValueError(f'incomplete {area} scroll data')
        room = values[0]
        if not 0 <= room < 256 or room in found:
            raise ValueError(f'invalid or duplicate {area} scroll room {room}')
        regions = []
        for offset in range(2, len(values), 8):
            region = tuple(values[offset:offset + 8])
            x_start, x_end, y_start, y_end, _, _, direction, extension = region
            if (x_start > x_end or y_start > y_end or
                    direction not in (0, 1, 2, 3, 255) or
                    any(not 0 <= value <= 255 for value in region)):
                raise ValueError(f'invalid {area} scroll bounds for room {room}')
            if direction == 255 and extension != 255:
                raise ValueError(f'orphan {area} scroll extension for room {room}')
            regions.append(region)
        found[room] = regions
    return found


def native_mzm_scroll_regions() -> dict[tuple[int, int], list[tuple[int, ...]]]:
    """Load every custom room scroll from the pinned Zero Mission source."""
    found = {}
    for area_index, area in enumerate(MZM_AREAS):
        directory = MZM_ROOMS_ROOT / area.lower()
        if not directory.is_dir():
            continue
        source = '\n'.join(path.read_text(encoding='utf-8')
                           for path in sorted(directory.glob('*.c')))
        for room, regions in parse_mzm_scroll_regions(source, area).items():
            found[(area_index, room)] = regions
    return found


def _scroll_local_cells(regions: list[tuple[int, ...]]) -> set[tuple[int, int]]:
    """Project native block-space scroll bounds through the minimap formula."""
    cells = set()
    for region in regions:
        bounds = list(region[:4])
        variants = [bounds]
        direction, extension = region[6], region[7]
        if direction in (0, 1, 2, 3) and extension != 255:
            extended = bounds.copy()
            extended[direction] = extension
            variants.append(extended)
        for x_start, x_end, y_start, y_end in variants:
            if x_start > x_end or y_start > y_end:
                continue
            for block_y in range(y_start, y_end + 1):
                for block_x in range(x_start, x_end + 1):
                    cells.add((max(0, block_x - 2) // 15,
                               max(0, block_y - 2) // 10))
    return cells


def native_mzm_room_evidence(
        anchors: list[tuple[int, int, int, int, int, int]]) -> dict[tuple[int, int], set[tuple[int, int, int]]]:
    """Project native doors and sprite placements into per-room map evidence."""
    from scripts.room_annotations import PLACEMENT_ITEM_RE, PLACEMENT_RE

    rooms = decode_room_descriptors(ROOM_SOURCE.read_text(encoding='utf-8'))
    tables = decode_door_tables(ROOM_SOURCE.read_text(encoding='utf-8'), rooms)
    origins = {(area, room): (x, y) for area, room, x, y, *_ in anchors}
    evidence = {}
    for area_index, area in enumerate(MZM_AREAS):
        for door in tables[area]:
            if "DOOR_TYPE_NONE" in door["type"]:
                continue
            room = door['sourceRoom']
            origin = origins.get((area_index, room))
            if origin is None:
                continue
            x, y = origin
            # All blocks touched by the door are native evidence for its source
            # room. Set projection avoids favoring an arbitrary door edge.
            points = evidence.setdefault((area_index, room), set())
            for block_y in range(door['yStart'], door['yEnd'] + 1):
                for block_x in range(door['xStart'], door['xEnd'] + 1):
                    points.add((area_index,
                                x + max(0, block_x - 2) // 15,
                                y + max(0, block_y - 2) // 10))
        directory = MZM_ROOMS_ROOT / area.lower()
        for path in sorted(directory.glob('*.c')):
            source = path.read_text(encoding='utf-8')
            for match in PLACEMENT_RE.finditer(source):
                source_area, room_text, _, body = match.groups()
                room = int(room_text)
                origin = origins.get((area_index, room))
                if source_area != area or origin is None:
                    continue
                x, y = origin
                points = evidence.setdefault((area_index, room), set())
                for block_y, block_x, _ in PLACEMENT_ITEM_RE.findall(body):
                    points.add((area_index,
                                x + max(0, int(block_x) - 2) // 15,
                                y + max(0, int(block_y) - 2) // 10))
    return evidence


def expand_mzm_clip_cells(anchors: list[tuple[int, int, int, int, int, int]],
                          dimensions: dict[tuple[int, int], tuple[int, int]]) -> list[tuple[int, ...]]:
    """Expand engine room bounds when no native minimap is available.

    MZM keeps a two-block Clipdata guard border on every side. The playable
    dimensions are therefore ``(width - 4) / 15`` by ``(height - 4) / 10``
    screens. This fallback remains deliberately conservative: overlaps or
    dimensions that do not exactly match the engine formula stay anchors.
    """
    if not anchors:
        raise ValueError('empty MZM room atlas')
    anchor_coords = {}
    for area, room, x, y, *_ in anchors:
        anchor_coords.setdefault((area, x, y), set()).add((area, room))
    candidates = {}
    for area, room, x, y, save, warp in anchors:
        blocks = dimensions.get((area, room))
        if blocks is None:
            continue
        bw, bh = blocks
        if (type(bw) is not int or type(bh) is not int or
                not 1 <= bw <= 128 or not 1 <= bh <= 128):
            continue
        if bw < 19 or bh < 14 or (bw - 4) % 15 or (bh - 4) % 10:
            continue
        width, height = (bw - 4) // 15, (bh - 4) // 10
        if x + width > 32 or y + height > 32:
            continue
        points = {(area, xx, yy)
                  for yy in range(y, y + height)
                  for xx in range(x, x + width)}
        if any(other != {(area, room)} for point in points
               if (other := anchor_coords.get(point)) is not None):
            continue
        candidates[(area, room)] = points
    owners = {}
    for owner, points in candidates.items():
        for point in points:
            owners.setdefault(point, set()).add(owner)
    conflicting = {owner for group in owners.values() if len(group) > 1
                   for owner in group}
    out = []
    for area, room, x, y, save, warp in anchors:
        owner = (area, room)
        if owner in candidates and owner not in conflicting:
            for _, xx, yy in sorted(candidates[owner], key=lambda t: (t[2], t[1])):
                out.append((area, room, xx, yy, save, warp, 1))
        else:
            out.append((area, room, x, y, save, warp, 0))
    return out


def resolve_mzm_minimap_cells(
        anchors: list[tuple[int, int, int, int, int, int]],
        dimensions: dict[tuple[int, int], tuple[int, int]],
        native: list[tuple[int, ...]],
        scroll_regions: dict[tuple[int, int], list[tuple[int, ...]]] | None = None,
        direct_evidence: dict[tuple[int, int], set[tuple[int, int, int]]] | None = None,
        ) -> tuple[list[tuple], dict]:
    """Join engine geometry to original minimap occupancy without guessing.

    Rooms sharing a RoomEntryRom map origin are recorded as progression
    variants of one family. A native cell is assigned only when one family
    claims it, or when its exact room origin disambiguates overlapping engine
    bounds. Every other native cell remains explicitly unowned.
    """
    if not anchors or not native:
        raise ValueError('MZM ownership resolution needs anchors and native cells')
    native_by_position = {(row[0], row[2], row[3]): row for row in native}
    if len(native_by_position) != len(native):
        raise ValueError('duplicate native MZM minimap coordinate')

    scroll_regions = scroll_regions or {}
    direct_evidence = direct_evidence or {}
    families = {}
    scroll_bounded_rooms = 0
    for area, room, x, y, save, warp in anchors:
        family = families.setdefault((area, x, y), {
            'area': area, 'x': x, 'y': y, 'rooms': [], 'points': set(),
            'save': False, 'warp': False,
        })
        family['rooms'].append(room)
        family['save'] |= bool(save)
        family['warp'] |= bool(warp)
        regions = scroll_regions.get((area, room))
        if regions is not None:
            local_cells = _scroll_local_cells(regions)
            scroll_bounded_rooms += 1
        else:
            bounds = dimensions.get((area, room))
            if bounds is None:
                continue
            width, height = bounds
            if (type(width) is not int or type(height) is not int or
                    width < 19 or height < 14 or
                    (width - 4) % 15 or (height - 4) % 10):
                continue
            screens_x, screens_y = (width - 4) // 15, (height - 4) // 10
            if not 1 <= screens_x <= 32 or not 1 <= screens_y <= 32:
                continue
            local_cells = {(xx, yy) for yy in range(screens_y)
                           for xx in range(screens_x)}
        for local_x, local_y in local_cells:
            point = (area, x + local_x, y + local_y)
            if point in native_by_position:
                family['points'].add(point)

    claims = {}
    for key, family in families.items():
        for point in family['points']:
            claims.setdefault(point, []).append(key)

    rows = []
    ambiguity_records = []
    owned_count = 0
    evidence_resolutions = 0
    for point, source in sorted(native_by_position.items()):
        area, _, x, y, _, _, _, tile = source
        candidates = claims.get(point, [])
        anchored = [point] if point in families else []
        owner = None
        if len(candidates) == 1:
            owner = candidates[0]
        elif len(anchored) == 1:
            # RoomEntryRom.mapX/mapY is direct evidence for the origin cell.
            owner = anchored[0]
        elif len(candidates) > 1:
            evidenced = [key for key in candidates
                         if any(point in direct_evidence.get((key[0], room), set())
                                for room in families[key]['rooms'])]
            if len(evidenced) == 1:
                owner = evidenced[0]
                evidence_resolutions += 1
        if owner is None and not candidates:
            witnessed = [key for key, family in families.items()
                         if any(point in direct_evidence.get((key[0], room), set())
                                for room in family['rooms'])]
            if len(witnessed) == 1:
                owner = witnessed[0]
                evidence_resolutions += 1
        if owner is not None:
            family = families[owner]
            rooms = sorted(set(family['rooms']))
            primary = rooms[0]
            provenance = 1 if (point in family['points'] or
                               any(point in direct_evidence.get((area, r), set())
                                   for r in rooms)) else 0
            note = ','.join(map(str, rooms)) if len(rooms) > 1 else '-'
            rows.append((area, primary, x, y, int(family['save']),
                         int(family['warp']), provenance, tile, note))
            owned_count += 1
        else:
            room_ids = sorted({room for key in candidates
                               for room in families[key]['rooms']})
            note = 'ambiguous:' + ','.join(map(str, room_ids)) if room_ids else 'unassigned'
            rows.append((area, 999, x, y, 0, 0, 3, tile, note))
            if room_ids:
                ambiguity_records.append({
                    'area': area, 'x': x, 'y': y,
                    'candidate_rooms': room_ids,
                    'candidate_origins': [[key[1], key[2]] for key in candidates],
                })

    # Some RoomEntry origins are intentionally absent from the pause minimap.
    # Preserve them as navigable anchor-only records, grouped by variant family.
    for key, family in sorted(families.items()):
        if key in native_by_position:
            continue
        rooms = sorted(set(family['rooms']))
        note = ','.join(map(str, rooms)) if len(rooms) > 1 else '-'
        rows.append((family['area'], rooms[0], family['x'], family['y'],
                     int(family['save']), int(family['warp']), 0, 0, note))

    report = {
        'format': 'MV_MZM_MINIMAP_OWNERSHIP_1',
        'native_cells': len(native),
        'owned_native_cells': owned_count,
        'unassigned_native_cells': len(native) - owned_count,
        'ambiguous_native_cells': len(ambiguity_records),
        'scroll_bounded_rooms': scroll_bounded_rooms,
        'native_evidence_cells': len({point for points in direct_evidence.values()
                                      for point in points}),
        'native_evidence_resolutions': evidence_resolutions,
        'variant_families': [
            {'area': family['area'], 'map_x': family['x'], 'map_y': family['y'],
             'rooms': sorted(set(family['rooms']))}
            for family in families.values() if len(set(family['rooms'])) > 1
        ],
        'ambiguities': ambiguity_records,
        'method': ('RoomEntryRom origins plus native room scroll bounds, with exact '
                   'Clipdata dimensions as fallback, door/sprite disambiguation, '
                   'and original pause-minimap occupancy'),
    }
    return rows, report


def native_mzm_minimap_cells() -> list[tuple[int, ...]]:
    """Decode *original* 32x32 MZM pause-screen minimaps without guessing owners.

    Entries are available from the private pinned database raw extraction, or
    directly from a locally owned, hash-verified US ROM using database offsets.
    Native tile ownership by individual room is unknown and stays unassigned.
    """
    import struct
    from scripts.mzm_room_render import lz77
    from scripts.import_game_assets import ROMS, ROOT, verified_rom

    raw_root = OUTPUT / 'raw/metroid/data/menus/pause_screen'
    missing = [slug for slug in MZM_AREAS
               if not (raw_root / (slug.lower() + '_minimap.tt')).is_file()]
    rom = None
    offsets = {}
    if missing:
        source = ROOT / 'roms' / ROMS['metroid'][0]
        database = ROOT / 'third_party/mzm/database.json'
        if source.is_file() and database.is_file():
            rom = verified_rom(source, ROMS['metroid'][1])
            records = json.loads(database.read_text(encoding='utf-8'))
            for entry in records:
                path = entry.get('path', '')
                if path.startswith('menus/pause_screen/') and path.endswith('_minimap.tt'):
                    offsets[path.rsplit('/', 1)[-1]] = entry

    cells = []
    for area, name in enumerate(MZM_AREAS):
        filename = name.lower() + '_minimap.tt'
        path = raw_root / filename
        if path.is_symlink():
            raise ValueError('symlinked private MZM minimap source refused')
        if path.is_file():
            if path.stat().st_size > 16384:
                raise ValueError('oversized private MZM minimap source')
            payload = path.read_bytes()
        elif rom is not None and filename in offsets:
            item = offsets[filename]
            offset = int(item['addr']['us'], 16)
            length = int(item['count'], 16) * int(item['size'])
            if not 0 < length <= 16384 or offset < 0 or offset + length > len(rom):
                raise ValueError('invalid verified MZM minimap database offset')
            payload = rom[offset:offset + length]
        else:
            continue
        decoded = lz77(payload, limit=2048)
        if len(decoded) != 32 * 32 * 2:
            raise ValueError(f'invalid native MZM minimap size for {name}')
        for y in range(32):
            for x in range(32):
                tile = struct.unpack_from('<H', decoded, (y * 32 + x) * 2)[0]
                if (tile & 0x03FF) != 0x140:
                    # 999 = unassigned native map tile, 3 = native source.
                    cells.append((area, 999, x, y, 0, 0, 3, tile))
    return cells


def mzm_global_doors(anchors: list[tuple[int, int, int, int, int, int]]) -> bytes:
    """Project native door geometry into original 32x32 minimap coordinates.

    Entries keep native room/index/type. This is NOT a claim that the minimap
    actually draws door sprites; it is a separate editor diagnostic layer.
    """
    rooms = decode_room_descriptors(ROOM_SOURCE.read_text(encoding='utf-8'))
    tables = decode_door_tables(ROOM_SOURCE.read_text(encoding='utf-8'), rooms)
    origins = {(a, r): (x, y) for a, r, x, y, *_ in anchors}
    lines = ['# area|room|door_index|map_x|map_y|native_type']
    for area, name in enumerate(MZM_AREAS):
        for door in tables[name]:
            if 'DOOR_TYPE_NONE' in door['type']:
                continue
            origin = origins.get((area, door['sourceRoom']))
            if origin is None:
                continue
            x, y = origin
            x += max(0, door['xStart'] - 2) // 15
            y += max(0, door['yStart'] - 2) // 10
            if 0 <= x < 32 and 0 <= y < 32:
                lines.append(f"{area}|{door['sourceRoom']}|{door['index']}|"
                             f"{x}|{y}|{door['type']}")
    return ('\n'.join(lines) + '\n').encode('utf-8')


def mzm_screen_preview(bmp: bytes, local_x: int, local_y: int) -> bytes | None:
    """Crop one *playable* 240x160 screen, omitting the native 32px guard.

    Downsample nearest-neighbour to 60x40 for a fast, proportion-correct GTK
    thumbnail. Return None for incompatible room BG1 geometry, never stretch an
    unrelated full-room preview across a minimap cell.
    """
    import struct
    from scripts.mzm_room_render import bmp24
    if not (0 <= local_x < 32 and 0 <= local_y < 32) or len(bmp) < 54:
        return None
    if bmp[:2] != b'BM' or struct.unpack_from('<I', bmp, 10)[0] != 54:
        return None
    width, height = struct.unpack_from('<ii', bmp, 18)
    planes, bits, compression = struct.unpack_from('<HHI', bmp, 26)
    if (not 0 < width <= 4096 or not 0 < height <= 4096 or
        planes != 1 or bits != 24 or compression != 0 or
        width < 304 or height < 224 or
        (width - 64) % 240 or (height - 64) % 160):
        return None
    pitch = (width * 3 + 3) & ~3
    if len(bmp) != 54 + pitch * height:
        return None
    if (local_x + 1) * 240 + 64 > width or (local_y + 1) * 160 + 64 > height:
        return None
    result = bytearray(60 * 40 * 3)
    for py in range(40):
        src_y = 32 + local_y * 160 + py * 4 + 2
        for px in range(60):
            src_x = 32 + local_x * 240 + px * 4 + 2
            pos = 54 + (height - 1 - src_y) * pitch + src_x * 3
            dest = (py * 60 + px) * 3
            result[dest:dest + 3] = bmp[pos:pos + 3][::-1]
    return bmp24(60, 40, result)


def generate_mzm_case_previews(area: int, rows: list[tuple]) -> int:
    """Refresh tiny private per-screen previews for verified owned cells."""
    room_sources = decode_room_descriptors(ROOM_SOURCE.read_text(encoding='utf-8'))
    anchors = {(MZM_AREAS.index(room['area']), room['index']):
               (int(room['fields']['mapX']), int(room['fields']['mapY']))
               for room in room_sources if room['area'] in MZM_AREAS and
               room['fields']['mapX'].isdecimal() and room['fields']['mapY'].isdecimal()}
    by_room = {}
    for row in rows:
        a, room, x, y = row[:4]
        provenance = row[6] if len(row) >= 7 else 0
        if a == area and room != 999 and provenance == 1 and (a, room) in anchors:
            by_room.setdefault(room, []).append((x, y))
    created = 0
    for room, cells in by_room.items():
        preview = OUTPUT / f'rooms/metroid/previews/{MZM_AREAS[area].lower()}_{room:03}_bg1.bmp'
        if not preview.is_file() or preview.is_symlink() or preview.stat().st_size > 64_000_000:
            continue
        source = preview.read_bytes()
        ox, oy = anchors[(area, room)]
        for x, y in cells:
            result = mzm_screen_preview(source, x - ox, y - oy)
            if result is None:
                continue
            rel = f'world_overview/mzm_cells/area_{area:02}_room_{room:03}_x_{x:02}_y_{y:02}.bmp'
            write_generated(rel, result)
            created += 1
    return created


def output_rows(world: str, rows: list[tuple]) -> bytes:
    # 8th field: native minimap tile code (0 when unknown). Provenance:
    # 0=original MZM anchor, 1=decoded MZM room geometry, 2=original Aria cell,
    # 3=MZM original pause-screen map tile with unknown room ownership.
    default = 2 if world == 'aria' else 0
    encoded = []
    for row in rows:
        if len(row) == 6:
            row = (*row, default)
        if (len(row) not in (7, 8, 9) or row[6] not in (0, 1, 2, 3)
                or (len(row) == 7 and row[6] == 3)
                or (len(row) >= 8 and (type(row[7]) is not int
                    or not 0 <= row[7] <= 65535))
                or (len(row) == 9 and (not isinstance(row[8], str)
                    or '|' in row[8] or '\n' in row[8] or len(row[8]) > 180))):
            raise ValueError('invalid world overview provenance/tile row')
        encoded.append('|'.join(map(str, row)))
    head = ('# Original map cells; provenance 0=anchor, 1=MZM geometry, 2=Aria, 3=MZM minimap tile.\n'
            '# area|room|x|y|save|warp|provenance|native_tile|variants_or_ambiguity\n')
    return (head + '\n'.join(encoded) + '\n').encode('utf-8')


def index(world: str)->int:
    if world=='mzm':
        from scripts.mzm_world_atlas import run
        src=OUTPUT/'rooms/metroid/world_atlas.tsv'
        if not src.is_file(): run()
        anchors = build_mzm(src.read_text(encoding='utf-8'))
        write_generated('world_overview/mzm_doors.tsv', mzm_global_doors(anchors))
        native = native_mzm_minimap_cells()
        if native:
            rows, report = resolve_mzm_minimap_cells(
                anchors, native_mzm_clip_dimensions(), native,
                native_mzm_scroll_regions(), native_mzm_room_evidence(anchors))
            write_generated('world_overview/mzm_ownership.json',
                            (json.dumps(report, indent=2) + '\n').encode('utf-8'))
        else:
            # Lack of verified private minimap inputs cannot justify drawing
            # a faux complete world from imperfect clipdata bounding boxes.
            rows = [(*anchor, 0, 0) for anchor in anchors]
    else:
        from scripts.import_aos_world import decode_world, DEFAULT_ROM
        p=OUTPUT/'rooms/aria/world.json'
        if p.is_file(): cat=json.loads(p.read_text(encoding='utf-8'))
        else: cat=decode_world(DEFAULT_ROM.read_bytes())
        rows=build_aria(cat)
    write_generated(f'world_overview/{world}.tsv',output_rows(world,rows))
    return len(rows)


def generate_previews(world:str, area:int, budget:int)->tuple[int,int]:
    path=OUTPUT/f'world_overview/{world}.tsv'
    rows=[tuple(map(int,line.split('|')[:8])) for line in path.read_text().splitlines()
          if line and not line.startswith('#')]
    done=0;failed=0
    # Old unsupported caches must not permanently hide original rooms:
    # a new decoder version or repaired extraction can make them renderable.
    failures = set()
    seen=set()
    for entry in rows:
        a,room,*_=entry
        if a!=area or room in seen or room in failures:continue
        seen.add(room)
        if world=='mzm':
            thumbnail=OUTPUT/f'rooms/metroid/previews/{MZM_AREAS[a].lower()}_{room:03}_bg1.bmp'
        else:
            thumbnail=OUTPUT/f'rooms/aria/previews/area_{a:02}_room_{room:03}_composite.bmp'
        if thumbnail.is_file():continue
        if done+failed>=budget:break
        try:
            if world=='mzm':
                from scripts.mzm_room_render import decode_room
                decode_room(MZM_AREAS[a],room)
            else:
                from scripts.aos_room_render import render_room
                from scripts.import_aos_world import DEFAULT_ROM
                # Imported once per batch; individual decoder still validates structure.
                if 'rom' not in locals(): rom=DEFAULT_ROM.read_bytes()
                render_room(rom,a,room)
            done+=1
        except OSError as exc:
            # Missing local ROM/raw extraction can be fixed later; do not
            # blacklist a valid room just because source files are absent.
            print(f'MISSING PRIVATE INPUT for {world} {a}:{room}: {exc}', flush=True)
            break
        except (ValueError,KeyError,IndexError) as exc:
            # Missing private raw extraction is temporary, not unsupported.
            if 'missing/unsafe extracted MZM resource:' in str(exc):
                print(f'MISSING PRIVATE INPUT for {world} {a}:{room}: {exc}', flush=True)
                break
            # Unsupported original structures must not block other rooms.
            failed+=1
            failures.add(room)
            print(f'SKIPPED original {world} {a}:{room}: {exc}',flush=True)
    if world == 'mzm':
        generate_mzm_case_previews(area, rows)
    write_generated(f'world_overview/{world}_{area:02}_unsupported.json',
                    (json.dumps(sorted(failures))+'\n').encode('utf-8'))
    return done,failed


def main()->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--world',choices=('mzm','aria'),required=True)
    parser.add_argument('--area',type=int,default=0)
    parser.add_argument('--budget',type=int,default=6)
    args=parser.parse_args()
    if not 0<=args.budget<=32:parser.error('preview budget must be 0..32')
    if not 0<=args.area<(7 if args.world=='mzm' else 12):parser.error('invalid area')
    try:
        count=index(args.world)
        done,failed=generate_previews(args.world,args.area,args.budget)
    except (OSError,ValueError,KeyError,ImportError) as exc:parser.error(str(exc))
    path = OUTPUT / f'world_overview/{args.world}.tsv'
    preview_rows = [tuple(map(int, line.split('|')[:8]))
                    for line in path.read_text(encoding='utf-8').splitlines()
                    if line and not line.startswith('#')]
    seen = {(a, room) for a, room, *_ in preview_rows if a == args.area and room != 999}
    unsupported = OUTPUT / f'world_overview/{args.world}_{args.area:02}_unsupported.json'
    try:
        failures = set(json.loads(unsupported.read_text(encoding='utf-8')))
    except (OSError, ValueError):
        failures = set()
    remaining = sum(not (OUTPUT / (f'rooms/metroid/previews/{MZM_AREAS[area].lower()}_{room:03}_bg1.bmp'
                            if args.world == 'mzm' else
                            f'rooms/aria/previews/area_{area:02}_room_{room:03}_composite.bmp')).is_file()
                    for area, room in seen if room not in failures)
    print(f'{args.world}: {count} verified map cells; generated {done} previews; '
          f'{failed} unsupported; remaining={remaining}', flush=True)
    return 0

if __name__=='__main__':raise SystemExit(main())
