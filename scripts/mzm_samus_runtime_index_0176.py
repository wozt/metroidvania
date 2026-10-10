# SPDX-License-Identifier: GPL-3.0-only
"""Validate local animation manifest and emit a safe, tab-delimited SDL runtime index."""
import argparse
import json
from pathlib import Path


def build(root):
    base = root / "assets/extracted"
    manifest = base / "samus_runtime_library_0175/manifest.json"
    data = json.loads(manifest.read_text(encoding="utf-8"))
    if data.get("schema") != "metroidvania-mzm-samus-runtime-library-v1":
        raise ValueError("unexpected catalogue schema")
    lines = []
    for key, record in sorted(data["sequences"].items()):
        if not key.replace("/", "").replace("_", "").isalnum():
            raise ValueError("unsafe animation name")
        frames = record["frames"]
        if not 1 <= len(frames) <= 256:
            raise ValueError("invalid frame count")
        for i, frame in enumerate(frames):
            rel = frame["bmp"]
            tick = frame["duration_ticks"]
            if frame["index"] != i or type(tick) is not int or not 1 <= tick <= 255:
                raise ValueError("invalid frame timing")
            if not isinstance(rel, str) or any(c in rel for c in "\t\r\n"):
                raise ValueError("invalid BMP path")
            candidate = root / rel
            if candidate.is_symlink() or not candidate.is_file() or not candidate.resolve().is_relative_to(base):
                raise ValueError("BMP is missing or unsafe: " + rel)
            lines.append(f"{key}\t{i}\t{tick}\t{rel}")
    return "\n".join(lines) + "\n", len(data["sequences"])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    target = root / "assets/extracted/samus_runtime_library_0175/runtime_index.tsv"
    if target.is_symlink():
        p.error("symlink destination refused")
    try:
        content, count = build(root)
        if not target.exists() or target.read_text(encoding="utf-8") != content:
            target.write_text(content, encoding="utf-8")
    except (OSError, ValueError, KeyError, TypeError) as exc:
        p.error(str(exc))
    print(f"Indexed {count} sequences for SDL3: {target}")


if __name__ == "__main__":
    main()
