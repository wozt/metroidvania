#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Follow a verified MZM Samus frame pointer and stage its four graphics banks.

This outputs a diagnostic 32 KiB OBJ VRAM dump, NOT a finished sprite.
The actor OAM, arm cannon, effects, and palette staging must still be decoded.
No copyrighted data is checked into the repository.
"""
import argparse
import hashlib
from pathlib import Path
import struct

EXPECTED_SHA1 = '5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8'
ROM_BASE = 0x08000000
ROM_LIMIT = 0x0e000000
OBJ_VRAM_BYTES = 0x8000
# Four independent 4bpp block uploads documented in mzm/docs/samus/graphics.md.
BANKS = ((0x000, 0x280), (0x400, 0x280), (0x280, 0x180), (0x680, 0x180))

def rom_offset(pointer, rom):
    if not ROM_BASE <= pointer < ROM_LIMIT:
        raise ValueError(f'not a GBA ROM pointer: 0x{pointer:08x}')
    offset = pointer - ROM_BASE
    if offset >= len(rom):
        raise ValueError('pointer exceeds supplied ROM size')
    return offset

def read_block(rom, pointer):
    off = rom_offset(pointer, rom)
    if off + 2 > len(rom):
        raise ValueError('truncated graphics header')
    count1, count2 = rom[off:off+2]
    size = (count1 + count2) * 32
    if off + 2 + size > len(rom):
        raise ValueError('truncated graphics data')
    a = rom[off+2:off+2+count1*32]
    b = rom[off+2+count1*32:off+2+size]
    return a, b

def parse_frame(rom, frame_pointer):
    off = rom_offset(frame_pointer, rom)
    if off + 13 > len(rom):
        raise ValueError('truncated frame record')
    upper, lower, oam = struct.unpack_from('<III', rom, off)
    duration = rom[off+12]
    rom_offset(oam, rom)
    return upper, lower, oam, duration

def stage(rom, frame_pointer):
    upper, lower, oam, duration = parse_frame(rom, frame_pointer)
    up1, up2 = read_block(rom, upper)
    lo1, lo2 = read_block(rom, lower)
    # top subset1 = shoulders, top subset2 = torso;
    # lower subset1 = legs, lower subset2 = lower body.
    pieces = (up1, up2, lo1, lo2)
    vram = bytearray(OBJ_VRAM_BYTES)
    for piece, (offset, capacity) in zip(pieces, BANKS):
        if len(piece) > capacity:
            raise ValueError(f'graphics subset exceeds VRAM slot 0x{offset:x}')
        vram[offset:offset+len(piece)] = piece
    return bytes(vram), {'frame_pointer': frame_pointer,
                         'upper_pointer': upper, 'lower_pointer': lower,
                         'oam_pointer': oam, 'duration': duration,
                         'tile_counts': [len(p)//32 for p in pieces]}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--frame-pointer', type=lambda s: int(s, 0), required=True,
                   help='Verified 0x08xxxxxx GBA address of SamusAnimationData record')
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parent.parent
    allowed = (root/'assets/extracted').resolve()
    output = a.output.resolve()
    if allowed not in output.parents or output.suffix != '.bin':
        p.error(f'output must be a .bin file below {allowed}')
    rom = a.rom.read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        p.error('Metroid Zero Mission USA ROM SHA-1 mismatch')
    try:
        vram, metadata = stage(rom, a.frame_pointer)
    except ValueError as ex:
        p.error(str(ex))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(vram)
    for k, v in metadata.items():
        print(f'{k}: {v if not k.endswith("pointer") else hex(v)}')
    print(f'Staged {len(vram)} bytes of local-only OBJ VRAM: {output}')
    print('Not yet a sprite: OAM, palette, arm cannon, and effects remain.')

if __name__ == '__main__':
    main()
