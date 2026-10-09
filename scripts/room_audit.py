#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Audit every discovered native room through the real render decoders.

The audit never writes preview images. It exercises decompression, native
resource loading, layer decoding, composition, collision rendering and bitmap
encoding, then returns structured diagnostics suitable for GTK and headless
automation. ROM-derived metadata remains private under assets/extracted.
"""
from __future__ import annotations

import argparse
from collections import Counter
from concurrent.futures import ProcessPoolExecutor, as_completed
import json
from pathlib import Path
import struct
import sys
import time
from typing import Callable

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.import_game_assets import OUTPUT, verified_rom, write_generated
from scripts.import_mzm_rooms import ROOM_SOURCE, decode_room_descriptors

FORMAT = "MV_ROOM_RENDER_AUDIT_1"
WORLDS = ("zero_mission", "aria")
STATUSES = ("success", "partial", "unsupported", "error")
DEFAULT_REPORT = OUTPUT / "audits/room_render_audit.json"


def _diagnostic_status(error: Exception) -> str:
    message = str(error).lower()
    unsupported = (
        "unsupported", "not available", "no supported", "not rle",
        "requires a future", "not decoded",
    )
    return "unsupported" if any(token in message for token in unsupported) else "error"


def _mzm_descriptor(room: dict) -> dict:
    fields = room["fields"]
    layers = [
        {"layer": name, "source": fields.get(f"p{name}Data"),
         "property": fields.get(f"{name.lower()}Prop")}
        for name in ("Bg1", "Bg2", "Bg3")
        if fields.get(f"p{name}Data") is not None
    ]
    return {
        "world": "zero_mission",
        "area": room["area"],
        "area_index": None,
        "room": room["index"],
        "native_id": room["id"],
        "native_reference": room["source"],
        "layers": layers,
        "tileset": fields["tileset"],
        "palette_references": [f"tilesets/{fields['tileset']}.pal"],
        "graphics_references": [fields["pBg1Data"], fields["pBg2Data"],
                                fields.get("pBg3Data"), fields["pClipData"]],
    }


def _aria_descriptor(room: dict) -> dict:
    return {
        "world": "aria",
        "area": room["area"],
        "area_index": room["engine_area"],
        "room": room["room"],
        "native_id": f"aria:{room['engine_area']:02d}:{room['room']:03d}",
        "native_reference": room["source_pointer"],
        "layers": [
            {"layer": item["layer"], "metadata_pointer": item["metadata_pointer"],
             "control": item["control"],
             "width_screens": item["width_screens"],
             "height_screens": item["height_screens"]}
            for item in room["backgrounds"]
        ],
        "tileset": None,
        "palette_references": [entry["resource_pointer"]
                               for entry in room["palette_loads"]],
        "graphics_references": [entry["resource_pointer"]
                                for entry in room["graphics_loads"]],
    }


def _finish_record(base: dict, started: float, result: dict | None,
                   error: Exception | None) -> dict:
    record = {**base, "duration_ms": round((time.perf_counter() - started) * 1000, 3),
              "process": {"status": "completed", "exit_code": 0},
              "expected_render": None, "invalid_graphics_references": [],
              "decoder_status": None, "error": None}
    if error is not None:
        record["status"] = _diagnostic_status(error)
        record["process"]["exit_code"] = 2
        record["error"] = {"type": type(error).__name__, "message": str(error)}
        return record
    assert result is not None
    record["decoder_status"] = result.get("status")
    record["status"] = "partial" if result.get("limitations") else "success"
    if result.get("width") and result.get("height"):
        record["expected_render"] = {
            "width_px": result["width"], "height_px": result["height"]}
    rendered_layers = []
    for name, layer in (result.get("layers") or {}).items() if isinstance(
            result.get("layers"), dict) else enumerate(result.get("layers", []), 1):
        entry = {key: value for key, value in layer.items()
                 if key not in ("path", "painted_pixels")}
        entry.setdefault("layer", name)
        rendered_layers.append(entry)
        unresolved = entry.get("unresolved_pixels_or_cells",
                               entry.get("unresolved_tiles_or_pixels", 0))
        if unresolved:
            record["invalid_graphics_references"].append({
                "layer": entry["layer"], "unresolved_count": unresolved})
    record["layers"] = rendered_layers
    if record["expected_render"] is None:
        sizes = [(layer.get("width_blocks", 0) * 16,
                  layer.get("height_blocks", 0) * 16)
                 for layer in rendered_layers if layer.get("width_blocks")]
        if sizes:
            width, height = max(sizes, key=lambda size: size[0] * size[1])
            record["expected_render"] = {"width_px": width, "height_px": height}
    return record


def audit_mzm_area(area: str) -> list[dict]:
    """Audit one discovered MZM source area in one worker process."""
    from scripts.mzm_room_render import decode_room

    rooms = [room for room in decode_room_descriptors(
        ROOM_SOURCE.read_text(encoding="utf-8")) if room["area"] == area]
    records = []
    for room in rooms:
        base = _mzm_descriptor(room)
        started = time.perf_counter()
        try:
            result = decode_room(area, room["index"], write_outputs=False)
            records.append(_finish_record(base, started, result, None))
        except (OSError, ValueError, KeyError, IndexError, struct.error) as exc:
            records.append(_finish_record(base, started, None, exc))
    return records


def audit_aria_area(area: int) -> list[dict]:
    """Audit one discovered Aria engine area with one shared decoded catalog."""
    from scripts.aos_room_render import render_room
    from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1, decode_world

    rom = verified_rom(DEFAULT_ROM, EXPECTED_SHA1)
    world = decode_world(rom)
    rooms = [room for room in world["rooms"] if room["engine_area"] == area]
    resource_cache = {}
    records = []
    for room in rooms:
        base = _aria_descriptor(room)
        started = time.perf_counter()
        try:
            result = render_room(rom, area, room["room"], verify_hash=False,
                                 world_catalog=world, write_outputs=False,
                                 resource_cache=resource_cache)
            records.append(_finish_record(base, started, result, None))
        except (OSError, ValueError, KeyError, IndexError, struct.error) as exc:
            records.append(_finish_record(base, started, None, exc))
    return records


def discovered_scopes(world: str, area: str | int | None = None) -> list[str | int]:
    if world == "zero_mission":
        rooms = decode_room_descriptors(ROOM_SOURCE.read_text(encoding="utf-8"))
        scopes = list(dict.fromkeys(room["area"] for room in rooms))
        if area is None:
            return scopes
        if isinstance(area, int) or str(area).isdecimal():
            index = int(area)
            if not 0 <= index < len(scopes):
                raise ValueError("Zero Mission audit area index is out of range")
            return [scopes[index]]
        match = next((name for name in scopes if name.lower() == str(area).lower()), None)
        if match is None:
            raise ValueError("unknown Zero Mission audit area")
        return [match]
    if world == "aria":
        from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1, decode_world
        catalog = decode_world(verified_rom(DEFAULT_ROM, EXPECTED_SHA1))
        scopes = sorted({room["engine_area"] for room in catalog["rooms"]})
        if area is None:
            return scopes
        if not str(area).isdecimal() or int(area) not in scopes:
            raise ValueError("unknown Aria audit area")
        return [int(area)]
    raise ValueError("world must be zero_mission or aria")


def audit_world(world: str, *, area: str | int | None = None, workers: int = 1,
                progress: Callable[[str], None] | None = None) -> dict:
    """Run deterministic area-isolated audits and return one structured report."""
    if world not in WORLDS:
        raise ValueError("world must be zero_mission or aria")
    if type(workers) is not int or not 1 <= workers <= 8:
        raise ValueError("workers must be an integer in [1, 8]")
    scopes = discovered_scopes(world, area)
    worker = audit_mzm_area if world == "zero_mission" else audit_aria_area
    records = []
    if workers == 1 or len(scopes) == 1:
        for scope in scopes:
            result = worker(scope)
            records.extend(result)
            if progress:
                progress(f"audited {world} area {scope}: {len(result)} rooms")
    else:
        with ProcessPoolExecutor(max_workers=min(workers, len(scopes))) as pool:
            futures = {pool.submit(worker, scope): scope for scope in scopes}
            for future in as_completed(futures):
                scope = futures[future]
                result = future.result()
                records.extend(result)
                if progress:
                    progress(f"audited {world} area {scope}: {len(result)} rooms")
    records.sort(key=lambda item: (
        item["area_index"] if item["area_index"] is not None else 0,
        item["area"], item["room"]))
    counts = Counter(record["status"] for record in records)
    return {
        "format": FORMAT,
        "world": world,
        "scope": {"area": area, "discovered_areas": scopes},
        "room_count": len(records),
        "summary": {status: counts[status] for status in STATUSES},
        "rooms": records,
    }


def write_private_report(report: dict, name: str | None = None) -> Path:
    world = report.get("world")
    if report.get("format") != FORMAT or world not in WORLDS:
        raise ValueError("invalid room audit report")
    filename = name or f"{world}.json"
    relative = f"audits/{filename}"
    write_generated(relative, (json.dumps(report, indent=2) + "\n").encode("utf-8"))
    return OUTPUT / relative


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--world", choices=WORLDS, required=True)
    parser.add_argument("--area")
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--format", choices=("json", "text"), default="text")
    args = parser.parse_args(argv)
    try:
        report = audit_world(args.world, area=args.area, workers=args.workers,
                             progress=lambda text: print(text, file=sys.stderr))
        if args.output:
            if args.output.is_symlink():
                raise ValueError("symlink audit output refused")
            payload = json.dumps(report, indent=2) + "\n"
            args.output.parent.mkdir(parents=True, exist_ok=True)
            temp = args.output.with_name(args.output.name + ".tmp")
            temp.write_text(payload, encoding="utf-8")
            temp.replace(args.output)
        else:
            path = write_private_report(report)
            if args.format == "text":
                print(f"report: {path}")
        if args.format == "json":
            print(json.dumps(report, separators=(",", ":")))
        else:
            print(f"{args.world}: {report['room_count']} rooms; " + ", ".join(
                f"{key}={value}" for key, value in report["summary"].items()))
    except (OSError, ValueError, KeyError, IndexError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
