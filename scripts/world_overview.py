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
from pathlib import Path
from scripts.import_game_assets import OUTPUT, write_generated

MZM_AREAS = ('Brinstar','Kraid','Norfair','Ridley','Tourian','Crateria','Chozodia')


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
        native: list[tuple[int, ...]]) -> tuple[list[tuple], dict]:
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

    families = {}
    for area, room, x, y, save, warp in anchors:
        family = families.setdefault((area, x, y), {
            'area': area, 'x': x, 'y': y, 'rooms': [], 'points': set(),
            'save': False, 'warp': False,
        })
        family['rooms'].append(room)
        family['save'] |= bool(save)
        family['warp'] |= bool(warp)
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
        for yy in range(y, y + screens_y):
            for xx in range(x, x + screens_x):
                point = (area, xx, yy)
                if point in native_by_position:
                    family['points'].add(point)

    claims = {}
    for key, family in families.items():
        for point in family['points']:
            claims.setdefault(point, []).append(key)

    rows = []
    ambiguity_records = []
    owned_count = 0
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
        if owner is not None:
            family = families[owner]
            rooms = sorted(set(family['rooms']))
            primary = rooms[0]
            provenance = 1 if point in family['points'] else 0
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
        'variant_families': [
            {'area': family['area'], 'map_x': family['x'], 'map_y': family['y'],
             'rooms': sorted(set(family['rooms']))}
            for family in families.values() if len(set(family['rooms'])) > 1
        ],
        'ambiguities': ambiguity_records,
        'method': ('RoomEntryRom origins plus exact Clipdata playable dimensions, '
                   'intersected with original pause-minimap occupancy'),
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
                if tile != 0x140:
                    # 999 = unassigned native map tile, 3 = native source.
                    cells.append((area, 999, x, y, 0, 0, 3, tile))
    return cells


def output_rows(world: str, rows: list[tuple]) -> bytes:
    # 8th field: native minimap tile code (0 when unknown). Provenance:
    # 0=original MZM anchor, 1=decoded MZM clip bounds, 2=original Aria cell,
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
    head = ('# Original map cells; provenance 0=anchor, 1=clip, 2=Aria, 3=MZM minimap tile.\n'
            '# area|room|x|y|save|warp|provenance|native_tile|variants_or_ambiguity\n')
    return (head + '\n'.join(encoded) + '\n').encode('utf-8')


def index(world: str)->int:
    if world=='mzm':
        from scripts.mzm_world_atlas import run
        src=OUTPUT/'rooms/metroid/world_atlas.tsv'
        if not src.is_file(): run()
        anchors = build_mzm(src.read_text(encoding='utf-8'))
        native = native_mzm_minimap_cells()
        if native:
            rows, report = resolve_mzm_minimap_cells(
                anchors, native_mzm_clip_dimensions(), native)
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
    # Unsupported rooms are remembered privately so each batch advances.
    failures_path=OUTPUT/f'world_overview/{world}_{area:02}_unsupported.json'
    try: failures=set(json.loads(failures_path.read_text(encoding='utf-8')))
    except (OSError,ValueError): failures=set()
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
            # Unsupported original structures must not block other rooms.
            failed+=1
            failures.add(room)
            print(f'SKIPPED original {world} {a}:{room}: {exc}',flush=True)
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
    print(f'{args.world}: {count} verified map cells; generated {done} previews; {failed} unsupported',flush=True)
    return 0

if __name__=='__main__':raise SystemExit(main())
