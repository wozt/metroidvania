#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the canonical deduplicated private Samus runtime library.

The current discovery adapter reads validated manifests produced by the native
MZM decoders. Output names are stable and contain no patch sequence numbers.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.asset_layout import EXTRACTED, METROID_SAMUS_RUNTIME, private_path
from scripts.mzm_samus_runtime_library_0175 import build

SUITS = ("PowerSuit", "VariaSuit", "GravitySuit", "FullSuit", "Suitless")


def safe_asset_path(root: Path, relative: str) -> Path:
    if not isinstance(relative, str) or not relative.startswith(
            EXTRACTED.as_posix() + "/"):
        raise ValueError("asset path is not private")
    path = root / relative
    base = root / EXTRACTED
    if not path.resolve().is_relative_to(base.resolve()):
        raise ValueError("asset escapes private root")
    current = root
    for part in Path(relative).parts:
        current /= part
        if current.is_symlink():
            raise ValueError("symlink source refused: " + str(current))
    if not path.is_file():
        raise ValueError("missing source: " + str(path))
    return path


def _write_atomic(path: Path, content: str) -> None:
    if path.is_symlink():
        raise ValueError("symlink output refused")
    temporary = path.with_suffix(path.suffix + ".tmp")
    if temporary.is_symlink():
        raise ValueError("symlink temporary output refused")
    temporary.write_text(content, encoding="utf-8")
    os.replace(temporary, path)


def produce(root: Path) -> dict:
    root = Path(root).resolve()
    destination = private_path(root, METROID_SAMUS_RUNTIME, create=True)
    if destination.is_symlink():
        raise ValueError("private Samus runtime cannot be a symlink")
    catalogue = build(root, strict=True)
    destination.mkdir(exist_ok=True)
    objects = destination / "objects"
    if objects.is_symlink():
        raise ValueError("private Samus object store cannot be a symlink")
    objects.mkdir(exist_ok=True)
    rows = []
    packed: dict[str, int] = {}
    for key, entry in sorted(catalogue["sequences"].items()):
        if len(key) >= 160 or any(character in key for character in "\t\r\n"):
            raise ValueError("invalid animation key")
        for frame in entry["frames"]:
            source = safe_asset_path(root, frame["bmp"])
            blob = source.read_bytes()
            if blob[:2] != b"BM":
                raise ValueError("invalid BMP header: " + str(source))
            digest = hashlib.sha256(blob).hexdigest()
            output = objects / f"{digest}.bmp"
            if output.is_symlink():
                raise ValueError("symlink asset destination")
            if output.exists():
                if output.read_bytes() != blob:
                    raise ValueError("object hash collision")
            else:
                temporary = output.with_suffix(".tmp")
                if temporary.is_symlink():
                    raise ValueError("symlink temporary output")
                temporary.write_bytes(blob)
                os.replace(temporary, output)
            packed[digest] = len(blob)
            relative = output.relative_to(root).as_posix()
            if len(relative) >= 320:
                raise ValueError("runtime BMP path too long")
            rows.append(
                f"{key}\t{frame['index']}\t{frame['duration_ticks']}\t{relative}")
    metadata = {
        "schema": "metroidvania-mzm-samus-runtime-v2",
        "source_schema": catalogue["schema"],
        "suits": catalogue["suits"],
        "sequences": len(catalogue["sequences"]),
        "unique_bmps": len(packed),
        "sources": catalogue["sources"],
        "object_store": "sha256",
        "note": "Private native extraction; source ROM data is never redistributed.",
    }
    expected = (
        (destination / "runtime_index.tsv", "\n".join(rows) + "\n"),
        (destination / "manifest.json",
         json.dumps(metadata, indent=2, sort_keys=True) + "\n"),
    )
    for path, content in expected:
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            _write_atomic(path, content)
    return metadata


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=ROOT)
    args = parser.parse_args(argv)
    try:
        result = produce(args.root)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.error(str(exc))
    print("Samus runtime library:", result["sequences"], "sequences,",
          result["unique_bmps"], "unique BMPs")
    print("Index:", args.root.resolve() / METROID_SAMUS_RUNTIME / "runtime_index.tsv")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
