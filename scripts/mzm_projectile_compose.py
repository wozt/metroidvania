# SPDX-License-Identifier: GPL-3.0-only
"""Zero Mission FrameData, OAM and render helpers shared by the sprite tools.

``ProjectileDraw`` and ``SpriteDraw`` draw each frame relative to the
object's position and mirror it around that point for X/Y flips. The
projectile library itself is written by the native importer
(``fusion_importer --mzm-export-samus``).
"""
from __future__ import annotations

import struct

from scripts.gba_oam import unpack_oam
from scripts.mzm_samus_frame import rom_offset

FLIPS = {"none": (False, False), "x": (True, False), "y": (False, True),
         "xy": (True, True)}


def _slice(rom: bytes, pointer: int, size: int, label: str) -> bytes:
    offset = rom_offset(pointer, rom)
    if offset + size > len(rom):
        raise ValueError("truncated " + label)
    return rom[offset:offset + size]


def frames(rom: bytes, symbols, table: str) -> list[tuple[int, int]]:
    """``(oam pointer, duration)`` entries up to the FrameData terminator."""
    address, size = symbols[table]
    result = []
    for index in range(size // 8):
        pointer, timer = struct.unpack("<IB", _slice(rom, address + index * 8, 5, table))
        if pointer == 0 and timer == 0:
            break
        if not 1 <= timer <= 255:
            raise ValueError(f"invalid frame duration in {table}")
        result.append((pointer, timer))
    if not result:
        raise ValueError(table + " has no frames")
    return result


def oam_entries(rom: bytes, pointer: int) -> list[dict]:
    """Decode the non-affine OBJ entries stored by one native frame."""
    count = struct.unpack("<H", _slice(rom, pointer, 2, "projectile OAM"))[0]
    # At most the 128 hardware OAM entries (the glass tube uses over 32).
    if not 1 <= count <= 128:
        raise ValueError("invalid projectile OAM part count")
    raw = _slice(rom, pointer + 2, count * 6, "projectile OAM entries")
    return [unpack_oam(struct.pack("<HHHH", *struct.unpack_from("<HHH", raw, i * 6), 0))
            for i in range(count)]


def object_bounds(rom: bytes, pointer: int, flip: str) -> tuple[int, int, int, int, int]:
    """Return the full OAM box union after ``SpriteDraw``'s global flip.

    Unlike the cropped bitmap bounds, these include transparent edge pixels.
    Affine OBJ hardware clips rotated pixels to these object rectangles.
    """
    entries = oam_entries(rom, pointer)
    flip_x, flip_y = FLIPS[flip]
    boxes = []
    for entry in entries:
        x = -entry["x"] - entry["w"] if flip_x else entry["x"]
        y = -entry["y"] - entry["h"] if flip_y else entry["y"]
        boxes.append((x, y, x + entry["w"], y + entry["h"]))
    left = min(box[0] for box in boxes)
    top = min(box[1] for box in boxes)
    right = max(box[2] for box in boxes)
    bottom = max(box[3] for box in boxes)
    return left, top, right - left, bottom - top, len(entries)


def render_affine_whole_part(rom: bytes, vram: bytes, palette, pointer: int,
                             part: int):
    """Render one OBJ as affine-whole mode reads it.

    In affine mode the OAM flip bits become matrix-number bits. The native
    whole-sprite path supports an X-flipped matrix separately; callers in
    this pipeline currently require unflipped parts and reject that case.
    The original Y flip is always ignored.
    """
    entries = oam_entries(rom, pointer)
    if not 0 <= part < len(entries):
        raise ValueError("OAM part index outside frame")
    entry = dict(entries[part])
    if entry["hflip"]:
        raise ValueError("affine-whole X-flipped OBJ is not supported")
    entry["vflip"] = False
    return _render_entries(vram, palette, [entry], "none")


def affine_whole_part_bounds(rom: bytes, pointer: int, part: int
                             ) -> tuple[int, int, int, int, int]:
    """Full unrotated box of one affine-whole OBJ."""
    entries = oam_entries(rom, pointer)
    if not 0 <= part < len(entries):
        raise ValueError("OAM part index outside frame")
    entry = entries[part]
    if entry["hflip"]:
        raise ValueError("affine-whole X-flipped OBJ is not supported")
    return entry["x"], entry["y"], entry["w"], entry["h"], 1


def _render_entries(vram: bytes, palette, entries: list[dict], flip: str):
    """Render decoded OAM entries relative to their sprite origin."""
    flip_x, flip_y = FLIPS[flip]
    pixels = {}
    for entry in reversed(entries):  # OAM slot zero has the highest priority.
        for iy in range(entry["h"]):
            for ix in range(entry["w"]):
                sx = entry["w"] - 1 - ix if entry["hflip"] else ix
                sy = entry["h"] - 1 - iy if entry["vflip"] else iy
                tile = entry["tile"] + (sy // 8) * 32 + sx // 8
                offset = tile * 32 + (sy % 8) * 4 + (sx % 8) // 2
                if offset >= len(vram):
                    raise ValueError("projectile tile outside OBJ VRAM")
                index = (vram[offset] >> (4 * (sx & 1))) & 15
                if not index:
                    continue
                color = palette[entry["bank"] * 16 + index]
                if color is None:
                    raise ValueError("projectile uses an unloaded palette row")
                x, y = entry["x"] + ix, entry["y"] + iy
                # ProjectileDraw mirrors parts around the projectile position.
                pixels[(-1 - x if flip_x else x, -1 - y if flip_y else y)] = color
    return pixels


def render(rom: bytes, vram: bytes, palette, pointer: int, flip: str):
    """Pixels relative to the projectile position for one OAM frame."""
    entries = oam_entries(rom, pointer)
    return _render_entries(vram, palette, entries, flip)
