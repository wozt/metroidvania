#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Extract the portions of each verified ROM described by pinned decomp DBs.

This command performs RAW extraction (not decoding of all assets). It also
renders the already-verified Samus and Soma animations as real BMP sprites.
All generated files stay under ignored assets/extracted/. Never publish them.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.asset_layout import (  # noqa: E402
    ARIA_RAW,
    ARIA_SOMA,
    IMPORT_CATALOG,
    METROID_RAW,
    METROID_SAMUS,
)

OUTPUT = ROOT / "assets" / "extracted"
ROMS = {
    "metroid": ("Metroid - Zero Mission (USA).gba",
                "5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8", "mzm"),
    "aria": ("Castlevania - Aria of Sorrow (USA).gba",
             "abd71fe01ebb201bcc133074db1dd8c5253776c7", "cvaos"),
}
_ANIMATIONS = {
    "samus": ("idle", "run", "jump", "attack"),
    "soma": ("idle", "run", "jump", "attack", "run_start", "run_stop"),
}
_ALLOWED_NAME = re.compile(r"^[A-Za-z0-9_.-]+$")


def verified_rom(path: Path, expected: str) -> bytes:
    data = path.read_bytes()
    if hashlib.sha1(data).hexdigest() != expected:
        raise ValueError(f"ROM SHA-1 mismatch: {path} (expected verified USA ROM)")
    return data


def safe_parts(value: str) -> tuple[str, ...]:
    path = Path(value)
    if path.is_absolute() or not value or "\\" in value:
        raise ValueError(f"unsafe extracted path: {value!r}")
    parts = path.parts
    if not parts or any(part in (".", "..") or not _ALLOWED_NAME.fullmatch(part)
                        for part in parts):
        raise ValueError(f"unsafe extracted path: {value!r}")
    return parts


def safe_output(relative: str) -> Path:
    parts = safe_parts(relative)
    output = OUTPUT.joinpath(*parts)
    # A symlink at any path component could redirect extracted ROM bytes into
    # another directory. Do not even follow a preexisting output symlink.
    for candidate in (ROOT, ROOT / "assets", OUTPUT, *(
            OUTPUT.joinpath(*parts[:index]) for index in range(1, len(parts) + 1))):
        if candidate.is_symlink():
            raise ValueError(f"symlink extraction destination refused: {candidate}")
    return output


def write_generated(relative: str, data: bytes) -> bool:
    dest = safe_output(relative)
    if dest.is_file() and dest.read_bytes() == data:
        return False
    dest.parent.mkdir(parents=True, exist_ok=True)
    temp = dest.with_name(dest.name + ".tmp")
    if temp.is_symlink():
        raise ValueError(f"symlink extraction temp refused: {temp}")
    try:
        temp.write_bytes(data)
        temp.replace(dest)
    finally:
        if temp.exists():
            temp.unlink()
    return True


def classify_raw(directory: str, path: str) -> str:
    path_lower = (directory + "/" + path).lower()
    if "direct_sound" in path_lower or "sound" in path_lower or "music" in path_lower:
        return "raw_audio_or_sound_data"
    if "room" in path_lower or "bg" in path_lower or "clip" in path_lower:
        return "raw_room_or_background_data"
    if "sprite" in path_lower or "obj" in path_lower or "oam" in path_lower:
        return "raw_sprite_or_object_data"
    if "cutscene" in path_lower or "scene" in path_lower:
        return "raw_cutscene_data"
    return "raw_unclassified_data"


