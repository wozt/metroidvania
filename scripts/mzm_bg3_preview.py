#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Experimental original MZM BG3 text-background preview from verified ROM data.

Output is a private, partial *diagnostic*, not a complete GBA frame. Missing
common palette rows and special effects remain unresolved rather than faked.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import struct
from scripts import mzm_room_render as native
from scripts.import_game_assets import OUTPUT, write_generated

SOURCE = native.ROOT / 'third_party/mzm/src/data/rooms_data.c'


def source_bg3_blob(symbol: str) -> bytes:
    if symbol in ('sSaveRoom_Bg3', 'sMapRoom_Bg3'):
        kind = 'save_room_bg3.rle' if symbol == 'sSaveRoom_Bg3' else 'map_room_bg3.rle'
        return native.read_raw('rooms/' + kind)
    if symbol == 'sBg3_Empty':
        return native.read_raw('rooms/bg3_empty.gfx.lz')
    m = re.fullmatch(r's([A-Za-z0-9]+)_Bg3_(\d+)', symbol)
    if m:
        area, number = m.groups()
        return native.read_raw(f'rooms/{area.lower()}/{area.lower()}_bg3_{number}.gfx')
    raise ValueError(f'BG3 resource not decoded: {symbol}')


def tileset_background_resource(tileset: int) -> bytes:
    source = SOURCE.read_text(encoding='utf-8')
    table = source.split('const struct TilesetEntry sTilesetEntries[79] = {', 1)[1]
    table = table.split('\n};', 1)[0]
    m = re.search(r'\[' + str(tileset) + r'\]\s*=\s*\{([^{}]+)\}', table)
    if not m:
        raise ValueError(f'no native tileset entry for {tileset}')
    gfx = re.search(r'\.pBackgroundGraphics\s*=\s*(s[A-Za-z0-9_]+)', m.group(1))
    if not gfx:
        raise ValueError(f'no background tileset reference for {tileset}')
    symbol = gfx.group(1)
    if symbol == 'sTileset_0_Bg_Gfx':
        return native.read_raw('rooms/tileset_0_background.gfx.lz')
    match = re.fullmatch(r'sTileset_(\d+)_Bg_Gfx', symbol)
    if not match:
        raise ValueError(f'unknown BG tileset resource {symbol}')
    return native.read_raw(f'tilesets/{match.group(1)}_bg.gfx.lz')


def preview(bg3_blob: bytes, background_gfx: bytes, palette: bytes) -> tuple[bytes, dict]:
    # Pinned MZM src/room.c: gCurrentRoomEntry.bg3Size=*data; LZ stream at data+4.
    if len(bg3_blob) < 9:
        raise ValueError('BG3 header too short')
    map_data = native.lz77(bg3_blob[4:])
    if len(map_data) not in (2048, 4096):
        raise ValueError(f'unexpected BG3 tilemap size {len(map_data)}')
    gfx = native.lz77(background_gfx)
    if not gfx or len(gfx) % 32:
        raise ValueError('BG3 background graphics not 4bpp tile-aligned')
    # RoomLoadTileset loads BG3 gfx into end of VRAM, charbase=3 candidate.
    base_byte = 0xfde0 - len(gfx) - 0xc000
    if base_byte < 0 or base_byte % 32:
        raise ValueError('BG3 charbase offset not tile aligned')
    base = base_byte // 32
    tw = 32
    th = len(map_data) // (tw * 2)
    pixels = bytearray(tw*8*th*8*3)
    visible = 0
    unresolved = 0
    for i in range(tw*th):
        word = struct.unpack_from('<H', map_data, i*2)[0]
        tile = (word & 1023) - base
        bank = word >> 12
        if tile < 0 or tile >= len(gfx)//32 or bank < 3 or bank > 15:
            unresolved += 1
            continue
        x0 = (i % tw)*8
        y0 = (i // tw)*8
        for y in range(8):
            for x in range(8):
                tx = 7-x if word & 0x400 else x
                ty = 7-y if word & 0x800 else y
                index = native.tile_pixel(gfx, tile, tx, ty)
                if index == 0:
                    continue
                color = native.palette_color(palette, bank, index)
                if color is None:
                    unresolved += 1
                    continue
                dst = ((y0+y) * tw*8 + x0+x)*3
                pixels[dst:dst+3] = bytes(color)
                visible += 1
    if visible == 0:
        raise ValueError('BG3 no visible validated tiles in available palette')
    return native.bmp24(tw*8, th*8, pixels), {
        'status': 'EXPERIMENTAL_BG3_TEXT_MAP', 'visible_pixels': visible,
        'unresolved_tiles': unresolved, 'width': tw*8, 'height': th*8,
        'caveat': 'Charbase/palette origin inferred; animated BG3, scrolling and blend not reproduced'
    }


def render_room(area: str, room_number: int) -> dict:
    room = native.read_source_room(area, room_number)
    fields = room['fields']
    symbol = fields['pBg3Data']
    palette = native.tileset_blobs(int(fields['tileset']))[1]
    result, report = preview(source_bg3_blob(symbol),
                             tileset_background_resource(int(fields['tileset'])), palette)
    rel = f'rooms/metroid/previews/{area.lower()}_{room_number:03}_bg3.bmp'
    write_generated(rel, result)
    report['path'] = rel
    report['source_symbol'] = symbol
    return report


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--area', default='Brinstar')
    p.add_argument('--room', type=int, default=33)
    args = p.parse_args()
    try:
        info = render_room(args.area, args.room)
    except (ValueError, OSError, IndexError, KeyError) as e:
        p.error(str(e))
    print('Private experimental MZM BG3 preview:', OUTPUT/info['path'])
    print('Visible pixels:', info['visible_pixels'], 'unresolved tiles:', info['unresolved_tiles'])
    print(info['caveat'])
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
