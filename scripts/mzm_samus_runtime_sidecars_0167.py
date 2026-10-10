# SPDX-License-Identifier: GPL-3.0-only
"""Create private timing sidecars for the six left-facing compositions."""
import argparse
import json
from pathlib import Path

COUNTS = {"midair_forward_left": 5, "midair_diagonal_up_left": 5,
          "shoot_standing_left": 3, "shoot_crouch_left": 3,
          "shoot_crouch_diagonal_up_left": 3, "run_diagonal_down_left": 10}


def validate(manifest, folder):
    if manifest.get("schema") != "metroidvania-mzm-samus-compositions-left-v1":
        raise ValueError("unexpected manifest schema")
    output = {}
    for name, count in COUNTS.items():
        sequence = manifest.get("sequences", {}).get(name, {})
        rows = sequence.get("frames", [])
        if sequence.get("status") != "diagnostic-composed" or len(rows) != count:
            raise ValueError("missing sequence: " + name)
        durations = []
        for index, row in enumerate(rows):
            path = folder / "composed" / name / f"{index:03d}.bmp"
            duration = row.get("duration_ticks")
            if (row.get("index") != index or
                row.get("bmp") != f"composed/{name}/{index:03d}.bmp" or
                type(duration) is not int or not 1 <= duration <= 255 or
                path.is_symlink() or not path.is_file()):
                raise ValueError("invalid native frame: " + name)
            durations.append(duration)
        output[name] = durations
    return output


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path,
                        default=Path("assets/extracted/samus_compositions_0166"))
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    allowed = root / "assets/extracted"
    folder = args.directory.absolute()
    if allowed.resolve() != allowed or allowed not in folder.parents or any(
            path.is_symlink() for path in (folder, *folder.parents)
            if path == allowed or allowed in path.parents):
        parser.error("private directory required, no symlinks")
    try:
        manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
        values = validate(manifest, folder)
        for name, durations in values.items():
            dest = folder / "composed" / name / "durations.txt"
            if dest.is_symlink():
                raise ValueError("symlink sidecar refused")
            data = "".join(str(n) + "\n" for n in durations)
            if not dest.exists() or dest.read_text(encoding="ascii") != data:
                dest.write_text(data, encoding="ascii")
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.error(str(exc))
    print("Prepared left-facing timing sidecars:", len(values))


if __name__ == "__main__":
    main()
