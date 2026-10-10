#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Export one native Zero Mission room for the SDL3 runtime.

The export contains the partial BG1-over-BG2 composite produced by
``scripts.mzm_room_render`` and every Clipdata cell resolved to its native
collision type the way ``RoomLoadTileset``/``ClipdataProcess`` do: values
below ``CLIPDATA_COUNT`` index ``sClipdataCollisionTypes``, values carrying
``CLIPDATA_TILEMAP_FLAG`` index ``sClipdataCollisionTypes_Tilemap`` (the RAM
copy that directly follows the main table), and other values read cleared RAM,
which is air. Tables are parsed from the pinned decompilation at export time;
no game table is stored in this repository. Output stays private:

    assets/extracted/metroid/rooms/runtime/<area>_<NNN>/room.tsv
    assets/extracted/metroid/rooms/runtime/<area>_<NNN>/background.bmp
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts import mzm_room_render as render
from scripts.asset_layout import METROID_ROOMS, private_path
from scripts.sprite_library import write_atomic

RUNTIME_ROOMS = METROID_ROOMS / "runtime"
ROOM_SCHEMA = "MVROOM-NATIVE"
DECOMP = ROOT / "third_party/mzm"
ENUM_RE = re.compile(r"MAKE_ENUM\(\s*\w+\s*,\s*(\w+)\s*\)\s*\{(.*?)\};", re.S)
ROW_RE = re.compile(r"\[\s*(\w+)\s*\]\s*=\s*(\w+)")


def parse_enum(header: str, name: str) -> dict[str, int]:
    """Values of one MAKE_ENUM block with C's implicit numbering."""
    for match in ENUM_RE.finditer(header):
        if match.group(1) != name:
            continue
        values, current = {}, -1
        body = re.sub(r"//[^\n]*|/\*.*?\*/", "", match.group(2), flags=re.S)
        for item in body.split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                key, expression = (part.strip() for part in item.split("=", 1))
                current = _evaluate(expression, values)
            else:
                key, current = item, current + 1
            values[key] = current
        return values
    raise ValueError(f"enum {name} not found")


def _evaluate(expression: str, known: dict[str, int]) -> int:
    expression = expression.strip()
    shift = re.fullmatch(r"(\d+)\s*<<\s*(\d+)", expression)
    if shift:
        return int(shift.group(1)) << int(shift.group(2))
    if expression in known:
        return known[expression]
    return int(expression, 0)


def parse_type_table(source: str, name: str, clipdata: dict[str, int],
                     types: dict[str, int]) -> dict[int, int]:
    match = re.search(rf"{name}\s*\[[^\]]*\]\s*=\s*\{{(.*?)\}};", source, re.S)
    if not match:
        raise ValueError(f"table {name} not found")
    table = {}
    for key, value in ROW_RE.findall(match.group(1)):
        index = clipdata.get(key)
        if index is None and re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", key):
            index = int(key, 0)
        if index is None or value not in types:
            raise ValueError(f"unknown entry {key} = {value} in {name}")
        table[index] = types[value]
    return table


def collision_tables(decomp: Path = DECOMP):
    header = (decomp / "include/constants/clipdata.h").read_text(encoding="utf-8")
    types = parse_enum(header, "ClipdataType")
    clipdata = parse_enum(header, "Clipdata")
    tilemap = parse_enum(header, "ClipdataTilemap")
    main = parse_type_table((decomp / "src/data/clipdata_types.c").read_text(encoding="utf-8"),
                            "sClipdataCollisionTypes", clipdata, types)
    tiles = parse_type_table(
        (decomp / "src/data/clipdata_types_tilemap.c").read_text(encoding="utf-8"),
        "sClipdataCollisionTypes_Tilemap", tilemap, types)
    return {"types": types, "main": main, "tilemap": tiles,
            "count": clipdata["CLIPDATA_COUNT"],
            "tilemap_flag": tilemap["CLIPDATA_TILEMAP_FLAG"],
            "tilemap_count": tilemap["CLIPDATA_TILEMAP_COUNT"]}


