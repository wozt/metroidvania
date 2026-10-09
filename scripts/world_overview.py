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


def output_rows(world:str, rows:list[tuple[int,int,int,int,int,int]])->bytes:
    head='# Verified original minimap cells. MZM entries are anchors, not bounds.\n# area|room|x|y|save|warp\n'
    return (head+'\n'.join('|'.join(map(str,row)) for row in rows)+'\n').encode('utf-8')


def index(world: str)->int:
    if world=='mzm':
        from scripts.mzm_world_atlas import run
        src=OUTPUT/'rooms/metroid/world_atlas.tsv'
        if not src.is_file(): run()
        rows=build_mzm(src.read_text(encoding='utf-8'))
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
    rows=[tuple(map(int,line.split('|'))) for line in path.read_text().splitlines() if line and not line.startswith('#')]
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
