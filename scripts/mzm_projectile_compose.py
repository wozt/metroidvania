# SPDX-License-Identifier: GPL-3.0-only
"""Compose Zero Mission projectile sprites from the native frame tables.

The staging mirrors the game:

* ``HudGenericLoadCommonSpriteGfx`` copies ``sCommonSpritesGfx`` to OBJ VRAM
  offset 0x800 and ``sCommonSpritesPal`` fills OBJ palette rows 2-7;
* ``ProjectileLoadGraphics`` copies the active beam set's four 512-byte
  blocks to OBJ offsets 0x1000/0x1400/0x1800/0x1C00 (the Pistol's first block
  is half-sized) and patches colors 0-5 of OBJ palette row 2 with the beam
  set's row of ``sBeamPal``;
* ``ProjectileDraw`` draws each ``FrameData`` frame relative to the
  projectile position and mirrors it around that point for X/Y flips.

Every non-particle, non-unused ``FrameData`` table of ``projectile_data.c`` is
exported in its four flip states with native frame durations.
"""
from __future__ import annotations

import re
import struct

from scripts.gba_oam import unpack_oam
from scripts.gba_tiles import bgr555
from scripts.mzm_samus_frame import rom_offset

COMMON_GFX_VRAM = 0x800
BEAM_VRAM = (0x1000, 0x1400, 0x1800, 0x1C00)
BEAM_BLOCK_BYTES = 512
# Beam set -> (graphics prefix, sBeamPal row) from ProjectileLoadGraphics.
BEAM_SETS = {
    "NormalBeam": ("sNormalBeamGfx", 0),
    "LongBeam": ("sLongBeamGfx", 1),
    "IceBeam": ("sIceBeamGfx", 2),
    "WaveBeam": ("sWaveBeamGfx", 3),
    "PlasmaBeam": ("sPlasmaBeamGfx", 4),
    "Pistol": ("sPistolGfx", 5),
}
FLIPS = {"none": (False, False), "x": (True, False), "y": (False, True),
         "xy": (True, True)}
FRAME_TABLE_RE = re.compile(r"^const\s+struct\s+FrameData\s+(s\w+Oam_\w+)\s*\[", re.M)


def frame_tables(source: str) -> list[str]:
    """Projectile FrameData tables, excluding particles and unused data."""
    names = FRAME_TABLE_RE.findall(source)
    return [name for name in names
            if not name.startswith("sParticle") and "Unused" not in name]


def beam_set(table: str) -> str:
    """Beam graphics set a table is drawn with; missiles and bombs use the
    common graphics, rendered with the default Power Beam palette row."""
    name = table[1:]
    if name.startswith("Charged"):
        name = name[len("Charged"):]
    for beam in BEAM_SETS:
        if name.startswith(beam):
            return beam
    return "NormalBeam"


def _slice(rom: bytes, pointer: int, size: int, label: str) -> bytes:
    offset = rom_offset(pointer, rom)
    if offset + size > len(rom):
        raise ValueError("truncated " + label)
    return rom[offset:offset + size]


def stage(rom: bytes, symbols, beam: str) -> tuple[bytes, list]:
    """OBJ VRAM and the 256-color OBJ palette for one beam set."""
    vram = bytearray(0x8000)
    address, size = symbols["sCommonSpritesGfx"]
    vram[COMMON_GFX_VRAM:COMMON_GFX_VRAM + size] = _slice(rom, address, size,
                                                          "common sprite graphics")
    prefix, row = BEAM_SETS[beam]
    for part, base in zip(("Top", "Bottom", "Charged_Top", "Charged_Bottom"), BEAM_VRAM):
        length = BEAM_BLOCK_BYTES // 2 if beam == "Pistol" and part == "Top" else BEAM_BLOCK_BYTES
        vram[base:base + length] = _slice(rom, symbols[f"{prefix}_{part}"][0], length,
                                          f"{prefix}_{part}")
    palette = [None] * 256
    address, size = symbols["sCommonSpritesPal"]
    common = _slice(rom, address, size, "common sprite palette")
    for index in range(size // 2):
        palette[32 + index] = bgr555(struct.unpack_from("<H", common, index * 2)[0])
    beams = _slice(rom, symbols["sBeamPal"][0], symbols["sBeamPal"][1], "beam palette")
    for index in range(6):
        palette[32 + index] = bgr555(struct.unpack_from("<H", beams, (row * 16 + index) * 2)[0])
    return bytes(vram), palette


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


def render(rom: bytes, vram: bytes, palette, pointer: int, flip: str):
    """Pixels relative to the projectile position for one OAM frame."""
    count = struct.unpack("<H", _slice(rom, pointer, 2, "projectile OAM"))[0]
    if not 1 <= count <= 32:
        raise ValueError("invalid projectile OAM part count")
    raw = _slice(rom, pointer + 2, count * 6, "projectile OAM entries")
    entries = [unpack_oam(struct.pack("<HHHH", *struct.unpack_from("<HHH", raw, i * 6), 0))
               for i in range(count)]
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


def compose_all(rom: bytes, source: str, symbols, sink) -> dict:
    """Send every projectile table and flip state to ``sink(key, frames, info)``.

    ``frames`` are ``(pixels, duration)`` tuples with origin-relative pixels.
    """
    report = {"tables": 0, "sequences": 0, "unresolved": {}}
    staged = {}
    for table in frame_tables(source):
        report["tables"] += 1
        beam = beam_set(table)
        try:
            if beam not in staged:
                staged[beam] = stage(rom, symbols, beam)
            vram, palette = staged[beam]
            entries = frames(rom, symbols, table)
            for flip in FLIPS:
                rendered = [(render(rom, vram, palette, pointer, flip), duration)
                            for pointer, duration in entries]
                if any(not pixels for pixels, _ in rendered):
                    raise ValueError("projectile frame has no visible pixels")
                sink(f"Projectile/{table[1:]}/{flip}", rendered,
                     {"table": table, "beam_set": beam, "flip": flip})
                report["sequences"] += 1
        except (ValueError, KeyError) as exc:
            report["unresolved"][table] = str(exc)
    return report
