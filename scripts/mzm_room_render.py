#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render original MZM room BG1/BG2 metatile layers from PRIVATE ROM blobs.

This is a partial offline reconstruction, not an emulated framebuffer and NOT
complete GBA compositing. Uses pinned decomp RoomEntryRom declarations,
RoomRleDecompress, the tileset table, and verified-ROM raw imports.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.import_mzm_rooms import ROOM_SOURCE, decode_room_descriptors
from scripts.import_game_assets import OUTPUT, write_generated
from scripts.gba_tiles import bgr555

MAX_RAW = 128 * 1024
MAX_TILES = 1024
MAX_BLOCKS = 6144
ROOM_PREFIX = 'raw/metroid/data'
SYM = re.compile(r'^s([A-Za-z0-9]+)_(\d+)_(Bg[012]|Clipdata)$')


def read_raw(name: str) -> bytes:
    # The name is constructed only from trusted, pinned descriptor tokens.
    path = OUTPUT / ROOM_PREFIX / name
    if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_RAW:
        raise ValueError(f'missing/unsafe extracted MZM resource: {path}')
    return path.read_bytes()


def lz77(data: bytes, *, limit: int = MAX_RAW) -> bytes:
    """GBA BIOS LZ77 0x10 stream, with rigorous backreference bounds."""
    if len(data) < 5 or data[0] != 0x10:
        raise ValueError('invalid 0x10 LZ77 header')
    size = int.from_bytes(data[1:4], 'little')
    if not 0 < size <= limit:
        raise ValueError('invalid LZ77 expanded size')
    out = bytearray()
    pos = 4
    while len(out) < size:
        if pos >= len(data):
            raise ValueError('truncated LZ77 control byte')
        flags = data[pos]
        pos += 1
        for mask in (128, 64, 32, 16, 8, 4, 2, 1):
            if len(out) == size:
                break
            if flags & mask:
                if pos + 2 > len(data):
                    raise ValueError('truncated LZ77 backreference')
                token = (data[pos] << 8) | data[pos + 1]
                pos += 2
                amount = (token >> 12) + 3
                distance = (token & 4095) + 1
                if distance > len(out) or len(out) + amount > size:
                    raise ValueError('LZ77 invalid backreference')
                for _ in range(amount):
                    out.append(out[-distance])
            else:
                if pos >= len(data):
                    raise ValueError('truncated LZ77 literal')
                out.append(data[pos])
                pos += 1
    if any(data[pos:]):
        raise ValueError('unexpected nonzero LZ77 trailing data')
    return bytes(out)


def rle_room(data: bytes) -> tuple[int, int, tuple[int, ...]]:
    """MZM custom two-pass RLE; first two bytes are block dimensions.

    Mirrors RoomRleDecompress(TRUE, src + 2, dst) in pinned src/room.c.
    """
    if len(data) < 6:
        raise ValueError('truncated room RLE')
    width, height = data[:2]
    total = width * height
    if not 0 < width <= 128 or not 0 < height <= 128 or not 0 < total <= MAX_BLOCKS:
        raise ValueError('invalid room dimensions')
    pos = 2
    halves = []
    for _ in range(2):
        if pos >= len(data):
            raise ValueError('missing room RLE pass')
        count_bytes = data[pos]
        pos += 1
        if count_bytes not in (1, 2):
            raise ValueError('invalid room RLE pass width')
        high_bit = 128 if count_bytes == 1 else 32768
        half = bytearray()
        while True:
            if pos + count_bytes > len(data):
                raise ValueError('truncated room RLE count')
            count = int.from_bytes(data[pos:pos + count_bytes], 'big')
            pos += count_bytes
            if count == 0:
                break
            encoded = bool(count & high_bit)
            count &= high_bit - 1
            if not count or len(half) + count > total:
                raise ValueError('room RLE pass overflow')
            if encoded:
                if pos >= len(data):
                    raise ValueError('truncated room RLE repeat')
                half.extend([data[pos]] * count)
                pos += 1
            else:
                if pos + count > len(data):
                    raise ValueError('truncated room RLE literals')
                half.extend(data[pos:pos + count])
                pos += count
        if len(half) != total:
            raise ValueError(f'room RLE pass length {len(half)} != {total}')
        halves.append(half)
    if any(data[pos:]):
        raise ValueError('room RLE has nonzero trailing bytes')
    return width, height, tuple(halves[0][i] | halves[1][i] << 8 for i in range(total))


