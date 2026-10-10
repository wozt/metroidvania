# SPDX-License-Identifier: GPL-3.0-only
"""Inventory native arm cannon assets and compose the four verified sequences.

All ROM-derived BMPs stay below assets/extracted; no conjectural pose mapping.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset
from scripts.asset_layout import METROID_SAMUS_DIAGNOSTICS
from scripts.mzm_samus_sprite import (
    make_power_suit_idle_right, make_power_suit_run_right,
    make_power_suit_jump_right, make_power_suit_attack_right,
    aligned_canvas_bounds, POWER_SUIT_IDLE_FRAME_COUNT,
    POWER_SUIT_RUN_FRAME_COUNT, POWER_SUIT_JUMP_FRAME_COUNT,
    POWER_SUIT_ATTACK_FRAME_COUNT,
)

SYMBOL_RE = re.compile(r"^sArmCannon[A-Za-z0-9_]*$")
NM_LINE = re.compile(r"^([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s+([a-zA-Z])\s+(\S+)$")


def parse_cannon_symbols(output, rom_length):
    result = {}
    for line in output.splitlines():
        match = NM_LINE.fullmatch(line.strip())
        if not match:
            continue
        address, size, kind, name = match.groups()
        if not SYMBOL_RE.fullmatch(name) or kind.lower() not in ('r', 'd'):
            continue
        address, size = int(address, 16), int(size, 16)
        if size <= 0 or not 0x08000000 <= address < 0x08000000 + rom_length:
            raise ValueError('invalid cannon symbol location: ' + name)
        if address - 0x08000000 + size > rom_length:
            raise ValueError('cannon symbol extends past ROM: ' + name)
        item = {'address': f'0x{address:08x}', 'size': size}
        if name in result and result[name] != item:
            raise ValueError('conflicting cannon symbol: ' + name)
        result[name] = item
    return result


def compose_verified(rom, output):
    builders = {
        'idle': (POWER_SUIT_IDLE_FRAME_COUNT, make_power_suit_idle_right),
        'run': (POWER_SUIT_RUN_FRAME_COUNT, make_power_suit_run_right),
        'jump': (POWER_SUIT_JUMP_FRAME_COUNT, make_power_suit_jump_right),
        'attack': (POWER_SUIT_ATTACK_FRAME_COUNT, make_power_suit_attack_right),
    }
    result = {}
    for name, (count, builder) in builders.items():
        # Probe all frames before writing anything for this sequence.
        probes = [builder(rom, i)[1] for i in range(count)]
        bounds = aligned_canvas_bounds(probes)
        frames = [builder(rom, i, bounds) for i in range(count)]
        rows = []
        for index, (bmp, meta) in enumerate(frames):
            relative = Path('composed') / name / f'{index:03d}.bmp'
            destination = output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if not destination.exists() or destination.read_bytes() != bmp:
                destination.write_bytes(bmp)
            rows.append({
                'index': index, 'bmp': relative.as_posix(),
                'duration_ticks': int(meta['frame']['duration']),
                'cannon_oam_parts': meta['arm_cannon']['count'] if meta['arm_cannon'] else 0,
                'muzzle_offset': list(meta['arm_cannon_animation']['muzzle_offset']),
                'body_oam_parts': meta['body_oam']['count'],
            })
        result[name] = {'frames': rows, 'canvas_bounds': list(bounds),
                        'note': 'only the original verified Power Suit right-facing sequence'}
    return result


def validate_private_output(root, dest):
    allowed = root / 'assets' / 'extracted'
    if not allowed.is_dir() or allowed.resolve() != allowed:
        raise ValueError('assets/extracted is absent or redirected')
    if allowed not in dest.parents:
        raise ValueError('output must be below assets/extracted')
    for path in (dest, *dest.parents):
        if path == allowed or allowed in path.parents:
            if path.is_symlink():
                raise ValueError('symlinked output path refused')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--elf', type=Path, default=Path('third_party/mzm/mzm_us.elf'))
    parser.add_argument('--output-dir', type=Path,
                        default=METROID_SAMUS_DIAGNOSTICS / 'cannon')
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    dest = args.output_dir.absolute()
    try:
        validate_private_output(root, dest)
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError('ROM SHA-1 mismatch')
        nm = subprocess.run(['arm-none-eabi-nm', '-S', '--defined-only', str(args.elf)],
                            capture_output=True, text=True, check=True)
        symbols = parse_cannon_symbols(nm.stdout, len(rom))
        if not symbols:
            raise ValueError('no cannon symbols in ELF')
        # Do not infer arm cannon pointers from body animation names.
        sequences = compose_verified(rom, dest)
        report = {
            'schema': 'metroidvania-mzm-samus-cannon-v1',
            'rom_sha1': EXPECTED_SHA1,
            'symbols': symbols,
            'verified_compositions': sequences,
            'unmapped': 'other cannon symbols are inventoried but not linked to body animation variants',
            'limitations': ['only four verified Power Suit right-facing sequences are composed',
                            'some sequences intentionally have no visible cannon OAM',
                            'effects and remaining orientations not exported'],
        }
        manifest = dest / 'manifest.json'
        data = json.dumps(report, indent=2, sort_keys=True) + '\n'
        if not manifest.exists() or manifest.read_text(encoding='utf-8') != data:
            manifest.write_text(data, encoding='utf-8')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.error(str(error))
    print(f'Indexed {len(symbols)} native cannon symbols.')
    print(f'Composed {sum(len(v["frames"]) for v in sequences.values())} frames '
          f'across {len(sequences)} verified animations.')
    print('Other cannon poses and effects are not yet mapped.')
    print('Manifest:', manifest)


if __name__ == '__main__':
    main()
