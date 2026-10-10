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
        # CLIP_BEHAVIOR_MAKE_CATEGORY(name, start, end) expands to two
        # enumerators that also continue the implicit numbering.
        body = re.sub(r"CLIP_BEHAVIOR_MAKE_CATEGORY\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)",
                      r"CLIP_BEHAVIOR_\1_START = \2, CLIP_BEHAVIOR_\1_END = \3,", body)
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
    behaviors = parse_enum(header, "ClipBehavior")
    clipdata = parse_enum(header, "Clipdata")
    tilemap = parse_enum(header, "ClipdataTilemap")
    main_source = (decomp / "src/data/clipdata_types.c").read_text(encoding="utf-8")
    tile_source = (decomp / "src/data/clipdata_types_tilemap.c").read_text(encoding="utf-8")
    return {"types": types, "behaviors": behaviors,
            "main": parse_type_table(main_source, "sClipdataCollisionTypes",
                                     clipdata, types),
            "tilemap": parse_type_table(tile_source, "sClipdataCollisionTypes_Tilemap",
                                        tilemap, types),
            "main_behavior": parse_type_table(main_source, "sClipdataBehaviorTypes",
                                              clipdata, behaviors),
            "tilemap_behavior": parse_type_table(tile_source, "sClipdataBehaviorTypes_Tilemap",
                                                 tilemap, behaviors),
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


def clip_behavior(raw: int, tables) -> int:
    """Native clip behavior (pClipBehaviors) of one raw Clipdata value."""
    if raw < tables["count"]:
        return tables["main_behavior"].get(raw, 0)
    flag = tables["tilemap_flag"]
    if flag <= raw < flag + tables["tilemap_count"]:
        return tables["tilemap_behavior"].get(raw - flag, 0)
    return 0


DOOR_RE = re.compile(r"const struct Door s(\w+)Doors\[\d+\]\s*=\s*\{(.*?)\n\};", re.S)
DOOR_ITEM_RE = re.compile(r"\{([^{}]*)\}", re.S)
DOOR_FIELD_RE = re.compile(r"\.(\w+)\s*=\s*([^,\n}]+)")
PIXEL_RE = re.compile(r"(-?)\s*BLOCK_TO_PIXEL\(\s*([\d.]+)f?\s*\)")


def _door_offset(expression: str) -> int:
    """s8 pixel offset written as [-]BLOCK_TO_PIXEL(n) or an integer."""
    match = PIXEL_RE.fullmatch(expression.strip())
    if match:
        value = int(float(match.group(2)) * 16)
        return -value if match.group(1) else value
    return int(expression, 0)


def door_tables(decomp: Path = DECOMP) -> dict[str, list[dict]]:
    """Every area's sAreaDoors table with exits; the NONE terminator ends it."""
    source = (decomp / "src/data/rooms_data.c").read_text(encoding="utf-8")
    tables = {}
    for match in DOOR_RE.finditer(source):
        doors = []
        for item in DOOR_ITEM_RE.finditer(match.group(2)):
            fields = {k: v.strip() for k, v in DOOR_FIELD_RE.findall(item.group(1))}
            flags = {part.strip() for part in fields["type"].split("|")}
            kind = next((flag for flag in flags if flag in DOOR_KINDS), None)
            if kind is None:
                raise ValueError(f"{match.group(1)} door has no base type")
            doors.append({"kind": DOOR_KINDS[kind], "event": "DOOR_TYPE_LOAD_EVENT_BASED_ROOM" in flags,
                          **{key: int(fields[key]) for key in
                             ("sourceRoom", "xStart", "xEnd", "yStart", "yEnd",
                              "destinationDoor")},
                          "xExit": _door_offset(fields["xExit"]),
                          "yExit": _door_offset(fields["yExit"])})
        tables[match.group(1)] = doors
    return tables


DOOR_KINDS = {
    "DOOR_TYPE_NONE": "none",
    "DOOR_TYPE_AREA_CONNECTION": "area",
    "DOOR_TYPE_NO_HATCH": "nohatch",
    "DOOR_TYPE_OPEN_HATCH": "hatch",
    "DOOR_TYPE_CLOSED_HATCH": "hatch",
    "DOOR_TYPE_REMOVE_MOTHER_SHIP": "mothership",
    "DOOR_TYPE_SET_MOTHER_SHIP": "mothership",
}
# Project bit order for the hatch weaknesses written to room.tsv.
DAMAGE_BITS = {"CAA_DAMAGE_TYPE_BEAM": 1, "CAA_DAMAGE_TYPE_BOMB_PISTOL": 2,
               "CAA_DAMAGE_TYPE_MISSILE": 4, "CAA_DAMAGE_TYPE_SUPER_MISSILE": 8,
               "CAA_DAMAGE_TYPE_POWER_BOMB": 16}


def hatch_tables(decomp: Path = DECOMP) -> tuple[list[str], dict[str, tuple[int, int]]]:
    """sHatchTypeTable (indexed by door behavior) and sHatchBehaviors."""
    hatch_source = (decomp / "src/data/hatch_data.c").read_text(encoding="utf-8")
    match = re.search(r"sHatchTypeTable\[[^\]]*\]\s*=\s*\{(.*?)\};", hatch_source, re.S)
    order = ["CLIP_BEHAVIOR_NO_DOOR", "CLIP_BEHAVIOR_GRAY_DOOR", "CLIP_BEHAVIOR_REGULAR_DOOR",
             "CLIP_BEHAVIOR_MISSILE_DOOR", "CLIP_BEHAVIOR_SUPER_MISSILE_DOOR",
             "CLIP_BEHAVIOR_POWER_BOMB_DOOR"]
    types = [None] * 8
    for key, value in re.findall(r"\[\s*([^\]]+?)\s*\]\s*=\s*(\w+)", match.group(1)):
        door = re.fullmatch(r"BEHAVIOR_TO_DOOR\((\w+)\)", key)
        index = order.index(door.group(1)) if door else int(key, 0)
        types[index] = value
    block_source = (decomp / "src/data/block_data.c").read_text(encoding="utf-8")
    match = re.search(r"sHatchBehaviors\[[^\]]*\]\[2\]\s*=\s*\{(.*?)\n\};", block_source, re.S)
    behaviors = {}
    for name, weakness, health in re.findall(
            r"\[(HATCH_\w+)\]\s*=\s*\{\s*([^,]+?),\s*(\d+)\s*\}", match.group(1)):
        bits = 0
        for part in weakness.split("|"):
            part = part.strip()
            if part != "CAA_DAMAGE_TYPE_NONE":
                bits |= DAMAGE_BITS[part]
        behaviors[name] = (bits, int(health))
    return types, behaviors


def room_hatches(doors, room: int, clip, tables, hatch_types, hatch_behaviors):
    """ConnectionLoadDoors: one hatch per hatch door of the room."""
    width, _, cells = clip
    result = []
    door_type = tables["types"]["CLIPDATA_TYPE_DOOR"]
    door_start = tables["behaviors"]["CLIP_BEHAVIOR_DOOR_START"]
    for index, door in enumerate(doors):
        if door["sourceRoom"] != room or door["kind"] != "hatch":
            continue
        position = width * door["yStart"] + door["xStart"]
        raw = cells[position + 1] if position + 1 < len(cells) else 0
        facing_right = collision_type(raw, tables) == door_type
        if not facing_right:
            raw = cells[position - 1]
        behavior = clip_behavior(raw, tables) - door_start
        hatch = hatch_types[behavior] if 1 <= behavior <= 5 else hatch_types[0]
        if hatch in (None, "HATCH_NONE", "HATCH_UNUSED"):
            continue
        weakness, health = hatch_behaviors[hatch]
        x = door["xStart"] + (1 if facing_right else -1)
        result.append((index, x, door["yStart"], hatch, weakness, health))
    return result


def encode_room(area: str, number: int, width: int, height: int,
                clip: tuple[int, int, tuple[int, ...]], tables,
                doors: list[dict] | None = None, hatches=None) -> str:
    clip_width, clip_height, cells = clip
    if clip_width * 16 != width or clip_height * 16 != height:
        raise ValueError("Clipdata dimensions differ from the rendered room")
    lines = [f"{ROOM_SCHEMA}\t1\tmzm\t{area}\t{number}\t{width}\t{height}"]
    transitions = {tables["behaviors"]["CLIP_BEHAVIOR_DOOR_TRANSITION"]: "door",
                   tables["behaviors"]["CLIP_BEHAVIOR_VERTICAL_UP_TRANSITION"]: "up",
                   tables["behaviors"]["CLIP_BEHAVIOR_VERTICAL_DOWN_TRANSITION"]: "down"}
    for y in range(clip_height):
        for x in range(clip_width):
            raw = cells[y * clip_width + x]
            if raw:
                lines.append(f"C\t{x}\t{y}\t{raw}\t{collision_type(raw, tables)}")
                kind = transitions.get(clip_behavior(raw, tables))
                if kind:
                    lines.append(f"B\t{x}\t{y}\t{kind}")
    for index, door in enumerate(doors or []):
        if door["sourceRoom"] != number:
            continue
        destination = "-\t0\t0\t0\t0"
        if door["kind"] not in ("area", "none") and door["destinationDoor"] < len(doors):
            target = doors[door["destinationDoor"]]
            destination = (f"{area.lower()}_{target['sourceRoom']:03}\t{target['xStart']}\t"
                           f"{target['yEnd']}\t{target['xExit']}\t{target['yExit']}")
        lines.append(f"D\t{index}\t{door['kind']}\t{door['xStart']}\t{door['xEnd']}\t"
                     f"{door['yStart']}\t{door['yEnd']}\t{destination}")
    for index, x, y, hatch, weakness, health in hatches or []:
        lines.append(f"H\t{index}\t{x}\t{y}\t{hatch[len('HATCH_'):].lower()}\t"
                     f"{weakness}\t{health}")
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
    tables = collision_tables()
    doors = door_tables().get(area, [])
    hatch_types, hatch_behaviors = hatch_tables()
    hatches = room_hatches(doors, number, clip, tables, hatch_types, hatch_behaviors)
    text = encode_room(area, number, width, height, clip, tables, doors, hatches)
    folder = private_path(Path(root), RUNTIME_ROOMS / f"{area.lower()}_{number:03}",
                          create=True)
    folder.mkdir(exist_ok=True)
    write_atomic(folder / "room.tsv", text)
    write_atomic(folder / "background.bmp", render.bmp24(width, height, bytes(rgb)))
    return {"folder": folder, "width": width, "height": height,
            "cells": text.count("\nC\t"), "doors": text.count("\nD\t"),
            "hatches": text.count("\nH\t"), "composite": composite}


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
          f"{result['cells']} Clipdata cells, {result['doors']} doors, "
          f"{result['hatches']} hatches, {result['composite']} composite")
    print("Folder:", result["folder"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
