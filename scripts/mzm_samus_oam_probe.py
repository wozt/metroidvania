#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inspect candidate MZM Samus raw OAM layouts without exporting game assets.

A raw Samus OAM list includes a game-specific header, so this diagnostic
reports bounded alternative interpretations rather than asserting an offset.
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


def probe_oam(rom, oam_pointer, limit=12):
    if not 1 <= limit <= 128:
        raise ValueError('limit must be 1..128')
    start = rom_offset(oam_pointer, rom)
    if start + 2 > len(rom):
        raise ValueError('truncated OAM header')
    header = struct.unpack_from('<H', rom, start)[0]
    hypotheses = []
    # Several GBA game formats put a count/flags halfword before 6-byte
    # OBJ entries; inspect 2- and 4-byte headers and optional 8-byte records.
    for skip in (2, 4):
        for stride in (6, 8):
            entries = []
            for i in range(limit):
                pos = start + skip + i * stride
                if pos + 6 > len(rom):
                    break
                attr0, attr1, attr2 = struct.unpack_from('<3H', rom, pos)
                try:
                    entry = unpack_oam(struct.pack('<4H', attr0, attr1, attr2, 0))
                except ValueError:
                    break
                entries.append(entry)
            hypotheses.append({'header_bytes': skip, 'entry_bytes': stride,
                               'decoded': len(entries), 'entries': entries})
    return {'address': oam_pointer, 'header_word': header,
            'possible_count_low12': header & 0x0fff,
            'flags_high4': header >> 12, 'hypotheses': hypotheses}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', type=Path, required=True)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument('--oam-pointer', type=lambda s: int(s, 0))
    g.add_argument('--frame-pointer', type=lambda s: int(s, 0))
    p.add_argument('--limit', type=int, default=6)
    a = p.parse_args()
    if not 1 <= a.limit <= 32:
        p.error('--limit must be 1..32')
    try:
        rom = a.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            p.error('Zero Mission USA ROM SHA-1 mismatch')
        ptr = a.oam_pointer if a.oam_pointer is not None else parse_frame(rom, a.frame_pointer)[2]
        info = probe_oam(rom, ptr, a.limit)
    except (OSError, ValueError) as exc:
        p.error(str(exc))
    print(f"OAM=0x{ptr:08X} header=0x{info['header_word']:04X} "
          f"low12={info['possible_count_low12']} flags=0x{info['flags_high4']:X}")
    for h in info['hypotheses']:
        print(f"  header={h['header_bytes']} stride={h['entry_bytes']} "
              f"decoded={h['decoded']}")
        for e in h['entries'][:min(3, a.limit)]:
            print(f"    x={e['x']} y={e['y']} size={e['w']}x{e['h']} "
                  f"tile={e['tile']} bank={e['bank']}")
    print('UNVERIFIED: compare header/stride and tile indices against pinned mzm source.')


if __name__ == '__main__':
    main()
