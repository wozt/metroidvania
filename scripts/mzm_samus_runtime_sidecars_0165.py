# SPDX-License-Identifier: GPL-3.0-only
"""Generate validated timing sidecars for the five additional native compositions."""
import argparse
import json
from pathlib import Path
from scripts.mzm_samus_runtime_sidecars_0163 import validate as validate_base
from scripts.asset_layout import METROID_SAMUS_DIAGNOSTICS

NAMES = ("run_diagonal_down_right", "run_diagonal_up_left", "shoot_standing_right",
         "midair_diagonal_up_right", "shoot_crouch_diagonal_up_right")
COUNTS = (10, 10, 3, 5, 3)


def validate(manifest, directory):
    if manifest.get("schema") != "metroidvania-mzm-samus-compositions-extension-v1":
        raise ValueError("invalid composition extension manifest")
    output = {}
    for name, count in zip(NAMES, COUNTS):
        row = manifest.get("sequences", {}).get(name)
        if not row or row.get("status") != "diagnostic-composed":
            raise ValueError("unavailable sequence: " + name)
        frames = row.get("frames")
        if not isinstance(frames, list) or len(frames) != count:
            raise ValueError("invalid sequence count: " + name)
        durations = []
        for index, frame in enumerate(frames):
            expected = "composed/%s/%03d.bmp" % (name, index)
            duration = frame.get("duration_ticks")
            if frame.get("index") != index or frame.get("bmp") != expected:
                raise ValueError("invalid native frame ordering/path: " + name)
            if type(duration) is not int or not 1 <= duration <= 255:
                raise ValueError("invalid frame duration: " + name)
            image = directory / expected
            if image.is_symlink() or not image.is_file():
                raise ValueError("missing private frame: " + str(image))
            durations.append(duration)
        output[name] = durations
    return output


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path,
                        default=METROID_SAMUS_DIAGNOSTICS / "compositions/extended")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    allowed = root / "assets/extracted"
    dest = args.directory.absolute()
    if not allowed.is_dir() or allowed.resolve() != allowed or allowed not in dest.parents:
        parser.error("must target private assets/extracted")
    if any(p.is_symlink() for p in (dest, *dest.parents)
           if p == allowed or allowed in p.parents):
        parser.error("symlink destination refused")
    try:
        manifest = json.loads((dest / "manifest.json").read_text(encoding="utf-8"))
        data = validate(manifest, dest)
        for name, durations in data.items():
            file = dest / "composed" / name / "durations.txt"
            if file.is_symlink():
                raise ValueError("symlink sidecar refused: " + str(file))
            content = "".join(str(i) + "\n" for i in durations)
            if not file.exists() or file.read_text(encoding="ascii") != content:
                file.write_text(content, encoding="ascii")
    except (OSError, KeyError, ValueError, TypeError) as exc:
        parser.error(str(exc))
    print("Prepared timing sidecars for", len(data), "extension animations.")

if __name__ == "__main__":
    main()
