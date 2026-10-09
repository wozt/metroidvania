# SPDX-License-Identifier: GPL-3.0-only
"""0124: immutable, deterministic project-authored room snapshot (no ROM assets).

room.json contains validated project semantics for future engine adapters.
preview.tsv is a bounded, deliberately non-gameplay subset for SDL3 smoke tests.
Exported packages are content-addressed and never overwrite earlier snapshots.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

from scripts import project_room_entities as rooms

PACKAGE_SCHEMA = "metroidvania.project-room-package"
PACKAGE_VERSION = 1
COLLISION_IDS = {
    "solid": 1, "one_way": 2, "hazard": 3, "slope_up": 4,
    "slope_down": 5, "water": 6, "air": 7,
}


def read_saved(root: Path, world: str, area: str, room: int) -> dict:
    """Read only saved data; never substitute an editor's unsaved staging file."""
    path = rooms.path_for(root, world, area, room, 16, 16)
    rooms._check_path(path, root)
    if not path.is_file() or path.stat().st_size > 4_000_000:
        raise ValueError("room-export requires a saved project room document")
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError("invalid saved project room document") from exc
    if not isinstance(raw, dict):
        raise ValueError("invalid saved project room document")
    data = rooms.migrate(raw)
    if not isinstance(data, dict):
        raise ValueError("invalid saved project room document")
    return rooms.validate(data, world, area, room,
                          data.get("width_px"), data.get("height_px"))


def preview_bytes(doc: dict) -> bytes:
    step = doc["collision"]["resolution_px"]
    rows = ["\t".join(("MVROOM-PREVIEW", "1", doc["world"], doc["area"],
                        str(doc["room"]), str(doc["width_px"]),
                        str(doc["height_px"]), str(step)))]
    # All preview coordinates are in *pixels*, including collision rectangles.
    for cell in sorted(doc["collision"]["cells"],
                       key=lambda item: (item["y"], item["x"])):
        rows.append(f"C\t{cell['x'] * step}\t{cell['y'] * step}\t"
                    f"{step}\t{step}\t{COLLISION_IDS[cell['type']]}")
    for door in sorted(doc["doors"], key=lambda item: item["id"]):
        rows.append(f"D\t{door['x']}\t{door['y']}\t"
                    f"{door['width']}\t{door['height']}\t0")
    for entity in sorted(doc["entities"], key=lambda item: item["id"]):
        # 16px is the existing private marker footprint in both workrooms.
        rows.append(f"E\t{entity['x']}\t{entity['y']}\t16\t16\t0")
    for event in sorted(doc["events"], key=lambda item: item["id"]):
        rows.append(f"V\t{event['x']}\t{event['y']}\t"
                    f"{event['width']}\t{event['height']}\t0")
    rows.append("END")
    return ("\n".join(rows) + "\n").encode("ascii")


def _write_synced(path: Path, data: bytes) -> None:
    with path.open("xb") as out:
        out.write(data)
        out.flush()
        os.fsync(out.fileno())


def export(root: Path, doc: dict, *, dry_run: bool = False) -> dict:
    rooms.validate(doc, doc["world"], doc["area"], doc["room"],
                   doc["width_px"], doc["height_px"])
    payload = {
        "schema": PACKAGE_SCHEMA, "version": PACKAGE_VERSION,
        "engine_adapter": "unavailable", "native_assets_included": False,
        "room": doc,
    }
    # Sorting every dictionary key and removing timestamps makes bytes stable.
    full = (json.dumps(payload, sort_keys=True, ensure_ascii=False,
                       separators=(",", ":")) + "\n").encode("utf-8")
    preview = preview_bytes(doc)
    digest = hashlib.sha256(full + b"\0" + preview).hexdigest()
    parent = root / "assets" / "extracted" / "exports" / doc["world"]
    name = f"{doc['area'].lower()}_{doc['room']:03d}_{digest}"
    dest = parent / name
    result = {
        "format": PACKAGE_SCHEMA, "version": PACKAGE_VERSION,
        "room": f"{doc['world']}:{doc['area']}:{doc['room']}",
        "sha256": digest, "package_dir": str(dest),
        "native_assets_included": False, "engine_adapter": "unavailable",
        "counts": {"collision": len(doc["collision"]["cells"]),
                   "doors": len(doc["doors"]), "entities": len(doc["entities"]),
                   "events": len(doc["events"]),
                   "transitions": len(doc["transitions"])},
        "persisted": not dry_run,
    }
    if dry_run:
        return result
    # Refuse symlink components before making ignored export folders.
    current = root
    for component in parent.relative_to(root).parts:
        current = current / component
        if current.is_symlink():
            raise ValueError("room-export parent symlink refused")
    parent.mkdir(parents=True, exist_ok=True)
    def verify_existing() -> None:
        if (dest.is_symlink() or not dest.is_dir() or
                (dest / "room.json").is_symlink() or
                (dest / "preview.tsv").is_symlink() or
                (dest / "room.json").read_bytes() != full or
                (dest / "preview.tsv").read_bytes() != preview):
            raise ValueError("room-export content-addressed package collision")
    if dest.exists() or dest.is_symlink():
        verify_existing()
    else:
        temporary = Path(tempfile.mkdtemp(prefix=".room-package-", dir=parent))
        try:
            _write_synced(temporary / "room.json", full)
            _write_synced(temporary / "preview.tsv", preview)
            try:
                temporary.rename(dest)  # Publish both files together.
            except FileExistsError:
                verify_existing()  # Concurrent identical export.
        finally:
            if temporary.exists():
                shutil.rmtree(temporary)
    result["created_paths"] = [str(dest / "room.json"), str(dest / "preview.tsv")]
    return result
