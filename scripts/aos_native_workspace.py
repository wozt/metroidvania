#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Convert verified Aria RGBA backgrounds into private editable 16x16 workroom tiles.

This is a visual workroom conversion; the original room's independent 8x8
entries, effects, collision flags and gameplay script are NOT re-encoded yet.
Only local, exact-SHA1-verified Aria USA ROM bytes are used.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

from scripts import aos_room_render as renderer
from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1, decode_world
from scripts.import_game_assets import OUTPUT, write_generated

MAX_TILES = 1024
MAX_CELLS = 6144
TRANSPARENT = bytes(16 * 16 * 4)


def png_rgba(width: int, height: int, rgba: bytes) -> bytes:
    """Pure-stdlib lossless RGBA PNG, loadable by GdkPixbuf."""
    if width < 16 or height < 16 or width > 256 or height > 1024 or len(rgba) != width * height * 4:
        raise ValueError('invalid Aria atlas image')
    def chunk(code: bytes, data: bytes) -> bytes:
        return struct.pack('>I', len(data)) + code + data + struct.pack('>I', zlib.crc32(code + data))
    raw = b''.join(b'\0' + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, level=6)) + chunk(b'IEND', b''))


def split_tiles(rgba: bytes, width: int, height: int, tile_map: dict[bytes, int],
                atlas: list[bytes]) -> tuple[int, int, list[int]]:
    if width <= 0 or height <= 0 or width % 16 or height % 16 or len(rgba) != width * height * 4:
        raise ValueError('Aria layer is not aligned to 16px tiles')
    w, h = width // 16, height // 16
    if w * h > MAX_CELLS:
        raise ValueError(f'Aria room exceeds {MAX_CELLS} editable cells per layer')
    values = []
    for cy in range(h):
        for cx in range(w):
            tile = b''.join(rgba[((cy * 16 + y) * width + cx * 16) * 4:
                                 ((cy * 16 + y) * width + cx * 16 + 16) * 4]
                            for y in range(16))
            if tile not in tile_map:
                if len(atlas) >= MAX_TILES:
                    raise ValueError('more than 1024 unique 16x16 visual tiles: atlas expansion required')
                tile_map[tile] = len(atlas)
                atlas.append(tile)
            values.append(tile_map[tile])
    return w, h, values


def pack_atlas(tiles: list[bytes]) -> bytes:
    rows = (len(tiles) + 15) // 16
    width, height = 256, rows * 16
    pixels = bytearray(width * height * 4)
    for i, tile in enumerate(tiles):
        px, py = (i % 16) * 16, (i // 16) * 16
        for y in range(16):
            dest = ((py + y) * width + px) * 4
            pixels[dest:dest + 64] = tile[y * 64:y * 64 + 64]
    return png_rgba(width, height, bytes(pixels))


def serialize(area: int, room: int, layers: dict[str, tuple[int, int, list[int]]],
              tile_count: int, atlas: str) -> bytes:
    if not 0 <= area < 12 or not 0 <= room < 1000 or not 1 <= tile_count <= MAX_TILES:
        raise ValueError('invalid Aria room identity or atlas size')
    if not atlas.startswith('rooms/aria/tilesets/') or not atlas.endswith('_atlas.png'):
        raise ValueError('invalid Aria atlas reference')
    lines = ['MVNATIVE 1', f'ROOM aria:{area:02d}:{room:03d}',
             f'TILESET {area}', f'TILES {tile_count}', f'ATLAS {atlas}']
    for name in ('BG1', 'BG2'):
        w, h, cells = layers[name]
        if not 1 <= w <= 128 or not 1 <= h <= 128 or w * h > MAX_CELLS or len(cells) != w * h:
            raise ValueError('invalid editable layer dimensions')
        if any(not 0 <= cell < tile_count for cell in cells):
            raise ValueError('invalid tile atlas reference')
        lines.append(f'LAYER {name} {w} {h}')
        for y in range(h):
            lines.append(' '.join(f'{c:04X}' for c in cells[y*w:(y+1)*w]))
    return ('\n'.join(lines) + '\nEND\n').encode('ascii')


def export(rom: bytes, area: int, room_number: int) -> dict:
    if not 0 <= area < 12 or not 0 <= room_number < 1000:
        raise ValueError('invalid Aria room identity')
    if hashlib.sha1(rom, usedforsecurity=False).hexdigest() != EXPECTED_SHA1:
        raise ValueError('expected unmodified Aria of Sorrow USA ROM')
    world = decode_world(rom, verify_hash=False)
    room = next((r for r in world['rooms'] if r['engine_area'] == area and r['room'] == room_number), None)
    if room is None:
        raise ValueError('unknown Aria room')
    vram, palette, _ = renderer.load_video_memory(rom, room)
    decoded = [renderer.decode_background(rom, d) for d in room['backgrounds']]
    bg_layers = {d['layer']: d for d in decoded}
    master = next((d for d in decoded if d['status'] == 'DECODED_TEXT_BACKGROUND'), None)
    if master is None:
        raise ValueError('this Aria room has no supported 4bpp background')
    tiles = [TRANSPARENT]
    keys = {TRANSPARENT: 0}
    layers = {}
    for name, number in (('BG1', 1), ('BG2', 2)):
        item = bg_layers[number]
        if item['status'] == 'DECODED_TEXT_BACKGROUND':
            rgba, _ = renderer.render_background(item, vram, palette)
            w = item['width_tiles'] * 8
            h = item['height_tiles'] * 8
        else:
            w = master['width_tiles'] * 8
            h = master['height_tiles'] * 8
            rgba = bytes(w * h * 4)
        layers[name] = split_tiles(rgba, w, h, keys, tiles)
    basename = f'area_{area:02d}_room_{room_number:03d}'
    atlas_name = f'rooms/aria/tilesets/{basename}_atlas.png'
    workroom_name = f'rooms/aria/workrooms/{basename}.mvnative'
    override_name = f'overrides/aria/{basename}.mvnative'
    # Never clobber any edited override or already-authored workroom.
    if not (OUTPUT / atlas_name).exists():
        write_generated(atlas_name, pack_atlas(tiles))
    if not (OUTPUT / workroom_name).exists():
        write_generated(workroom_name, serialize(area, room_number, layers, len(tiles), atlas_name))
    bg1 = bg_layers.get(1)
    collision_name = None
    if bg1 and bg1.get('collision') is not None:
        collision_name = f'rooms/aria/previews/{basename}_collision.bmp'
        write_generated(collision_name, renderer.collision_preview(bg1))
    return {'room_id': f'aria:{area:02d}:{room_number:03d}',
            'tile_count': len(tiles), 'atlas': str(OUTPUT/atlas_name),
            'workroom': str(OUTPUT/workroom_name),
            'override': str(OUTPUT/override_name),
            'collision': str(OUTPUT/collision_name) if collision_name else None,
            'limitation': 'Visual 16x16 RGBA cells; original 8x8 tile IDs, animations, entities, collisions and BG3 editability not yet preserved.'}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, default=DEFAULT_ROM)
    parser.add_argument('--area', type=int, required=True)
    parser.add_argument('--room', type=int, required=True)
    args = parser.parse_args()
    try:
        result = export(args.rom.read_bytes(), args.area, args.room)
    except (OSError, ValueError, KeyError, IndexError, struct.error) as exc:
        parser.error(str(exc))
    print(json.dumps(result))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