def collision_type(raw: int, tables) -> int:
    """Native collision type of one raw Clipdata value."""
    if raw < tables["count"]:
        return tables["main"].get(raw, tables["types"]["CLIPDATA_TYPE_AIR"])
    flag = tables["tilemap_flag"]
    if flag <= raw < flag + tables["tilemap_count"]:
        return tables["tilemap"].get(raw - flag, tables["types"]["CLIPDATA_TYPE_AIR"])
    if raw < flag:
        return tables["types"]["CLIPDATA_TYPE_AIR"]
    raise ValueError(f"Clipdata value 0x{raw:x} is outside the native tables")


def encode_room(area: str, number: int, width: int, height: int,
                clip: tuple[int, int, tuple[int, ...]], tables) -> str:
    clip_width, clip_height, cells = clip
    if clip_width * 16 != width or clip_height * 16 != height:
        raise ValueError("Clipdata dimensions differ from the rendered room")
    lines = [f"{ROOM_SCHEMA}\t1\tmzm\t{area}\t{number}\t{width}\t{height}"]
    for y in range(clip_height):
        for x in range(clip_width):
            raw = cells[y * clip_width + x]
            if raw:
                lines.append(f"C\t{x}\t{y}\t{raw}\t{collision_type(raw, tables)}")
    lines.append("END")
    return "\n".join(lines) + "\n"


def produce(root: Path, area: str, number: int) -> dict:
    room = render.read_source_room(area, number)
    area = room["area"]
    fields = room["fields"]
    tileset = int(fields["tileset"])
    gfx, palette, table = render.decoded_tileset(tileset)
    layers = {}
    for layer in ("Bg1", "Bg2"):
        if fields.get(f"{layer.lower()}Prop") == "BG_PROP_RLE_COMPRESSED":
            layers[layer] = render.rle_room(render.room_blob(fields[f"p{layer}Data"]))
    if "Bg1" not in layers:
        raise ValueError("room BG1 is not RLE compressed; no runtime export")
    width_blocks, height_blocks, bg1_blocks = layers["Bg1"]
    words = [entry for index in set(bg1_blocks) if index < len(table)
             for entry in table[index]]
    base, _ = render.graphic_base(words, len(gfx) // 32)
    mask1 = bytearray(width_blocks * 16 * height_blocks * 16)
    rgb, _, _ = render.render_layer(width_blocks, height_blocks, bg1_blocks, table,
                                    gfx, palette, base, visibility=mask1)
    composite = "BG1"
    if "Bg2" in layers and layers["Bg2"][:2] == (width_blocks, height_blocks):
        mask2 = bytearray(len(mask1))
        rgb2, _, _ = render.render_layer(width_blocks, height_blocks, layers["Bg2"][2],
                                         table, gfx, palette, base, visibility=mask2)
        rgb, _ = render.compose_partial_bg12(rgb, mask1, rgb2, mask2)
        composite = "BG1 over BG2"
    width, height = width_blocks * 16, height_blocks * 16
    clip = render.rle_room(render.room_blob(fields["pClipData"]))
    text = encode_room(area, number, width, height, clip, collision_tables())
    folder = private_path(Path(root), RUNTIME_ROOMS / f"{area.lower()}_{number:03}",
                          create=True)
    folder.mkdir(exist_ok=True)
    write_atomic(folder / "room.tsv", text)
    write_atomic(folder / "background.bmp", render.bmp24(width, height, bytes(rgb)))
    return {"folder": folder, "width": width, "height": height,
            "cells": text.count("\nC\t"), "composite": composite}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--area", required=True)
    parser.add_argument("--room", required=True, type=int)
    args = parser.parse_args(argv)
    try:
        result = produce(args.root, args.area, args.room)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Runtime room {result['width']}x{result['height']}: "
          f"{result['cells']} Clipdata cells, {result['composite']} composite")
    print("Folder:", result["folder"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
