#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Patch 014: ROM-local discovery of candidate Zero Mission Samus frame tables."""
from pathlib import Path
from textwrap import dedent

root = Path.cwd()
assert (root / 'scripts/mzm_samus_frame.py').exists(), 'Run from the metroidvania repository root'
files = {
'scripts/mzm_samus_scan.py': '''#!/usr/bin/env python3
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
''',
'tests/test_mzm_samus_scan.py': '''# SPDX-License-Identifier: GPL-3.0-only
import struct
import unittest
from scripts.mzm_samus_scan import scan_tables, valid_frame

BASE = 0x08000000


def fixture():
    rom = bytearray(0x2000)
    # A ROM-local 4bpp graphics bundle, 1+1 tiles, repeated in synthetic frames.
    rom[0x500:0x502] = bytes((1, 1))
    rom[0x502:0x542] = bytes([0x11]) * 64
    for i in range(4):
        off = 0x800 + i * 16
        struct.pack_into('<III', rom, off, BASE + 0x500, BASE + 0x500, BASE + 0x600)
        rom[off + 12] = i + 2
        struct.pack_into('<I', rom, 0x100 + i * 4, BASE + off)
    return rom


class ScannerTest(unittest.TestCase):
    def test_find_four_frames(self):
        tables = scan_tables(fixture(), min_frames=3)
        self.assertEqual(len(tables), 1)
        self.assertEqual(tables[0]['table_offset'], 0x100)
        self.assertEqual(tables[0]['frames'], 4)

    def test_reject_invalid_frame(self):
        rom = fixture()
        self.assertFalse(valid_frame(rom, BASE + 0x900))
        rom[0x80C] = 0
        self.assertFalse(valid_frame(rom, BASE + 0x800))

    def test_reject_invalid_limits(self):
        with self.assertRaises(ValueError):
            scan_tables(fixture(), 5, 2)


if __name__ == '__main__':
    unittest.main()
''',
'docs/MZM_ANIMATION_DISCOVERY.md': '''# MZM Samus animation pointer discovery (local research)

`scripts/mzm_samus_scan.py` checks the USA ROM SHA-1 and scans ROM-aligned
pointer arrays for valid-looking `SamusAnimationData` records (two graphics
pointers, one OAM pointer, one duration). It also checks both graphics bundles
and capacity constraints. Results are **candidates**, not identified poses.

```sh
python3 scripts/mzm_samus_scan.py \\
  --rom 'roms/Metroid - Zero Mission (USA).gba' \\
  --min-frames 3 --max-results 30
```

For the candidate's **first frame pointer**, stage the graphics via the existing
`mzm_samus_frame.py` utility. Confirm the relevant pointers against the pinned
`third_party/mzm/src/data/samus/samus_animation_pointers.c` tables and, if
necessary, an ELF/map generated from the decompilation. The OAM format, suit
palette, gun barrel and animation names are not inferred by the scanner.

Scanner output contains numeric addresses only. Do not commit extracted assets.
'''
}
for name,content in files.items():
    path=root/name
    if path.exists():
        raise SystemExit(f'Already exists: {path}')
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(content)
status=root/'docs/PROJECT_STATUS.md'
s=status.read_text()
append='''\n## Patch 014 - MZM Samus animation table discovery\n\n- Added a ROM-hash-gated candidate scanner for arrays pointing to plausible\n  Samus animation frames, plus synthetic regression tests.\n- The output is address-only research metadata: no graphics or ROM dumps.\n- Candidate tables must be matched to upstream symbols before assigning poses.\n- Next: verify the chosen pointer chains and decode game-specific OAM.\n'''
if '## Patch 014 -' not in s: status.write_text(s.rstrip()+'\n'+append)
print('Patch 014 applied: 3 new files and project status updated.')
