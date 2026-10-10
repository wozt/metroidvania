# SPDX-License-Identifier: GPL-3.0-only
"""Create private simple timing sidecars for experimental SDL3 compositions."""
import argparse
import json
from pathlib import Path

NAMES = ('run_diagonal_up_right', 'midair_forward_right', 'shoot_crouch_right')
MAX_COUNTS = {'run_diagonal_up_right': 10, 'midair_forward_right': 5, 'shoot_crouch_right': 3}


def validate(manifest, base):
    if manifest.get('schema') != 'metroidvania-mzm-samus-compositions-v1':
        raise ValueError('unexpected composition manifest')
    output = {}
    for name in NAMES:
        record = manifest.get('sequences', {}).get(name)
        if not record or record.get('status') != 'diagnostic-composed':
            raise ValueError('composition unavailable: ' + name)
        rows = record.get('frames', [])
        if len(rows) != MAX_COUNTS[name]:
            raise ValueError('unexpected frame count: ' + name)
        values = []
        for index, row in enumerate(rows):
            if row.get('index') != index or type(row.get('duration_ticks')) is not int:
                raise ValueError('frame order/timing invalid: ' + name)
            duration = row['duration_ticks']
            if not 1 <= duration <= 255:
                raise ValueError('invalid duration: ' + name)
            expected = f'composed/{name}/{index:03d}.bmp'
            if row.get('bmp') != expected:
                raise ValueError('unexpected BMP path: ' + name)
            bmp = base / expected
            if bmp.is_symlink() or not bmp.is_file():
                raise ValueError('missing/redirected BMP: ' + str(bmp))
            values.append(duration)
        output[name] = values
    return output


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--directory', type=Path,
                   default=Path('assets/extracted/samus_compositions_0160'))
    args = p.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    allowed = root / 'assets' / 'extracted'
    dest = args.directory.absolute()
    if not allowed.is_dir() or allowed.resolve() != allowed or allowed not in dest.parents:
        p.error('directory must be under private assets/extracted')
    if any(x.is_symlink() for x in (dest, *dest.parents)
           if x == allowed or allowed in x.parents):
        p.error('symlink output refused')
    try:
        manifest = json.loads((dest/'manifest.json').read_text(encoding='utf-8'))
        timings = validate(manifest, dest)
        for name, durations in timings.items():
            sidecar = dest / 'composed' / name / 'durations.txt'
            content = ''.join(f'{d}\n' for d in durations)
            if not sidecar.exists() or sidecar.read_text(encoding='ascii') != content:
                sidecar.write_text(content, encoding='ascii')
    except (ValueError, OSError, KeyError, TypeError) as exc:
        p.error(str(exc))
    print('Prepared native-timing sidecars for', len(timings), 'composed sequences.')


if __name__ == '__main__':
    main()
