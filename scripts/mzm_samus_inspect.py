#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inspect candidate Samus pointer tables (metadata only; no copyrighted output).

An address found by the scanner is not evidence of pose identity. The OAM
format has game-specific headers; this tool intentionally reports raw OAM
words rather than pretending they are generic eight-byte hardware entries.
"""
import argparse
import hashlib
from pathlib import Path
import struct

try:
    from scripts.mzm_samus_frame import EXPECTED_SHA1, ROM_BASE, rom_offset, stage
    from scripts.mzm_samus_scan import valid_frame
except ModuleNotFoundError:
    from mzm_samus_frame import EXPECTED_SHA1, ROM_BASE, rom_offset, stage
    from mzm_samus_scan import valid_frame


def inspect_table(rom, table, max_frames=24):
    if not 1 <= max_frames <= 128:
        raise ValueError('max_frames must be 1..128')
    offset = rom_offset(table, rom)
    if offset % 4:
        raise ValueError('table pointer must be four-byte aligned')
    records = []
    for index in range(max_frames):
        pos = offset + index * 4
        if pos + 4 > len(rom):
            break
        frame = struct.unpack_from('<I', rom, pos)[0]
        if not valid_frame(rom, frame):
            break
        _, meta = stage(rom, frame)
        oam_pos = rom_offset(meta['oam_pointer'], rom)
        # Raw bytes are reported only as short diagnostics, never written.
        raw = rom[oam_pos:oam_pos + 16]
        records.append({'index': index, 'address': frame,
                        'duration': meta['duration'],
                        'tile_counts': meta['tile_counts'],
                        'upper_pointer': meta['upper_pointer'],
                        'lower_pointer': meta['lower_pointer'],
                        'oam_pointer': meta['oam_pointer'],
                        'oam_prefix': raw.hex(' ')})
    if not records:
        raise ValueError('no valid Samus frame at this table address')
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, type=Path)
    parser.add_argument('--table', required=True, type=lambda v: int(v, 0))
    parser.add_argument('--max-frames', default=24, type=int)
    args = parser.parse_args()
    data = args.rom.read_bytes()
    if hashlib.sha1(data).hexdigest() != EXPECTED_SHA1:
        parser.error('Zero Mission USA ROM SHA-1 mismatch')
    try:
        records = inspect_table(data, args.table, args.max_frames)
    except ValueError as exc:
        parser.error(str(exc))
    print(f'Table 0x{args.table:08X}: {len(records)} candidate frames')
    for row in records:
        print(f"[{row['index']:02d}] frame=0x{row['address']:08X} "
              f"duration={row['duration']:3d} tiles={row['tile_counts']} "
              f"OAM=0x{row['oam_pointer']:08X}")
        print(f"     OAM first 16 bytes: {row['oam_prefix']}")
    print('UNVERIFIED: no pose labels or decoded body OAM yet.')


if __name__ == '__main__':
    main()