def extract_database_entries(db: list[dict[str, Any]], rom: bytes, world: str,
                             *, write: bool = True) -> list[dict[str, Any]]:
    """Validate all known offsets before writing ANY bytes; no guesswork."""
    if world not in ("metroid", "aria"):
        raise ValueError("unsupported source world")
    entries = []
    seen = set()
    for number, item in enumerate(db):
        directory = item.get("dir", "data") if world == "metroid" else "data"
        path = item["path"]
        safe_parts(directory)
        safe_parts(path)
        offset_hex = item.get("addr", {}).get("us")
        if offset_hex is None:
            continue
        count = item["count"]
        if isinstance(count, dict):
            count = count["us"]
        offset = int(offset_hex, 16)
        length = int(count, 16) * int(item["size"])
        if offset < 0 or length < 0 or offset + length > len(rom):
            raise ValueError(f"decomp entry {number} exceeds ROM boundaries: {path}")
        raw_root = METROID_RAW if world == "metroid" else ARIA_RAW
        relative = (raw_root.relative_to("assets/extracted")
                    / directory / path).as_posix()
        if relative in seen:
            raise ValueError(f"duplicate extracted file: {relative}")
        seen.add(relative)
        entries.append({"path": relative, "rom_offset": offset,
                        "length": length, "role": classify_raw(directory, path),
                        "status": "RAW_UNDECODED"})
    if write:
        for entry in entries:
            start = entry["rom_offset"]
            write_generated(entry["path"], rom[start:start + entry["length"]])
    return entries


def render_sprites(roms: dict[str, bytes]) -> list[dict[str, Any]]:
    # Use the existing reverse-engineered, validated OAM decoders. Their output
    # includes transparent backgrounds and native frame timing metadata.
    from scripts import mzm_samus_sprite as samus
    from scripts import aos_soma_sprite as soma

    builders = {
        "idle": (samus.POWER_SUIT_IDLE_FRAME_COUNT, samus.make_power_suit_idle_right),
        "run": (samus.POWER_SUIT_RUN_FRAME_COUNT, samus.make_power_suit_run_right),
        "jump": (samus.POWER_SUIT_JUMP_FRAME_COUNT, samus.make_power_suit_jump_right),
        "attack": (samus.POWER_SUIT_ATTACK_FRAME_COUNT, samus.make_power_suit_attack_right),
    }
    rendered = []
    for name in _ANIMATIONS["samus"]:
        total, builder = builders[name]
        bounds = samus.aligned_canvas_bounds([
            builder(roms["metroid"], frame)[1] for frame in range(total)
        ])
        for frame in range(total):
            data, info = builder(roms["metroid"], frame, bounds)
            path = (METROID_SAMUS.relative_to("assets/extracted")
                    / "animations/basic" / f"{name}_{frame}.bmp").as_posix()
            write_generated(path, data)
            rendered.append({"path": path, "world": "metroid", "actor": "samus",
                             "animation": name, "frame": frame,
                             "duration_ticks": info["frame"]["duration"],
                             "status": "DECODED_BMP"})
    for name in _ANIMATIONS["soma"]:
        bounds = soma.animation_canvas_bounds(roms["aria"], name)
        for frame in range(soma.SOMA_ANIMATIONS[name]["frame_count"]):
            data, info = soma.make_soma_animation_frame(
                roms["aria"], name, frame, bounds)
            path = (ARIA_SOMA.relative_to("assets/extracted")
                    / "animations/basic" / f"{name}_{frame}.bmp").as_posix()
            write_generated(path, data)
            rendered.append({"path": path, "world": "aria", "actor": "soma",
                             "animation": name, "frame": frame,
                             "duration_ticks": info["duration"],
                             "status": "DECODED_BMP"})
    return rendered


def references_for(world: str, decomp_root: Path) -> dict[str, list[str]]:
    # These SOURCE paths are navigation references, not extracted scene/music
    # previews or editable room entities.
    prefixes = {
        "metroid": {
            "room_tables": "include/data/rooms",
            "sprite_tables": "include/data/sprites",
            "cutscene_tables": "include/data/cutscenes",
            "audio_sources": "sound",
        },
        "aria": {
            "room_and_data_asm": "asm/data",
            "audio_sources": "sound",
        },
    }[world]
    result = {}
    for kind, prefix in prefixes.items():
        folder = decomp_root / prefix
        result[kind] = sorted(str(p.relative_to(decomp_root)) for p in folder.rglob("*")
                              if p.is_file() and p.suffix in (".h", ".c", ".s", ".inc")) \
                        if folder.is_dir() else []
    return result