def metatiles(data: bytes) -> list[tuple[int, int, int, int]]:
    """Tileset metatile table: source RoomLoadTileset reads pTilemap + 2.

    A u16 header precedes quadruples of GBA BG text tile entries; an optional
    trailing alignment word is ignored. No fabricated entries are added.
    """
    if len(data) < 10 or len(data) > 32768:
        raise ValueError('invalid tileset metatile table size')
    entries = (len(data) - 2) // 8
    if not 0 < entries <= 1024:
        raise ValueError('invalid metatile count')
    return [struct.unpack_from('<4H', data, 2 + n * 8) for n in range(entries)]


def graphic_base(words: list[int], tile_count: int) -> tuple[int, int]:
    """Infer character-block origin from valid referenced tile indices.

    The MZM room loader copies the compressed tiles to VRAM+0x5800; common
    engine tiles occupy other VRAM positions. Report unresolved indices.
    """
    ids = [v & 1023 for v in words]
    # Charbase 1 -> 0x5800 - 0x4000 = 0x1800 = 192 tiles.
    # Charbase 0 -> 0x5800 = 704 tiles. Only select with evidence.
    candidates = (192, 704)
    ranked = [(sum(base <= n < base + tile_count for n in ids), base)
              for base in candidates]
    coverage, base = max(ranked, key=lambda v: (v[0], -abs(v[1] - 192)))
    if coverage == 0:
        raise ValueError('no BG metatile indices refer to tileset graphics')
    return base, coverage


def read_source_room(area: str, number: int) -> dict:
    if not ROOM_SOURCE.is_file():
        raise ValueError('pinned MZM source missing: git submodule update --init')
    rooms = decode_room_descriptors(ROOM_SOURCE.read_text(encoding='utf-8'))
    for room in rooms:
        if room['area'].lower() == area.lower() and room['index'] == number:
            return room
    raise ValueError(f'unknown Zero Mission room {area}:{number}')


def room_blob(symbol: str) -> bytes:
    m = SYM.fullmatch(symbol)
    if not m:
        raise ValueError(f'unsupported room resource symbol: {symbol}')
    area, index, kind = m.groups()
    name = f'rooms/{area.lower()}/{area.lower()}_{index}_{kind.lower()}.gfx'
    return read_raw(name)


def tileset_blobs(index: int) -> tuple[bytes, bytes, bytes]:
    if not 0 <= index <= 78:
        raise ValueError('unsupported Zero Mission tileset')
    return (read_raw(f'tilesets/{index}.gfx.lz'),
            read_raw(f'tilesets/{index}.pal'),
            read_raw(f'tilesets/{index}.tm'))


def tile_pixel(gfx: bytes, tile_index: int, tx: int, ty: int) -> int:
    pos = tile_index * 32 + ty * 4 + tx // 2
    b = gfx[pos]
    return (b >> (4 * (tx & 1))) & 15


def palette_color(palette: bytes, bank: int, index: int) -> tuple[int, int, int] | None:
    # RoomLoadTileset loads source palette rows 1..13 into BG palette rows 3..15.
    # Palette rows 0..2 are common graphics and must NOT be invented.
    if bank < 3 or bank > 15:
        return None
    entry = (bank - 2) * 16 + index
    if entry * 2 + 2 > len(palette):
        return None
    return bgr555(struct.unpack_from('<H', palette, entry * 2)[0])


def render_layer(width: int, height: int, blocks: tuple[int, ...],
                 table: list[tuple[int, int, int, int]], gfx: bytes,
                 palette: bytes, base: int) -> tuple[bytearray, int, int]:
    tw, th = width * 16, height * 16
    rgb = bytearray(tw * th * 3)
    unresolved = 0
    painted = 0
    for by in range(height):
        for bx in range(width):
            block = blocks[by * width + bx]
            if block >= len(table):
                unresolved += 1
                continue
            for cell_y in range(2):
                for cell_x in range(2):
                    word = table[block][cell_y * 2 + cell_x]
                    tile = (word & 1023) - base
                    bank = (word >> 12) & 15
                    if not 0 <= tile < len(gfx) // 32 or bank < 3:
                        unresolved += 1
                        continue
                    for y in range(8):
                        for x in range(8):
                            px = 7 - x if (word & 0x400) else x
                            py = 7 - y if (word & 0x800) else y
                            color_index = tile_pixel(gfx, tile, px, py)
                            color = palette_color(palette, bank, color_index)
                            if color is None:
                                unresolved += 1
                                continue
                            dst = ((by * 16 + cell_y * 8 + y) * tw +
                                   bx * 16 + cell_x * 8 + x) * 3
                            rgb[dst:dst + 3] = bytes(color)
                            painted += 1
    return rgb, unresolved, painted


