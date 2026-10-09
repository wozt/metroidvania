#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Preview authentic *partial* MZM BG1 without invented room content by default.

A blank geometry document is used only as the SDL3 viewer's interchange format;
it is NOT a reconstruction of native collisions, entities or doors. Explicit
--demo-overlays restores the previous synthetic project-data demonstration.
All generated outputs are private and ignored; ROMs are never modified.
"""
from __future__ import annotations

import argparse
import subprocess
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts import mzm_room_render as native
from scripts import project_room_entities as rooms
from scripts import project_room_package as package
from scripts import native_source_overlay


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--area", default="Brinstar", choices=rooms.MZM_AREAS)
    parser.add_argument("--room", default=33, type=int)
    parser.add_argument("--demo-overlays", action="store_true",
                        help="OPT IN to synthetic collision, object and door overlays")
    parser.add_argument("--launch", action="store_true",
                        help="open the resulting preview in the SDL3 viewer")
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
        # The entity writer creates descendants but expects this sandbox root to exist.
        # Keep all generated/project-owned files under the ignored private tree.
        if root.is_symlink():
            raise ValueError("native demo root symlink refused")
        root.mkdir(parents=True, exist_ok=True)
        # Re-create ONLY this explicitly named private demonstration workroom.
        doc = rooms._new("mzm", args.area, args.room, width, height)
        # The native room is NOT described by project-owned collisions/objects.
        # Never manufacture them for an authentic-source preview.
        if args.demo_overlays:
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
        native_path = None
        native_counts = None
        try:
            native_path, native_counts = native_source_overlay.write_overlay(
                ROOT, args.area, args.room, width, height)
        except (ValueError, OSError, KeyError, IndexError) as native_exc:
            # The graphical preview can still work without decoded Clipdata.
            print("Native source overlays unavailable:", native_exc)
    except (ValueError, OSError, KeyError, IndexError) as exc:
        parser.error(str(exc))
    print("Private native-sized viewer interchange:", result["package_dir"])
    print("Partial ORIGINAL BG1 (not packaged):", native.OUTPUT /
          source["layers"]["Bg1"]["path"])
    composite = source.get('composite', {})
    if composite.get('status') == 'PARTIAL_BG1_OVER_BG2_DIAGNOSTIC':
        print("BG1-over-BG2 diagnostic (not packaged):", native.OUTPUT / composite['path'])
        print("Visibility counts:", {key: composite[key] for key in
              ('bg1_visible_pixels', 'bg2_visible_pixels', 'unresolved_pixels')})
        print("BG layer order/priority is not yet verified against the GBA renderer.")
    else:
        print("BG1+BG2 composite unavailable:", composite.get('reason', 'not decoded'))
    if args.demo_overlays:
        print("NOTE: synthetic project collision/object/door overlays ENABLED.")
    else:
        print("NATIVE PREVIEW: no invented collision, entity or door overlays.")
        print("BG1/BG2 are partial separate previews: this is NOT a full GBA composite.")
    cmd = [str(ROOT / "build/fusion_room_package_viewer")]
    if native_path:
        cmd.extend(["--native-source", str(native_path)])
        print("Native Clipdata and annotation source overlay:", native_counts)
    cmd.append(str(Path(result["package_dir"]) / "preview.tsv"))
    print("Run from repository root:")
    print(" ".join(cmd))
    if args.launch:
        try:
            return subprocess.run(cmd,
                cwd=ROOT, check=False).returncode
        except OSError as exc:
            parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
