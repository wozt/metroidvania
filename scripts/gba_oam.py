#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compose a ROM-local GBA 4bpp OBJ sprite from verified OAM and tile offsets.

This is a hardware-format decoder, not a game-specific animation extractor.
Inputs are never inferred or redistributed. Supports 1D and 2D OBJ mapping
with uncompressed graphics; affine OBJ entries and 8bpp OBJ are rejected.
"""
import argparse
import hashlib
from pathlib import Path
import struct
try:
    from scripts.gba_tiles import SHA1, bgr555, bounded_slice
except ModuleNotFoundError:  # direct invocation from the scripts directory
    from gba_tiles import SHA1, bgr555, bounded_slice

SIZES = (
    ((8, 8), (16, 16), (32, 32), (64, 64)),
    ((16, 8), (32, 8), (32, 16), (64, 32)),
    ((8, 16), (8, 32), (16, 32), (32, 64)),
)


def unpack_oam(raw):
    if len(raw) != 8:
        raise ValueError('OAM entry must contain eight bytes')
    a0, a1, a2, _ = struct.unpack('<4H', raw)
    if a0 & 0x0300:
        raise ValueError('affine/double-size or disabled OBJ not supported')
    if a0 & 0x2000:
        raise ValueError('8bpp OBJ not supported')
    shape, size = (a0 >> 14) & 3, (a1 >> 14) & 3
    if shape == 3:
        raise ValueError('reserved OBJ shape')
    w, h = SIZES[shape][size]
    x, y = a1 & 511, a0 & 255
    if x >= 256:
        x -= 512
    if y >= 128:
        y -= 256
    return dict(x=x, y=y, w=w, h=h, tile=a2 & 1023,
                bank=(a2 >> 12) & 15, hflip=bool(a1 & 0x1000),
                vflip=bool(a1 & 0x2000), priority=(a2 >> 10) & 3)


def compose(tiles, palettes, entries, origin_x, origin_y, canvas_w, canvas_h,
            mapping="1d"):
    if canvas_w < 1 or canvas_h < 1 or canvas_w > 512 or canvas_h > 512:
        raise ValueError('invalid canvas size')
    if len(palettes) != 512:
        raise ValueError('expected 256 BGR555 OBJ palette entries')
    if not tiles or len(tiles) % 32:
        raise ValueError('tile data must be 4bpp aligned')
    if mapping not in ("1d", "2d"):
        raise ValueError('OBJ mapping must be 1d or 2d')
    rgba = bytearray(canvas_w * canvas_h * 4)
    # In 1D mode rows follow the object width; in 2D mode each tile row has
    # the hardware's fixed 32-tile stride. Tile numbers count 32-byte blocks.
    for entry in reversed(entries):  # OAM index zero has highest priority.
        for iy in range(entry['h']):
            for ix in range(entry['w']):
                sx = entry['w'] - 1 - ix if entry['hflip'] else ix
                sy = entry['h'] - 1 - iy if entry['vflip'] else iy
                row_stride = entry['w'] // 8 if mapping == "1d" else 32
                tile = entry['tile'] + (sy // 8) * row_stride + sx // 8
                off = tile * 32 + (sy % 8) * 4 + (sx % 8) // 2
                if off >= len(tiles):
                    raise ValueError(f'OBJ tile {tile} outside supplied tile region')
                index = (tiles[off] >> (4 * (sx & 1))) & 15
                if index == 0:
                    continue
                px = entry['x'] + ix - origin_x
                py = entry['y'] + iy - origin_y
                if not (0 <= px < canvas_w and 0 <= py < canvas_h):
                    continue
                color_off = (entry['bank'] * 16 + index) * 2
                rgb = bgr555(struct.unpack_from('<H', palettes, color_off)[0])
                dst = (py * canvas_w + px) * 4
                rgba[dst:dst + 4] = bytes((*rgb, 255))
    return rgba


def to_bmp(rgba, width, height):
    """32-bit BGRA BMP with alpha and explicit V4 color masks."""
    pitch = width * 4
    data = bytearray(len(rgba))
    for y in range(height):
        for x in range(width):
            src = (y * width + x) * 4
            dst = ((height - 1 - y) * width + x) * 4
            r, g, b, a = rgba[src:src + 4]
            data[dst:dst + 4] = bytes((b, g, r, a))
    # BITMAPV4HEADER (108 bytes) explicitly declares alpha channel masks.
    h = bytearray(108)
    struct.pack_into('<IiiHHII', h, 0, 108, width, height, 1, 32, 3, len(data))
    struct.pack_into('<IIII', h, 40, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000)
    struct.pack_into('<I', h, 56, 0x73524742)  # LCS_sRGB
    return struct.pack('<2sIHHI', b'BM', 14 + 108 + len(data), 0, 0, 122) + h + data


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--character', required=True, choices=sorted(SHA1))
    p.add_argument('--rom', required=True, type=Path)
    p.add_argument('--tiles-offset', type=lambda s: int(s, 0), required=True)
    p.add_argument('--tile-count', type=int, required=True)
    p.add_argument('--palette-offset', type=lambda s: int(s, 0), required=True)
    p.add_argument('--oam-offset', type=lambda s: int(s, 0), required=True)
    p.add_argument('--oam-count', type=int, required=True)
    p.add_argument('--origin-x', type=int, default=0)
    p.add_argument('--origin-y', type=int, default=0)
    p.add_argument('--width', type=int, default=64)
    p.add_argument('--height', type=int, default=64)
    p.add_argument('--mapping', choices=('1d', '2d'), default='1d')
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if not (1 <= a.tile_count <= 1024 and 1 <= a.oam_count <= 128):
        p.error('tile-count must be 1..1024 and oam-count 1..128')
    allowed = (Path(__file__).resolve().parent.parent / 'assets/extracted').resolve()
    output = a.output.resolve()
    if allowed not in output.parents or output.suffix.lower() != '.bmp':
        p.error(f'output must be a .bmp below {allowed}')
    data = a.rom.read_bytes()
    if hashlib.sha1(data).hexdigest() != SHA1[a.character]:
        p.error('ROM does not match verified USA SHA-1')
    try:
        tiles = bounded_slice(data, a.tiles_offset, a.tile_count * 32, 'tiles')
        palette = bounded_slice(data, a.palette_offset, 512, 'OBJ palette')
        oam = bounded_slice(data, a.oam_offset, a.oam_count * 8, 'OAM')
        entries = [unpack_oam(oam[i:i + 8]) for i in range(0, len(oam), 8)]
        bmp = to_bmp(compose(tiles, palette, entries,
                             a.origin_x, a.origin_y, a.width, a.height,
                             a.mapping), a.width, a.height)
    except ValueError as exc:
        p.error(str(exc))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(bmp)
    print(f'Wrote local OAM sprite: {output} ({len(entries)} OBJ entries)')
    print('Not a verified Samus/Soma pose unless the offsets and tile mapping are confirmed.')


if __name__ == '__main__':
    main()
