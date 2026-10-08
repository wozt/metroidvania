#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Identify plausible MZM SamusAnimationData sequences in a verified personal ROM.

This is a discovery tool, not proof of an animation identity. Every hit needs
cross-checking against mzm symbols/disassembly. No copyrighted bytes exported.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

try:
    from scripts.mzm_samus_frame import EXPECTED_SHA1, ROM_BASE, rom_offset, parse_frame, read_block
except ModuleNotFoundError:
    from mzm_samus_frame import EXPECTED_SHA1, ROM_BASE, rom_offset, parse_frame, read_block


def valid_frame(rom, address):
    try:
        upper, lower, oam, duration = parse_frame(rom, address)
        if not 1 <= duration <= 120:
            return False
        for ptr in (upper, lower):
            first, second = read_block(rom, ptr)
            if not first and not second:
                return False
            if len(first) > 0x280 or len(second) > 0x280:
                return False
        # Samus OAM is a counted structure, not necessarily an 8-byte OAM array.
        # Only require the pointer to be in range here.
        rom_offset(oam, rom)
        return True
    except (ValueError, struct.error):
        return False


def scan_tables(rom, min_frames=3, max_frames=32):
    """Find aligned 32-bit pointer arrays to plausible 13-byte animation records."""
    if not 2 <= min_frames <= max_frames <= 128:
        raise ValueError('invalid frame limits')
    matches = []
    # Sequential frame records are often contiguous (13 bytes per record),
    # but this scanner instead searches explicit pointer arrays to avoid assumptions.
    limit = len(rom) - min_frames * 4
    pos = 0
    while pos <= limit:
        first = struct.unpack_from('<I', rom, pos)[0]
        if not (ROM_BASE <= first < ROM_BASE + len(rom)) or not valid_frame(rom, first):
            pos += 4
            continue
        values = []
        while len(values) < max_frames and pos + 4 * (len(values) + 1) <= len(rom):
            ptr = struct.unpack_from('<I', rom, pos + len(values) * 4)[0]
            if not valid_frame(rom, ptr):
                break
            values.append(ptr)
        if len(values) >= min_frames and len(set(values)) >= 2:
            matches.append({'table_offset': pos, 'table_address': ROM_BASE + pos,
                            'frames': len(values), 'frame_pointers': values})
            pos += len(values) * 4
        else:
            pos += 4
    return matches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--min-frames', type=int, default=3)
    parser.add_argument('--max-frames', type=int, default=24)
    parser.add_argument('--max-results', type=int, default=40)
    parser.add_argument('--json', action='store_true', help='Print only metadata; no ROM bytes')
    a = parser.parse_args()
    if not 1 <= a.max_results <= 1000:
        parser.error('--max-results must be 1..1000')
    data = a.rom.read_bytes()
    if hashlib.sha1(data).hexdigest() != EXPECTED_SHA1:
        parser.error('ROM SHA-1 mismatch (Zero Mission USA expected)')
    try:
        found = scan_tables(data, a.min_frames, a.max_frames)
    except ValueError as exc:
        parser.error(str(exc))
    found.sort(key=lambda m: (-m['frames'], m['table_offset']))
    result = found[:a.max_results]
    if a.json:
        print(json.dumps(result, indent=2))
    else:
        print(f'Found {len(found)} candidate pointer tables; showing {len(result)}.')
        for item in result:
            print(f"table=0x{item['table_address']:08X} frames={item['frames']} "
                  f"first=0x{item['frame_pointers'][0]:08X}")
        print('UNVERIFIED: candidates are not labeled by pose. Validate against mzm.')


if __name__ == '__main__':
    main()