def bmp24(width: int, height: int, rgb: bytes) -> bytes:
    if not 0 < width <= 4096 or not 0 < height <= 4096 or len(rgb) != width*height*3:
        raise ValueError('invalid RGB bitmap')
    pitch = (width * 3 + 3) & ~3
    pixels = bytearray(pitch * height)
    for y in range(height):
        for x in range(width):
            src = (y * width + x) * 3
            dst = (height - y - 1) * pitch + x * 3
            pixels[dst:dst + 3] = rgb[src:src + 3][::-1]
    return (struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54) +
            struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0,
                        len(pixels), 0, 0, 0, 0) + pixels)


def decode_room(area: str, number: int) -> dict:
    room = read_source_room(area, number)
    fields = room['fields']
    tid = int(fields['tileset'])
    compressed, palette, mt = tileset_blobs(tid)
    gfx = lz77(compressed)
    if len(gfx) % 32:
        raise ValueError('tileset is not 4bpp 8x8 aligned')
    table = metatiles(mt)
    layers = {}
    block_maps = {}
    for layer in ('Bg1', 'Bg2'):
        key = 'p' + layer + 'Data'
        if fields.get('bg1Prop' if layer == 'Bg1' else 'bg2Prop') != 'BG_PROP_RLE_COMPRESSED':
            layers[layer] = {'status': 'NOT_RLE', 'source': fields.get(key)}
            continue
        w, h, blocks = rle_room(room_blob(fields[key]))
        # Original rooms may contain special/common engine blocks not present
        # in the local tileset. Mark them unresolved rather than invent pixels.
        block_maps[layer] = (w,h,blocks)
        layers[layer] = {'status': 'DECODED_METATILES', 'width_blocks': w,
                         'height_blocks': h, 'block_count': len(blocks),
                         'unique_blocks': len(set(blocks))}
    if 'Bg1' not in block_maps:
        raise ValueError('room BG1 RLE not available')
    words = [entry for idx in set(block_maps['Bg1'][2])
             if idx < len(table) for entry in table[idx]]
    base, coverage = graphic_base(words, len(gfx) // 32)
    result = {'format': 'MV_MZM_RENDER_1', 'room_id': room['id'],
              'area': room['area'], 'index': number, 'tileset': tid,
              'native_music': fields['musicTrack'],
              'status': 'PARTIAL_NATIVE_BG_RENDER',
              'graphics_tile_base': base, 'graphics_tiles': len(gfx)//32,
              'tileset_entries': len(table), 'sample_index_coverage': coverage,
              'layers': layers,
              'limitations': ['common GBA tiles and palette rows 0..2 not decoded',
                              'BG0, BG3, blending, animated graphics and entities omitted',
                              'rendered BG1/BG2 images are independent opaque diagnostic previews']}
    for layer, (w,h,blocks) in block_maps.items():
        image, missing, painted = render_layer(w,h,blocks,table,gfx,palette,base)
        rel = f'rooms/metroid/previews/{area.lower()}_{number:03}_{layer.lower()}.bmp'
        write_generated(rel, bmp24(w*16,h*16,image))
        layers[layer].update({'path': rel, 'unresolved_pixels_or_cells': missing,
                              'painted_pixels': painted})
    rel = f'rooms/metroid/previews/{area.lower()}_{number:03}.json'
    write_generated(rel, (json.dumps(result, indent=2)+'\n').encode())
    return result


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--area', default='Brinstar')
    ap.add_argument('--room', type=int, default=33)
    args = ap.parse_args()
    try:
        result = decode_room(args.area, args.room)
    except (ValueError, OSError, KeyError, IndexError) as exc:
        ap.error(str(exc))
    print('Decoded original room metatiles:', result['room_id'])
    for layer in result['layers'].values():
        if 'path' in layer:
            print('  ', OUTPUT/layer['path'], 'painted pixels=', layer['painted_pixels'],
                  'unresolved=', layer['unresolved_pixels_or_cells'])
    print('PARTIAL AUTHENTIC BG1/BG2 PREVIEW: common graphics and other layers not rendered')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
