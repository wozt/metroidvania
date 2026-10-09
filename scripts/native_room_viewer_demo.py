#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Produce a private MZM project preview with dimensions matching real local BG1.

Requires previously imported, locally owned verified Zero Mission ROM resources.
Uses the repository's authentic partial BG1/BG2 renderer, never embeds a BMP
in the project package. Project data is isolated under ignored assets/extracted/.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts import mzm_room_render as native
from scripts import project_room_entities as rooms
from scripts import project_room_package as package


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--area", default="Brinstar", choices=rooms.MZM_AREAS)
    parser.add_argument("--room", default=33, type=int)
    args = parser.parse_args()
    if not 0 <= args.room <= 999:
        parser.error("room must be in 0..999")
    try:
        source = native.decode_room(args.area, args.room)
        bg1 = source["layers"]["Bg1"]
        if bg1["status"] != "DECODED_METATILES":
            raise ValueError("original BG1 not decoded for this room")
        width, height = bg1["width_blocks"] * 16, bg1["height_blocks"] * 16
        root = ROOT / "assets/extracted/native_demo_0125"
        # Re-create ONLY this explicitly named private demonstration workroom.
        doc = rooms._new("mzm", args.area, args.room, width, height)
        cols, rows = width // 16, height // 16
        rooms.collision_fill(doc, 0, rows - 1, cols, 1, "solid")
        if cols >= 5 and rows >= 5:
            rooms.collision_fill(doc, 2, rows - 3, min(4, cols - 2), 1, "one_way")
            rooms.collision_fill(doc, cols - 2, rows - 2, 1, 1, "water")
        if cols >= 2 and rows >= 2:
            rooms.create(doc, "OBJECT", 16, 16, "Native-size demo marker")
            rooms.door_create(doc, 0, height - 32, 16, 32,
                              "Demo exit", "normal", "left")
        rooms.save(root, doc)
        result = package.export(root, doc)
    except (ValueError, OSError, KeyError, IndexError) as exc:
        parser.error(str(exc))
    print("Private native-sized project exported:", result["package_dir"])
    print("Local source BG1 (not packaged):", native.OUTPUT /
          source["layers"]["Bg1"]["path"])
    print("Run from repository root:")
    print("./build/fusion_room_package_viewer", Path(result["package_dir"]) / "preview.tsv")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
