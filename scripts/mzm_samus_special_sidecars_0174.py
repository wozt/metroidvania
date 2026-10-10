# SPDX-License-Identifier: GPL-3.0-only
"""Validate and prepare native special animation timing sidecars."""
import argparse
import json
from pathlib import Path

NAMES = ("spinning_right", "spinning_left",
         "spacejumping_right", "spacejumping_left",
         "screwattacking_right", "screwattacking_left")


def prepare(directory):
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != "metroidvania-mzm-samus-special-body-only-v1":
        raise ValueError("unexpected special animation manifest")
    for name in NAMES:
        entry = manifest["sequences"][name]
        if entry.get("status") != "body-only-diagnostic-composed":
            raise ValueError("missing composed sequence " + name)
        frames = entry["frames"]
        if len(frames) != 8:
            raise ValueError("unexpected frame count for " + name)
        ticks = []
        for i, frame in enumerate(frames):
            rel = f"composed/{name}/{i:03d}.bmp"
            file = directory / rel
            if frame["index"] != i or frame["bmp"] != rel or file.is_symlink() or not file.is_file():
                raise ValueError("invalid frame " + rel)
            tick = frame["duration_ticks"]
            if type(tick) is not int or not 1 <= tick <= 255:
                raise ValueError("invalid duration " + rel)
            ticks.append(str(tick))
        output = directory / "composed" / name / "durations.txt"
        if output.is_symlink():
            raise ValueError("symlink sidecar refused")
        content = "\n".join(ticks) + "\n"
        if not output.exists() or output.read_text(encoding="ascii") != content:
            output.write_text(content, encoding="ascii")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--directory", type=Path,
                   default=Path("assets/extracted/samus_special_0173"))
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    from scripts.mzm_samus_compositions_0160 import safe_dir
    relative = args.directory
    if relative.is_absolute():
        try:
            relative = relative.relative_to(root)
        except ValueError:
            p.error("directory must be inside project")
    try:
        directory = safe_dir(root, relative)
        prepare(directory)
    except (OSError, ValueError, KeyError, TypeError) as e:
        p.error(str(e))
    print("Prepared 6 native special animations (48 frames)")


if __name__ == "__main__":
    main()
