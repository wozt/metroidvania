#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the canonical private Soma Cruz animation library from the Aria ROM.

Every animation listed by Soma's native animation descriptor is exported with
its native frame durations, not only hand-picked sequences. Each frame is the
64x64 body cell selected by its frame id, colorized with the palette bank the
player code loads for that animation (the byte table at 0x080E126C, read by
sub_0803C7B4 on every direct animation change) and cropped; offsets place the BMP relative to Soma's draw origin, which sits 32
pixels right of and 47 pixels below the cell's top-left corner (the anchor
verified for the knife attack). The knife animations are exported the same
way from their own descriptor, with OAM offsets already origin-relative.

One-shot animations requested through entity + 0x20 keep the previous
palette in the game; the export uses each animation's own bank. Palette
banks never referenced by that table and every other weapon remain
unidentified and are reported rather than guessed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.aos_soma_sprite import (
    EXPECTED_SHA1,
    KNIFE_ANIMATION_DESCRIPTOR,
    KNIFE_CELL_ANCHOR,
    KNIFE_PALETTE_DESCRIPTOR,
    SOMA_ANIMATION_DESCRIPTOR,
    SOMA_ANIMATIONS,
    _rom_slice,
    decode_cell,
    decode_tiles,
    extract_cell_tiles,
    extract_knife_frame,
    load_palette,
    parse_animation,
)
from scripts.asset_layout import ARIA_SOMA_RUNTIME
from scripts.sprite_library import LibraryWriter, bmp_from_pixels


def animation_count(rom: bytes, descriptor: int) -> int:
    _, count, _, _, _ = struct.unpack(
        "<HHIII", _rom_slice(rom, descriptor, 16, "animation descriptor"))
    return count


def _opaque_pixels(rgba: bytes, width: int, height: int, origin_x: int,
                   origin_y: int) -> dict[tuple[int, int], tuple[int, int, int]]:
    pixels = {}
    for y in range(height):
        for x in range(width):
            offset = (y * width + x) * 4
            if rgba[offset + 3]:
                pixels[(x - origin_x, y - origin_y)] = tuple(rgba[offset:offset + 3])
    return pixels


def soma_frame(rom: bytes, frame_id: int, palette: bytes):
    tiles, graphics = extract_cell_tiles(rom, frame_id)
    rgba, width, height = decode_cell(tiles, palette)
    pixels = _opaque_pixels(rgba, width, height, *KNIFE_CELL_ANCHOR)
    return bmp_from_pixels(pixels), graphics


def knife_frame(rom: bytes, frame_id: int, palette: bytes):
    tiles, metadata = extract_knife_frame(rom, frame_id)
    rgba, width, height = decode_tiles(tiles, metadata["width"] // 8,
                                       metadata["height"] // 8, palette)
    x, y = metadata["position"]
    origin = (KNIFE_CELL_ANCHOR[0] - x, KNIFE_CELL_ANCHOR[1] - y)
    return bmp_from_pixels(_opaque_pixels(rgba, width, height, *origin)), metadata


ANIMATION_PALETTE_TABLE = 0x080E126C


def animation_palette_banks(rom: bytes, count: int) -> list[int]:
    """Palette bank per Soma animation (byte table read by sub_0803C7B4)."""
    return list(_rom_slice(rom, ANIMATION_PALETTE_TABLE, count, "animation palette table"))


def produce(root: Path, rom_path: Path) -> dict:
    rom = Path(rom_path).read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError("original Aria of Sorrow USA ROM SHA-1 mismatch")
    library = LibraryWriter(root, ARIA_SOMA_RUNTIME)
    observed = {definition["segments"][0][0]: name
                for name, definition in SOMA_ANIMATIONS.items()
                if len(definition["segments"]) == 1}
    animations = {}
    soma_count = animation_count(rom, SOMA_ANIMATION_DESCRIPTOR)
    banks = animation_palette_banks(rom, soma_count)
    palettes = {}
    for index in range(soma_count):
        animation = parse_animation(rom, index)
        bank = banks[index]
        if bank not in palettes:
            palettes[bank] = load_palette(rom, palette_index=bank)
        frames, sources = [], []
        for frame in animation["frames"]:
            (bmp, left, top), graphics = soma_frame(rom, frame["frame_id"],
                                                     palettes[bank])
            frames.append((bmp, frame["duration"], left, top))
            sources.append({"frame_id": frame["frame_id"],
                            "sheet": graphics["sheet_index"],
                            "quadrant": list(graphics["quadrant"])})
        key = f"Soma/animation_{index:03d}"
        library.add(key, frames)
        animations[key] = {
            "native_index": index,
            "animation_pointer": f"0x{animation['animation_pointer']:08x}",
            "observed_label": observed.get(index),
            "palette_bank": bank,
            "frames": sources,
        }
    knife_palette = load_palette(rom, KNIFE_PALETTE_DESCRIPTOR)
    unresolved = {}
    for index in range(animation_count(rom, KNIFE_ANIMATION_DESCRIPTOR)):
        animation = parse_animation(rom, index, KNIFE_ANIMATION_DESCRIPTOR)
        key = f"Knife/animation_{index:03d}"
        frames, sources = [], []
        try:
            for frame in animation["frames"]:
                (bmp, left, top), metadata = knife_frame(rom, frame["frame_id"],
                                                          knife_palette)
                frames.append((bmp, frame["duration"], left, top))
                sources.append({"record": frame["frame_id"],
                                "component": f"0x{metadata['component_pointer']:08x}"})
        except ValueError as exc:
            # Multi-component weapon OAM records are not decoded yet.
            unresolved[key] = str(exc)
            continue
        library.add(key, frames)
        animations[key] = {
            "native_index": index,
            "animation_pointer": f"0x{animation['animation_pointer']:08x}",
            "frames": sources,
        }
    totals = library.finish()
    palette_banks = _rom_slice(rom, 0x082097D4, 4, "Soma palette descriptor")[2]
    metadata = {
        "schema": "metroidvania-aos-soma-runtime-v1",
        **totals,
        "soma_animations": sum(key.startswith("Soma/") for key in animations),
        "knife_animations": sum(key.startswith("Knife/") for key in animations),
        "draw_origin_in_cell": list(KNIFE_CELL_ANCHOR),
        "unidentified": {
            "soma_palette_banks": sorted(set(range(1, palette_banks)) - set(banks)),
            "weapons": "only the knife descriptors are verified",
            "semantic_labels": "observed labels cover a subset of animations",
            "animations": unresolved,
        },
        "note": "Private native extraction; source ROM data is never redistributed.",
    }
    library.write_text("animations.json",
                       json.dumps(animations, indent=2, sort_keys=True) + "\n")
    library.write_text("manifest.json",
                       json.dumps(metadata, indent=2, sort_keys=True) + "\n")
    return metadata


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba")
    args = parser.parse_args(argv)
    try:
        result = produce(args.root, args.rom)
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    print("Soma runtime library:", result["soma_animations"], "Soma and",
          result["knife_animations"], "knife animations,", result["frames"],
          "frames,", result["unique_bmps"], "unique BMPs")
    print("Index:", args.root.resolve() / ARIA_SOMA_RUNTIME / "runtime_index.tsv")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
