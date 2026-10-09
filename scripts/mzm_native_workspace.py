#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Produce private EDITABLE room block maps and a real ROM-metatile atlas.

Never patch the ROM or upstream raw files. The base map is derived from the
verified MZM import, while editor writes live under private overrides/.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path
from scripts import mzm_room_render as native
from scripts.import_game_assets import OUTPUT, write_generated

AREAS = re.compile(r'^[A-Za-z][A-Za-z0-9]{0,23}$')
MAX_BLOCKS = native.MAX_BLOCKS

def serialize(room: dict, tile_count: int, layers: dict, atlas: str) -> bytes:
    if not (1 <= tile_count <= 1024 and re.fullmatch(r'mzm:[a-z0-9]+:[0-9]{3}', room['id'])):
        raise ValueError('invalid room identity or metatile count')
    if not re.fullmatch(r'rooms/metroid/tilesets/[0-9]{1,2}_atlas\.bmp', atlas):
        raise ValueError('invalid atlas path')
    out = ['MVNATIVE 1', f"ROOM {room['id']}", f"TILESET {room['fields']['tileset']}",
           f'TILES {tile_count}', f'ATLAS {atlas}']
    for layer in ('Bg1', 'Bg2'):
        w, h, ids = layers[layer]
        if not 1 <= w <= 255 or not 1 <= h <= 255 or w*h > MAX_BLOCKS or len(ids) != w*h:
            raise ValueError('invalid room dimensions')
        if any(not 0 <= int(i) <= 65535 for i in ids):
            raise ValueError('invalid source block index')
        out.append(f'LAYER {layer.upper()} {w} {h}')
        for y in range(h):
            out.append(' '.join(f'{i:04X}' for i in ids[y*w:(y+1)*w]))
    out.append('END')
    return ('\n'.join(out) + '\n').encode('ascii')

def parse(blob: bytes) -> dict:
    lines = blob.decode('ascii').splitlines()
    if len(lines) < 10 or lines[0] != 'MVNATIVE 1' or not lines[1].startswith('ROOM ') or not lines[2].startswith('TILESET ') or not lines[3].startswith('TILES ') or not lines[4].startswith('ATLAS '):
        raise ValueError('invalid MVNATIVE header')
    roomid = lines[1][5:]
    count = int(lines[3][6:])
    if not re.fullmatch(r'mzm:[a-z0-9]+:[0-9]{3}', roomid) or not 1 <= count <= 1024:
        raise ValueError('invalid MVNATIVE metadata')
    cursor = 5
    layers = {}
    for layer in ('BG1', 'BG2'):
        name, actual, sw, sh = lines[cursor].split()
        if name != 'LAYER' or actual != layer:
            raise ValueError('invalid layer order')
        w, h = int(sw), int(sh)
        if not 1 <= w <= 255 or not 1 <= h <= 255 or w*h > MAX_BLOCKS:
            raise ValueError('invalid dimensions')
        cursor += 1
        ids = []
        for line in lines[cursor:cursor+h]:
            tokens = line.split(' ')
            if len(tokens) != w or any(not re.fullmatch(r'[0-9A-F]{4}', t) for t in tokens):
                raise ValueError('invalid block row')
            ids.extend(int(t, 16) for t in tokens)
        if len(ids) != w*h:
            raise ValueError('truncated layer')
        cursor += h
        layers[layer] = (w,h,ids)
    if lines[cursor:] != ['END']:
        raise ValueError('invalid MVNATIVE trailer')
    return {'room_id': roomid, 'tile_count': count, 'layers': layers}

def export(area: str, number: int) -> dict:
    if not AREAS.fullmatch(area) or not 0 <= number < 1000:
        raise ValueError('invalid area or room number')
    room = native.read_source_room(area, number)
    fields = room['fields']
    tid = int(fields['tileset'])
    compressed, palette, mt = native.tileset_blobs(tid)
    gfx = native.lz77(compressed)
    table = native.metatiles(mt)
    layers = {}
    for layer in ('Bg1','Bg2'):
        field = 'bg1Prop' if layer == 'Bg1' else 'bg2Prop'
        if fields[field] != 'BG_PROP_RLE_COMPRESSED':
            raise ValueError(f'{room["id"]} {layer} not supported by current RLE decoder')
        layers[layer] = native.rle_room(native.room_blob(fields['p'+layer+'Data']))
    clip_width, clip_height, clip_blocks = native.rle_room(
        native.room_blob(fields['pClipData']))
    collision_rel = f'rooms/metroid/previews/{area.lower()}_{number:03}_collision.bmp'
    write_generated(collision_rel,
                    native.collision_preview(clip_width, clip_height, clip_blocks))
    words = [entry for idx in set(layers['Bg1'][2]) if idx < len(table)
             for entry in table[idx]]
    base, _ = native.graphic_base(words, len(gfx)//32)
    # 16xN atlas: each entry rendered using the EXACT native metatile rules.
    cols = 16
    rows = (len(table)+15)//16
    atlas_width, atlas_height = cols*16, rows*16
    image = bytearray(atlas_width*atlas_height*3)
    missing = 0
    for tidx in range(len(table)):
        tile, unresolved, _ = native.render_layer(1,1,(tidx,),table,gfx,palette,base)
        missing += unresolved
        ox,oy = tidx%cols*16,tidx//cols*16
        for row in range(16):
            dest = ((oy+row)*atlas_width + ox)*3
            image[dest:dest+48] = tile[row*48:(row+1)*48]
    atlas_rel = f'rooms/metroid/tilesets/{tid}_atlas.bmp'
    if not (OUTPUT / atlas_rel).exists():
        write_generated(atlas_rel, native.bmp24(atlas_width,atlas_height,image))
    map_rel = f'rooms/metroid/workrooms/{area.lower()}_{number:03}.mvnative'
    if not (OUTPUT / map_rel).exists():
        write_generated(map_rel, serialize(room,len(table),layers,atlas_rel))
    # Deliberately NEVER overwrite edited overrides.
    override_rel = f'overrides/metroid/{area.lower()}_{number:03}.mvnative'
    info = {'room_id':room['id'],'tile_count':len(table),
            'layers':{k:{'width':v[0],'height':v[1]} for k,v in layers.items()},
            'atlas':str(OUTPUT/atlas_rel),'base':str(OUTPUT/map_rel),
            'override':str(OUTPUT/override_rel),'unresolved_atlas_cells':missing,
            'collision':str(OUTPUT/collision_rel),
            'limitations':'BG0, BG3, common tiles and animated palette omitted; edit blocks BG1/BG2 only'}
    # BG3 is a read-only optional preview. Any unsupported layer must never
    # block room editing or overwrite saved user overrides.
    try:
        from scripts.mzm_bg3_preview import render_room
        info["background"] = render_room(area, number)
    except (ValueError, OSError, IndexError, KeyError) as exc:
        info["background"] = {"status": "NOT_DECODED", "reason": str(exc)}
    from scripts.room_annotations import export_mzm
    info['annotations'] = str(OUTPUT / export_mzm(room['area'], number))
    return info

def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--area',required=True)
    ap.add_argument('--room',required=True,type=int)
    args=ap.parse_args()
    try:
        result = export(args.area,args.room)
    except (OSError,ValueError,KeyError,IndexError) as exc:
        ap.error(str(exc))
    print(json.dumps(result))
    return 0
if __name__=='__main__':
    raise SystemExit(main())
