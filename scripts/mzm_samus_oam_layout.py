#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Rank *hypotheses* for the MZM Samus raw OAM layout, never label poses.

All candidate counts are capped by the raw header and the ROM boundary.
No graphical data is exported. The 2/4 byte headers and 6/8 byte strides are
hypotheses until cross-checked with the pinned mzm source implementation.
"""
import argparse
import hashlib
from pathlib import Path
import struct

try:
    from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset, parse_frame
    from scripts.gba_oam import unpack_oam
except ModuleNotFoundError:
    from mzm_samus_frame import EXPECTED_SHA1, rom_offset, parse_frame
    from gba_oam import unpack_oam


def candidates(rom, address, max_entries=32):
    if not 1 <= max_entries <= 128:
        raise ValueError('max_entries must be 1..128')
    off = rom_offset(address, rom)
    if off + 2 > len(rom):
        raise ValueError('truncated OAM header')
    header = struct.unpack_from('<H', rom, off)[0]
    claimed = header & 0x0fff
    if not 1 <= claimed <= 128:
        raise ValueError('OAM header has implausible object count')
    result = []
    for head in (2, 4):
        for stride in (6, 8):
            count = min(claimed, max_entries)
            if off + head + count * stride > len(rom):
                continue
            entries = []
            for i in range(count):
                pos = off + head + i * stride
                a0, a1, a2 = struct.unpack_from('<3H', rom, pos)
                try:
                    entry = unpack_oam(struct.pack('<4H', a0, a1, a2, 0))
                except ValueError:
                    break
                entries.append(entry)
            # A complete decode ranks ahead of a partial one. No layout is
            # asserted correct based solely on its score.
            result.append({'header_bytes': head, 'stride_bytes': stride,
                           'declared_count': claimed, 'decoded': len(entries),
                           'complete': len(entries) == count,
                           'entries': entries})
    result.sort(key=lambda item: (not item['complete'], -item['decoded'],
                                  item['header_bytes'], item['stride_bytes']))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', required=True, type=Path)
    x = p.add_mutually_exclusive_group(required=True)
    x.add_argument('--frame-pointer', type=lambda s: int(s, 0))
    x.add_argument('--oam-pointer', type=lambda s: int(s, 0))
    p.add_argument('--max-entries', type=int, default=16)
    args = p.parse_args()
    try:
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            p.error('expected an unmodified Zero Mission USA ROM')
        addr = args.oam_pointer if args.oam_pointer is not None else parse_frame(rom, args.frame_pointer)[2]
        results = candidates(rom, addr, args.max_entries)
    except (ValueError, OSError) as exc:
        p.error(str(exc))
    print(f'OAM 0x{addr:08X}: {len(results)} bounded layout hypotheses')
    for c in results:
        print(f"header={c['header_bytes']} stride={c['stride_bytes']} "
              f"count={c['declared_count']} decoded={c['decoded']} "
              f"complete={c['complete']}")
        for item in c['entries'][:3]:
            print(f"  x={item['x']} y={item['y']} size={item['w']}x{item['h']} "
                  f"tile={item['tile']} bank={item['bank']}")
    print('UNVERIFIED hypotheses; confirm with pinned mzm source before extracting sprites.')


if __name__ == '__main__':
    main()
