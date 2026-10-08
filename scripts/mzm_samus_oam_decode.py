#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode MZM Samus raw OAM (2-byte count/flags + 6 bytes per OBJ).

Layout verified against pinned metroidret/mzm tools/oam.py; frame identities
and sprite palette are NOT inferred. No proprietary data is written.
"""
import argparse
import hashlib
from pathlib import Path
import struct

try:
    from scripts.mzm_samus_frame import EXPECTED_SHA1, parse_frame, rom_offset
    from scripts.gba_oam import unpack_oam
except ModuleNotFoundError:
    from mzm_samus_frame import EXPECTED_SHA1, parse_frame, rom_offset
    from gba_oam import unpack_oam


def decode_raw_samus_oam(rom, pointer, max_parts=128):
    if not 1 <= max_parts <= 128:
        raise ValueError('max_parts must be 1..128')
    offset = rom_offset(pointer, rom)
    if offset + 2 > len(rom):
        raise ValueError('truncated Samus OAM header')
    word = struct.unpack_from('<H', rom, offset)[0]
    count = word & 0xfff
    if count == 0 or count > 128 or count > max_parts:
        raise ValueError('invalid or excessive Samus OAM part count')
    if offset + 2 + count * 6 > len(rom):
        raise ValueError('truncated Samus OAM entries')
    entries = []
    for index in range(count):
        a0, a1, a2 = struct.unpack_from('<HHH', rom, offset + 2 + index * 6)
        entry = unpack_oam(struct.pack('<HHHH', a0, a1, a2, 0))
        entries.append(entry)
    return {
        'oam_pointer': pointer,
        'count': count,
        'header': word,
        'arm_cannon_front': bool(word & 0x1000),
        'arm_cannon_behind': bool(word & 0x2000),
        'other_header_flags': word & 0xc000,
        'entries': entries,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', required=True, type=Path)
    group = p.add_mutually_exclusive_group(required=True)
    group.add_argument('--frame-pointer', type=lambda v: int(v, 0))
    group.add_argument('--oam-pointer', type=lambda v: int(v, 0))
    p.add_argument('--max-parts', type=int, default=128)
    a = p.parse_args()
    try:
        data = a.rom.read_bytes()
        if hashlib.sha1(data).hexdigest() != EXPECTED_SHA1:
            p.error('Zero Mission USA ROM SHA-1 mismatch')
        pointer = a.oam_pointer if a.oam_pointer is not None else parse_frame(data, a.frame_pointer)[2]
        info = decode_raw_samus_oam(data, pointer, a.max_parts)
    except (ValueError, OSError) as exc:
        p.error(str(exc))
    print(f"Samus OAM 0x{pointer:08X}: {info['count']} parts, header=0x{info['header']:04X}")
    print(f"Arm cannon front={info['arm_cannon_front']} behind={info['arm_cannon_behind']}")
    for index, entry in enumerate(info['entries']):
        print(f"[{index:02d}] x={entry['x']:4d} y={entry['y']:4d} "
              f"size={entry['w']}x{entry['h']} tile={entry['tile']} "
              f"bank={entry['bank']} hflip={entry['hflip']} vflip={entry['vflip']}")
    print('Decoded raw Samus body OAM; palette and arm cannon staging still needed.')


if __name__ == '__main__':
    main()
