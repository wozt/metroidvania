# SPDX-License-Identifier: GPL-3.0-only
"""Create a conservative, ROM-verified taxonomy of MZM arm cannon assets.

A graphic's descriptive symbol is not proof that it belongs to a specific body
animation. Mappings remain symbolic until verified by native pointer tables.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from scripts.mzm_samus_frame import EXPECTED_SHA1
from scripts.mzm_samus_cannon_0156 import parse_cannon_symbols, validate_private_output
from scripts.asset_layout import METROID_SAMUS_DIAGNOSTICS

GFX = re.compile(r'^sArmCannonGfx_(Upper|Lower)_(.+)$')
DIRECTIONS = ('DiagonalDown', 'DiagonalUp', 'Forward', 'Down', 'Up')
SIDES = ('Left', 'Right')


def classify(name):
    match = GFX.fullmatch(name)
    if not match:
        return {'kind': 'other', 'classification': 'unknown'}
    part, tail = match.groups()
    tokens = tail.split('_')
    direction = next((t for t in tokens if t in DIRECTIONS), None)
    side = next((t for t in tokens if t in SIDES), None)
    armed = 'Armed' in tokens
    return {'kind': 'graphics', 'part': part.lower(),
            'direction': direction or 'unspecified',
            'facing': side or 'unspecified',
            'armed': armed, 'variant_tokens': [t for t in tokens
                if t not in DIRECTIONS and t not in SIDES and t != 'Armed'],
            'classification': 'from-native-symbol-name-only'}


def generate(symbols, previous):
    if previous.get('schema') != 'metroidvania-mzm-samus-cannon-v1':
        raise ValueError('unexpected cannon manifest schema')
    verified = previous.get('verified_compositions', {})
    if not isinstance(verified, dict):
        raise ValueError('malformed verified compositions')
    resources = {}
    groups = {}
    for name, record in sorted(symbols.items()):
        entry = dict(record)
        entry.update(classify(name))
        resources[name] = entry
        if entry['kind'] == 'graphics':
            key = (entry['direction'], entry['facing'], entry['armed'],
                   tuple(entry['variant_tokens']))
            label = '/'.join([key[0], key[1], 'armed' if key[2] else 'default',
                              '_'.join(key[3]) or 'base'])
            group = groups.setdefault(label, {'direction': key[0], 'facing': key[1],
                'armed': key[2], 'variant_tokens': list(key[3]), 'parts': {}})
            part = entry['part']
            if part in group['parts']:
                group.setdefault('conflicts', []).append(name)
            else:
                group['parts'][part] = name
    return {
        'schema': 'metroidvania-mzm-samus-cannon-taxonomy-v1',
        'rom_sha1': EXPECTED_SHA1,
        'source_manifest': (METROID_SAMUS_DIAGNOSTICS / 'cannon/manifest.json').as_posix(),
        'resources': resources, 'graphics_groups': groups,
        'verified_sequences': {
            name: {'frame_count': len(info.get('frames', [])),
                   'status': 'composed-by-0156'}
            for name, info in sorted(verified.items())},
        'body_animation_links': {},
        'mapping_status': 'No new body-to-cannon animation mapping established',
        'limits': ['graphics classification derived from descriptive symbols only',
                   'graphics groups are NOT playable animation sequences',
                   'no inferred OAM, offsets, layering or timing',
                   'original 0156 compositions remain unchanged']
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom', type=Path, required=True)
    ap.add_argument('--elf', type=Path, default=Path('third_party/mzm/mzm_us.elf'))
    ap.add_argument('--cannon-manifest', type=Path,
                    default=METROID_SAMUS_DIAGNOSTICS / 'cannon/manifest.json')
    ap.add_argument('--output', type=Path,
                    default=METROID_SAMUS_DIAGNOSTICS / 'cannon/taxonomy.json')
    a = ap.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    output = a.output.absolute()
    if output.suffix != '.json':
        ap.error('output must be JSON')
    try:
        validate_private_output(root, output)
        rom = a.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError('ROM SHA-1 mismatch')
        previous = json.loads(a.cannon_manifest.read_text(encoding='utf-8'))
        nm = subprocess.run(['arm-none-eabi-nm', '-S', '--defined-only', str(a.elf)],
                            check=True, capture_output=True, text=True)
        symbols = parse_cannon_symbols(nm.stdout, len(rom))
        if not symbols:
            raise ValueError('no native cannon symbols')
        result = generate(symbols, previous)
        output.parent.mkdir(parents=True, exist_ok=True)
        payload = json.dumps(result, indent=2, sort_keys=True) + '\n'
        if not output.exists() or output.read_text(encoding='utf-8') != payload:
            output.write_text(payload, encoding='utf-8')
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        ap.error(str(exc))
    print(f'Classified {len(result["resources"])} native symbols into '
          f'{len(result["graphics_groups"])} graphics groups.')
    print(f'Retained {len(result["verified_sequences"])} verified compositions; '
          'no unverified animations composed.')
    print('Taxonomy:', output)


if __name__ == '__main__':
    main()
