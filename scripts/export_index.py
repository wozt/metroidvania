# SPDX-License-Identifier: GPL-3.0-only
"""Integrity indexes for private multi-file exports.

An export that writes many files (one folder per room, for instance) also
writes ``export_index.tsv`` next to them:

    schema<TAB>metroidvania-export-index-v1
    file<TAB><path relative to the index folder><TAB><sha256>
    skipped<TAB><item><TAB><reason>

``scripts.rebuild`` declares the index as the task output and calls
``problems`` to decide whether the export is still intact. Sprite libraries
(``scripts.sprite_library``) need no extra index: their objects are named by
their SHA-256, which ``sprite_library_problems`` checks.
"""
from __future__ import annotations

import hashlib
from pathlib import Path

from scripts.sprite_library import INDEX_SCHEMA as SPRITE_INDEX_SCHEMA, write_atomic

SCHEMA = "metroidvania-export-index-v1"
NAME = "export_index.tsv"


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(folder: Path, files: list[Path], skipped: list[tuple[str, str]] = ()) -> Path:
    """Index `files` (inside `folder`) and the skipped items, sorted."""
    folder = Path(folder)
    lines = [f"schema\t{SCHEMA}"]
    for path in sorted(files):
        relative = Path(path).relative_to(folder).as_posix()
        if "\t" in relative or "\n" in relative:
            raise ValueError(f"unsupported export path: {relative}")
        lines.append(f"file\t{relative}\t{_sha256(Path(path))}")
    for item, reason in sorted(skipped):
        lines.append(f"skipped\t{item}\t{' '.join(reason.split())}")
    index = folder / NAME
    write_atomic(index, "\n".join(lines) + "\n")
    return index


def problems(index: Path) -> list[str]:
    """Missing or modified files listed by an export index."""
    index = Path(index)
    if not index.is_file():
        return [f"missing index: {index}"]
    lines = index.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != f"schema\t{SCHEMA}":
        return [f"unsupported export index: {index}"]
    found = []
    for line in lines[1:]:
        fields = line.split("\t")
        if fields[0] == "skipped" and len(fields) == 3:
            continue
        if fields[0] != "file" or len(fields) != 3:
            found.append(f"malformed export index line: {line!r}")
            continue
        path = index.parent / fields[1]
        if not path.is_file():
            found.append(f"missing export file: {path}")
        elif _sha256(path) != fields[2]:
            found.append(f"modified export file: {path}")
    return found


def sprite_library_problems(index: Path) -> list[str]:
    """Missing or modified objects of a content-addressed sprite library."""
    index = Path(index)
    if not index.is_file():
        return [f"missing sprite index: {index}"]
    lines = index.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != f"schema\t{SPRITE_INDEX_SCHEMA}":
        return [f"unsupported sprite index: {index}"]
    found = []
    checked = set()
    for line in lines[1:]:
        fields = line.split("\t")
        if len(fields) != 6:
            found.append(f"malformed sprite index line: {line!r}")
            continue
        if fields[5] in checked:
            continue
        checked.add(fields[5])
        path = Path(fields[5])
        if not path.is_absolute():
            path = _project_root(index) / path
        if not path.is_file():
            found.append(f"missing sprite object: {path}")
        elif _sha256(path) != path.stem:
            found.append(f"modified sprite object: {path}")
    return found


def _project_root(index: Path) -> Path:
    """The folder that holds ``assets/extracted`` above an index."""
    for parent in index.resolve().parents:
        if (parent / "assets" / "extracted").is_dir():
            return parent
    return index.resolve().parent
