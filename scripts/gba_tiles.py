#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode a 4bpp GBA tile grid from an OWNED local ROM into a BMP preview.

This intentionally does NOT identify game-specific assets, decompress LZ77,
or reconstruct OAM and animation frames. Offsets must be verified separately.
"""
import argparse
import hashlib
from pathlib import Path
import struct

SHA1 = {
    'samus': '5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8',
    'soma': 'abd71fe01ebb201bcc133074db1dd8c5253776c7',
}

def bgr555(value):
    return tuple(((value >> shift) & 31) * 255 // 31 for shift in (0, 5, 10))

def decode_tiles(tile_data, palette, tiles_per_row):
    if not tile_data or len(tile_data) % 32:
        raise ValueError('tile data must consist of complete 32-byte 4bpp tiles')
    if len(palette) != 32:
        raise ValueError('palette must have exactly 16 BGR555 colors')
    colors = [bgr555(struct.unpack_from('<H', palette, n * 2)[0]) for n in range(16)]
    tile_count = len(tile_data) // 32
    width = min(tile_count, tiles_per_row) * 8
    height = ((tile_count + tiles_per_row - 1) // tiles_per_row) * 8
    # BMP: 24-bit BGR bottom-up, with magenta marking palette index zero.
    pitch = (width * 3 + 3) & ~3
    pixels = bytearray(pitch * height)
    for tile in range(tile_count):
        tx, ty = tile % tiles_per_row * 8, tile // tiles_per_row * 8
        for y in range(8):
            for x in range(8):
                byte = tile_data[tile * 32 + y * 4 + x // 2]
                index = (byte >> (4 * (x & 1))) & 15
                rgb = (255, 0, 255) if index == 0 else colors[index]
                at = (height - 1 - (ty + y)) * pitch + (tx + x) * 3
                pixels[at:at + 3] = bytes(reversed(rgb))
    header = struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54)
    info = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, len(pixels), 0, 0, 0, 0)
    return header + info + pixels

def bounded_slice(data, offset, size, label):
    if offset < 0 or size <= 0 or offset > len(data) or size > len(data) - offset:
        raise ValueError(f'{label} outside ROM bounds')
    return data[offset:offset + size]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--character', required=True, choices=sorted(SHA1))
    parser.add_argument('--rom', required=True, type=Path)
    parser.add_argument('--tiles-offset', required=True, type=lambda x: int(x, 0))
    parser.add_argument('--palette-offset', required=True, type=lambda x: int(x, 0))
    parser.add_argument('--tile-count', type=int, default=16)
    parser.add_argument('--tiles-per-row', type=int, default=4)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if not 1 <= args.tile_count <= 4096 or not 1 <= args.tiles_per_row <= 128:
        parser.error('tile-count must be 1..4096 and tiles-per-row 1..128')
    # Never permit an output in tracked repo paths; require the ignored extraction tree.
    root = Path(__file__).resolve().parent.parent
    allowed = (root / 'assets/extracted').resolve()
    output = args.output.resolve()
    if output != allowed and allowed not in output.parents:
        parser.error(f'output must be under {allowed}')
    if output.suffix.lower() != '.bmp':
        parser.error('output must be a .bmp file')
    data = args.rom.read_bytes()
    if hashlib.sha1(data).hexdigest() != SHA1[args.character]:
        parser.error('ROM SHA-1 does not match the selected USA game')
    tiles = bounded_slice(data, args.tiles_offset, args.tile_count * 32, 'tiles')
    palette = bounded_slice(data, args.palette_offset, 32, 'palette')
    bitmap = decode_tiles(tiles, palette, args.tiles_per_row)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(bitmap)
    print(f'Wrote local-only tile preview: {output} ({args.tile_count} tiles)')
    print('NOTE: tiles are not composed into a character pose; OAM/animations remain TODO.')

if __name__ == '__main__':
    main()
