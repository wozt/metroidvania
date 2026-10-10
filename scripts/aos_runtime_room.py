#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Export one native Aria of Sorrow room for the runtime.

The export holds the static-origin background composite of
``scripts.aos_room_render`` and the BG1 collision byte of every 8x8 cell as
``sub_08001A00`` returns it: the table index of ``sub_08001800`` and, for
slope bytes (bits 6-7), bit 2 toggled when the block is X-flipped. The
source's Y-flip toggle tests ``(flags >> 12) & 3`` and can never fire, so it is
not applied. Each ``T`` line is one entry of the room's transition list
(``sub_08010350``): source screen X and Y, the signed X adjustment at +6, the
BG1 position at +0xA/+0xC loaded in the target room, and the target area and
room. Output stays private:

    assets/extracted/aria/rooms/runtime/area_<AA>_room_<RRR>/room.tsv
    assets/extracted/aria/rooms/runtime/area_<AA>_room_<RRR>/background.bmp
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts import aos_room_render as render
from scripts.asset_layout import ARIA_ROOMS, private_path
from scripts.sprite_library import write_atomic

RUNTIME_ROOMS = ARIA_ROOMS / "runtime"
ROOM_SCHEMA = "AOSROOM-NATIVE"


def native_cell(value: int, xflip: bool) -> int:
    """Collision byte as sub_08001A00 returns it for one cell."""
    if value & 0xC0 and xflip:
        value ^= 4
    return value


def _signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def encode_room(area: int, room: int, background: dict, transitions=()) -> str:
    width, height = background["width_tiles"], background["height_tiles"]
    cells, flips = background["collision"], background["collision_xflip"]
    lines = [f"{ROOM_SCHEMA}\t2\t{area}\t{room}\t{background['width_screens']}\t"
             f"{background['height_screens']}\t{width}\t{height}"]
    for y in range(height):
        row = cells[y * width:(y + 1) * width]
        flip = flips[y * width:(y + 1) * width]
        lines.append("R\t" + "".join(f"{native_cell(v, f):02x}" for v, f in zip(row, flip)))
    for item in transitions:
        lines.append(f"T\t{item['source_screen_x']}\t{item['source_screen_y']}\t"
                     f"{_signed16(item['field_6'])}\t{item['load_x']}\t{item['load_y']}\t"
                     f"{item['target_engine_area']}\t{item['target_room']}")
    lines.append("END")
    return "\n".join(lines) + "\n"


def produce(root: Path, area: int, room: int, rom: bytes, world=None) -> dict:
    world = world or render.decode_world(rom)
    entry = next(item for item in world["rooms"]
                 if item["engine_area"] == area and item["room"] == room)
    bg1 = next((b for b in entry["backgrounds"] if b["layer"] == 1), None)
    background = render.decode_background(rom, bg1) if bg1 else {"status": "ABSENT"}
    if background.get("status") != "DECODED_TEXT_BACKGROUND" or \
            background.get("collision") is None:
        raise ValueError("room has no decodable BG1 collision table")
    vram, palette, _ = render.load_video_memory(rom, entry)
    layers = [render.decode_background(rom, item) for item in entry["backgrounds"]]
    width, height, composite = render.composite_backgrounds(
        [(layer, render.render_background(layer, vram, palette)[0]) for layer in layers
         if layer["status"] == "DECODED_TEXT_BACKGROUND"])
    folder = private_path(Path(root), RUNTIME_ROOMS / f"area_{area:02}_room_{room:03}",
                          create=True)
    folder.mkdir(exist_ok=True)
    write_atomic(folder / "room.tsv",
                 encode_room(area, room, background, entry["transitions"]))
    write_atomic(folder / "background.bmp", render.bmp24(width, height, composite))
    return {"folder": folder, "width": width, "height": height,
            "cells": background["width_tiles"] * background["height_tiles"]}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba")
    parser.add_argument("--area", type=int)
    parser.add_argument("--room", type=int)
    parser.add_argument("--all", action="store_true",
                        help="export every room with a decodable BG1 layer")
    args = parser.parse_args(argv)
    if args.all == (args.area is not None or args.room is not None) or \
            (not args.all and (args.area is None or args.room is None)):
        parser.error("use either --all or both --area and --room")
    try:
        rom = args.rom.read_bytes()
        if args.all:
            world = render.decode_world(rom)
            exported, skipped = 0, []
            for item in world["rooms"]:
                try:
                    produce(args.root, item["engine_area"], item["room"], rom, world)
                    exported += 1
                except ValueError as exc:
                    skipped.append(f"{item['engine_area']}/{item['room']}: {exc}")
            print(f"Aria runtime rooms exported: {exported}, skipped: {len(skipped)}")
            for line in skipped:
                print("  skipped", line)
            return 0
        result = produce(args.root, args.area, args.room, rom)
    except (OSError, ValueError, StopIteration) as exc:
        parser.error(str(exc))
    print(f"Aria runtime room {result['width']}x{result['height']}: "
          f"{result['cells']} collision cells")
    print("Folder:", result["folder"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
