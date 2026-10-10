#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inventory private extracted assets and measure exact-content duplication."""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.asset_layout import EXTRACTED, SHARED_DIAGNOSTICS, private_path

SCHEMA = "metroidvania-private-asset-audit-v1"
REPORT = SHARED_DIAGNOSTICS / "asset_inventory.json"
TEXT_SUFFIXES = {
    ".c", ".h", ".md", ".py", ".cmake", ".toml", ".txt", ".json",
}
CLASSIFICATIONS = {
    "metroid": "canonical",
    "aria": "canonical",
    "shared": "canonical",
    ".editor_staging": "temporary_project_state",
    "audits": "active_legacy_path",
    "authored_rooms": "active_project_state",
    "exports": "active_project_state",
    "native_source_overlays": "active_generated_cache",
    "overrides": "active_project_state",
    "rooms": "active_legacy_path",
    "sprite_previews": "active_legacy_path",
    "world_overview": "active_legacy_path",
    "catalog.json": "active_legacy_path",
    "raw": "unreferenced_generated_cache",
    "cli_demo_room_0125": "unreferenced_generated_cache",
    "native_demo_0125": "active_generated_cache",
    "samus": "superseded_runtime_bundle",
    "samus_cannon": "legacy_samus_pipeline",
    "samus_library": "legacy_samus_pipeline",
    "samus_runtime": "legacy_samus_pipeline",
    "samus_runtime_library_0175": "legacy_samus_pipeline",
    "samus_compositions_0160": "legacy_samus_pipeline",
    "samus_compositions_0164": "legacy_samus_pipeline",
    "samus_compositions_0166": "legacy_samus_pipeline",
    "samus_compositions_0170": "legacy_samus_pipeline",
    "samus_special_0171": "legacy_samus_pipeline",
    "samus_special_0172": "legacy_samus_pipeline",
    "samus_special_0173": "legacy_samus_pipeline",
    "sprites": "legacy_samus_pipeline",
}


def _digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def _reference_counts(root: Path, names: set[str]) -> Counter:
    counts: Counter = Counter()
    excluded = {".git", "build", "assets", "legacy", "third_party"}
    for path in root.rglob("*"):
        if (not path.is_file() or any(part in excluded for part in path.parts)
                or path.suffix.lower() not in TEXT_SUFFIXES
                or path.stat().st_size > 4_000_000):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError):
            continue
        for name in names:
            if f"assets/extracted/{name}" in text:
                counts[name] += 1
    return counts


def audit(root: Path, *, hash_files: bool = True) -> dict:
    root = Path(root).resolve()
    base = private_path(root, EXTRACTED)
    if not base.is_dir():
        raise ValueError("private extraction root does not exist")
    files = []
    top_level: dict[str, dict] = {}
    for child in sorted(base.iterdir(), key=lambda item: item.name):
        if child.is_symlink():
            raise ValueError(f"symlink private asset entry refused: {child}")
        top_level[child.name] = {
            "name": child.name,
            "kind": "directory" if child.is_dir() else "file",
            "classification": CLASSIFICATIONS.get(child.name, "unclassified"),
            "files": 0,
            "bytes": 0,
        }
    for path in sorted(base.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"symlink private asset entry refused: {path}")
        if not path.is_file():
            continue
        relative = path.relative_to(base)
        if relative in (REPORT.relative_to(EXTRACTED),
                        REPORT.relative_to(EXTRACTED).with_suffix(".tmp")):
            continue
        size = path.stat().st_size
        record = top_level[relative.parts[0]]
        record["files"] += 1
        record["bytes"] += size
        files.append((path, relative.parts[0], size))
    references = _reference_counts(root, set(top_level))
    for name, record in top_level.items():
        record["reference_files"] = references[name]

    duplicates = {
        "hashed": hash_files,
        "groups": 0,
        "files": 0,
        "reclaimable_bytes": 0,
        "cross_top_level_groups": 0,
        "cross_top_level_reclaimable_bytes": 0,
    }
    if hash_files:
        groups: dict[str, list[tuple[str, int]]] = defaultdict(list)
        for path, top, size in files:
            groups[_digest(path)].append((top, size))
        for entries in groups.values():
            if len(entries) < 2:
                continue
            size = entries[0][1]
            duplicates["groups"] += 1
            duplicates["files"] += len(entries)
            duplicates["reclaimable_bytes"] += size * (len(entries) - 1)
            if len({entry[0] for entry in entries}) > 1:
                duplicates["cross_top_level_groups"] += 1
                duplicates["cross_top_level_reclaimable_bytes"] += (
                    size * (len(entries) - 1))
    return {
        "schema": SCHEMA,
        "root": EXTRACTED.as_posix(),
        "totals": {
            "files": len(files),
            "bytes": sum(item[2] for item in files),
        },
        "duplicates": duplicates,
        "top_level": list(top_level.values()),
    }


def _write_private(root: Path, report: dict) -> Path:
    target = private_path(
        root, REPORT, create=True)
    if target.is_symlink():
        raise ValueError("symlink audit output refused")
    temporary = target.with_suffix(".tmp")
    temporary.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(target)
    return target


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=ROOT)
    parser.add_argument("--no-hash", action="store_true")
    parser.add_argument("--write", action="store_true",
                        help="write the private report below shared/diagnostics")
    args = parser.parse_args(argv)
    try:
        report = audit(args.root, hash_files=not args.no_hash)
        output = _write_private(args.root, report) if args.write else None
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(json.dumps(report, indent=2, sort_keys=True))
    if output:
        print(f"Private report: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