def run(args: argparse.Namespace) -> dict[str, Any]:
    if not (ROOT / "CMakeLists.txt").is_file():
        raise ValueError("run this script from the metroidvania project checkout")
    paths = {
        "metroid": args.metroid,
        "aria": args.aria,
    }
    roms: dict[str, bytes] = {}
    for name, (_, sha1, _) in ROMS.items():
        roms[name] = verified_rom(paths[name], sha1)
    manifest: dict[str, Any] = {
        "format": "MV_ASSET_IMPORT_1",
        "provenance": "user-supplied verified USA ROMs, pinned decomp database offsets",
        "warning": "Private extracted Nintendo/Konami data. NEVER commit or redistribute.",
        "sources": {w: {"expected_sha1": data[1], "decomp": data[2]}
                    for w, data in ROMS.items()},
        "raw_entries": [], "decoded_sprites": [], "references": {},
        "not_yet_decoded": ["full room tilemap geometry", "complete world maps",
             "enemy and object sprites/stats/placements", "music tracks as playable audio",
             "sound effects as playable audio", "cutscene timelines/scripts"],
    }
    for world, (_, _, decomp_name) in ROMS.items():
        root = ROOT / "third_party" / decomp_name
        db_path = root / "database.json"
        if not db_path.is_file():
            raise ValueError(f"missing pinned decomp database: {db_path}; "
                             "run git submodule update --init")
        db = json.loads(db_path.read_text(encoding="utf-8"))
        if not isinstance(db, list):
            raise ValueError(f"invalid decomp database format: {db_path}")
        manifest["references"][world] = references_for(world, root)
        if args.scope in ("all", "raw"):
            manifest["raw_entries"].extend(extract_database_entries(db, roms[world], world))
    if args.scope in ("all", "sprites"):
        manifest["decoded_sprites"] = render_sprites(roms)
    counts = Counter(item["role"] for item in manifest["raw_entries"])
    # Original RoomEntryRom descriptors, not decoded room pixels.
    if args.scope == "all":
        from scripts.import_mzm_rooms import run as import_mzm_rooms
        native_rooms = import_mzm_rooms()
        from scripts.mzm_world_atlas import run as import_mzm_world_atlas
        native_world_atlas = import_mzm_world_atlas()
        manifest["native_rooms"] = {"metroid": {
            "count": native_rooms["count"],
            "status": "DECOMP_ROOM_DESCRIPTOR_ONLY",
            "catalog": "rooms/metroid/catalog.json",
            "world_atlas": "rooms/metroid/world_atlas.json",
            "native_door_links": len(native_world_atlas["connections"])}}
    manifest["summary"] = {
        "raw_blocks": len(manifest["raw_entries"]),
        "raw_bytes": sum(item["length"] for item in manifest["raw_entries"]),
        "decoded_sprite_frames": len(manifest["decoded_sprites"]),
        "raw_categories": dict(sorted(counts.items())),
    }
    catalog_path = IMPORT_CATALOG.relative_to("assets/extracted").as_posix()
    write_generated(catalog_path, (json.dumps(manifest, indent=2, ensure_ascii=False)
                                  + "\n").encode("utf-8"))
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metroid", type=Path,
                        default=ROOT / "roms" / ROMS["metroid"][0])
    parser.add_argument("--aria", type=Path,
                        default=ROOT / "roms" / ROMS["aria"][0])
    parser.add_argument("--scope", choices=("all", "sprites", "raw"), default="all")
    args = parser.parse_args()
    try:
        manifest = run(args)
    except (ValueError, OSError, KeyError, TypeError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    summary = manifest["summary"]
    print("Authentic local import: verified both ROM SHA-1 fingerprints")
    print(f"Raw decomp-identified blocks: {summary['raw_blocks']} "
          f"({summary['raw_bytes']} bytes, format NOT decoded)")
    print(f"Decoded native sprite frames: {summary['decoded_sprite_frames']}")
    if "native_rooms" in manifest:
        print(f"Original MZM room descriptors: {manifest['native_rooms']['metroid']['count']} (metadata only)")
    print(f"Private catalog: {ROOT / IMPORT_CATALOG}")
    print("Complete maps, creature stats, audio, and cutscenes remain UNDECODED.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
