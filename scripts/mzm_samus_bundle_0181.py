# SPDX-License-Identifier: GPL-3.0-only
"""Build an independent, deduplicated, private Samus runtime asset bundle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil

from scripts.mzm_samus_runtime_library_0175 import build

SUITS = ("PowerSuit", "VariaSuit", "GravitySuit", "FullSuit", "Suitless")


def safe_asset_path(root, rel):
    if not isinstance(rel, str) or not rel.startswith("assets/extracted/"):
        raise ValueError("asset path is not private")
    path = root / rel
    base = root / "assets/extracted"
    if not path.resolve().is_relative_to(base.resolve()):
        raise ValueError("asset escapes private root")
    if path.is_symlink() or not path.is_file():
        raise ValueError("missing or symlink source: " + str(path))
    return path


def produce(root):
    root = root.resolve()
    base = root / "assets/extracted"
    dest = base / "samus"
    if base.is_symlink() or dest.is_symlink():
        raise ValueError("private assets cannot be symlinks")
    catalogue = build(root, strict=True)
    dest.mkdir(parents=True, exist_ok=True)
    (dest / "objects").mkdir(exist_ok=True)
    rows = []
    packed = {}
    for key, entry in sorted(catalogue["sequences"].items()):
        if len(key) >= 160 or any(c in key for c in "\t\r\n"):
            raise ValueError("invalid animation key")
        for frame in entry["frames"]:
            src = safe_asset_path(root, frame["bmp"])
            blob = src.read_bytes()
            if blob[:2] != b"BM":
                raise ValueError("invalid BMP header: " + str(src))
            digest = hashlib.sha256(blob).hexdigest()
            output = dest / "objects" / (digest + ".bmp")
            if output.is_symlink():
                raise ValueError("symlink asset destination")
            if output.exists():
                if output.read_bytes() != blob:
                    raise ValueError("object hash collision")
            else:
                # Atomic creation avoids leaving half-written BMP files.
                temporary = output.with_suffix(".tmp")
                if temporary.is_symlink():
                    raise ValueError("symlink temporary output")
                temporary.write_bytes(blob)
                os.replace(temporary, output)
            packed[digest] = len(blob)
            rel = output.relative_to(root).as_posix()
            if len(rel) >= 320:
                raise ValueError("runtime BMP path too long")
            rows.append(f"{key}\t{frame['index']}\t{frame['duration_ticks']}\t{rel}")
    index = dest / "runtime_index.tsv"
    manifest = dest / "manifest.json"
    for p in (index, manifest):
        if p.is_symlink():
            raise ValueError("symlink output refused")
    metadata = {
        "schema": "metroidvania-mzm-samus-bundle-v1",
        "source_schema": catalogue["schema"],
        "suits": catalogue["suits"],
        "sequences": len(catalogue["sequences"]),
        "unique_bmps": len(packed),
        "sources": catalogue["sources"],
        "note": "No original ROM data is redistributed; local-only private derived sprites.",
    }
    expected = ((index, "\n".join(rows) + "\n"),
                (manifest, json.dumps(metadata, indent=2, sort_keys=True) + "\n"))
    for path, text in expected:
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            path.write_text(text, encoding="utf-8")
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        result = produce(root)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.error(str(exc))
    print("Samus bundle:", result["sequences"], "sequences,",
          result["unique_bmps"], "unique BMPs")
    print("Index:", root / "assets/extracted/samus/runtime_index.tsv")


if __name__ == "__main__":
    main()
