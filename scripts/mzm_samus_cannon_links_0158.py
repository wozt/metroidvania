# SPDX-License-Identifier: GPL-3.0-only
"""Cross-reference native body animations and arm cannon animation tables.

Symbol/name correspondence is only a candidate. No unverified sprite composition.
"""
import argparse
import hashlib
import json
from pathlib import Path

from scripts.asset_layout import METROID_SAMUS_BODY_SOURCE, METROID_SAMUS_DIAGNOSTICS
import re
import subprocess
from scripts.mzm_samus_frame import EXPECTED_SHA1

BODY = re.compile(r'^sSamusAnim_(PowerSuit|FullSuit|Suitless)_(.+)$')
CANNON = re.compile(r'^sArmCannonAnim_(Suit|Suitless)_(.+)$')
NM = re.compile(r'^([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s+([RrDd])\s+(sArmCannonAnim_[A-Za-z0-9_]+)$')
DIRECTIONS = ('Forward', 'DiagonalUp', 'DiagonalDown', 'Up', 'Down', 'None')


def cannon_symbols(lines):
    found = {}
    for line in lines.splitlines():
        m = NM.fullmatch(line.strip())
        if not m:
            continue
        addr, size, _, name = m.groups()
        addr, size = int(addr, 16), int(size, 16)
        if size < 8 or size % 8 or not 0x08000000 <= addr < 0x0E000000:
            raise ValueError('invalid native arm cannon animation array: ' + name)
        item = {'address': f'0x{addr:08x}', 'frames_in_elf': size // 8}
        if name in found and found[name] != item:
            raise ValueError('conflicting cannon symbol: ' + name)
        found[name] = item
    return found


def body_family(name):
    match = BODY.fullmatch(name)
    if not match:
        return None
    return match.group(1), match.group(2)


def candidates(body_name, symbols):
    pair = body_family(body_name)
    if pair is None:
        return []
    suit, tail = pair
    prefix = 'Suitless' if suit == 'Suitless' else 'Suit'
    # This matches only exact symbolic suffixes; no guessing of pose aliases.
    result = []
    for name, meta in symbols.items():
        match = CANNON.fullmatch(name)
        if not match or match.group(1) != prefix:
            continue
        suffix = match.group(2)
        if suffix == tail:
            result.append({'symbol': name, **meta, 'evidence': 'exact-symbol-suffix',
                           'status': 'candidate-not-composed'})
        elif any(suffix == tail.replace('_' + d + '_', '_') for d in DIRECTIONS):
            # Keep ambiguous aim aliases unlinked rather than pretend they are exact.
            continue
    return sorted(result, key=lambda row: row['symbol'])


def build(body_manifest, cannon_manifest, symbols):
    if body_manifest.get('schema') != 'metroidvania-mzm-samus-body-export-v1':
        raise ValueError('unsupported body manifest')
    if cannon_manifest.get('schema') != 'metroidvania-mzm-samus-cannon-v1':
        raise ValueError('unsupported cannon manifest')
    output = {'schema': 'metroidvania-mzm-samus-cannon-links-v1',
              'provenance': 'matching MZM US ELF symbols and private body/cannon manifests',
              'verified_compositions': cannon_manifest['verified_compositions'],
              'links': {}, 'unmapped': [], 'symbols': symbols,
              'warning': 'name-matched links are candidates; gameplay pose/aim and OAM pairing require verification'}
    for name, body in sorted(body_manifest['animations'].items()):
        found = candidates(name, symbols)
        row = {'body_animation': name, 'body_frame_count': len(body['frames']),
               'cannon_candidates': found,
               'status': 'exact-name-candidate-only' if found else 'unmapped'}
        output['links'][name] = row
        if not found:
            output['unmapped'].append(name)
    return output


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--elf', type=Path, default=Path('third_party/mzm/mzm_us.elf'))
    p.add_argument('--body', type=Path, default=METROID_SAMUS_BODY_SOURCE / 'manifest.json')
    p.add_argument('--cannon', type=Path,
                   default=METROID_SAMUS_DIAGNOSTICS / 'cannon/manifest.json')
    p.add_argument('--output', type=Path,
                   default=METROID_SAMUS_DIAGNOSTICS / 'cannon/links.json')
    a = p.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    allowed = root / 'assets/extracted'
    dest = a.output.absolute()
    if not allowed.is_dir() or allowed.resolve() != allowed or allowed not in dest.parents or dest.suffix != '.json':
        p.error('output must be JSON under assets/extracted')
    if any(q.is_symlink() for q in (dest, *dest.parents) if q == allowed or allowed in q.parents):
        p.error('symlink output refused')
    try:
        if hashlib.sha1(a.rom.read_bytes()).hexdigest() != EXPECTED_SHA1:
            raise ValueError('ROM SHA-1 mismatch')
        nm = subprocess.run(['arm-none-eabi-nm', '-S', '--defined-only', str(a.elf)],
                            check=True, text=True, capture_output=True)
        symbols = cannon_symbols(nm.stdout)
        if not symbols:
            raise ValueError('no sized cannon animation symbols')
        body = json.loads(a.body.read_text(encoding='utf-8'))
        cannon = json.loads(a.cannon.read_text(encoding='utf-8'))
        report = build(body, cannon, symbols)
        dest.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps(report, indent=2, sort_keys=True) + '\n'
        if not dest.exists() or dest.read_text(encoding='utf-8') != data:
            dest.write_text(data, encoding='utf-8')
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        p.error(str(exc))
    linked = len(report['links']) - len(report['unmapped'])
    print(f'Indexed {len(symbols)} cannon animation arrays; exact-name candidates for {linked}/{len(report["links"])} body animations.')
    print(f'Unmapped: {len(report["unmapped"])}. No speculative sprites composed.')
    print('Report:', dest)

if __name__ == '__main__':
    main()
