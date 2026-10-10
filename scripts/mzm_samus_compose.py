# SPDX-License-Identifier: GPL-3.0-only
"""Compose every native Zero Mission Samus animation from the source tables.

The selection mirrors ``SamusUpdateGraphicsOam`` and ``SamusUpdatePalette``
in the pinned mzm decompilation:

* body animations come from ``sSamusAnimPointers_<PowerSuit|FullSuit|Suitless>``
  tables, either the per-pose table or a per-selector table such as
  ``..._Standing[ACD_*]``;
* the arm cannon animation comes from the matching
  ``sArmCannonAnimPointers_<Suit|Suitless>`` table with the same selector, or
  from ``..._All[pose]`` when no dedicated table exists (Skidding, Screw
  Attack);
* the arm cannon graphics come from ``sArmCannonGfxPointers_*[acd]`` using
  the running-right, hanging, zipline and default rules of the source;
* the palette is the first two banks of the suit's default palette. Varia
  reuses Power Suit bodies and Gravity reuses Full Suit bodies, exactly as the
  source chooses graphics by suit type and palettes by suit flags.

Each frame is rendered once into OBJ palette indices and colorized for every
visual suit that shares its graphics. Frames are cropped to visible pixels and
carry their native draw offset relative to Samus's position, including the two
pixel downward shift applied by ``SamusDraw``. Output stays private.
"""
from __future__ import annotations

import hashlib
import re
import struct
from dataclasses import dataclass

from scripts.gba_oam import unpack_oam
from scripts.gba_tiles import bgr555
from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset, stage
from scripts.sprite_library import bmp_from_pixels

ANIMATION_RECORD_BYTES = 16
CANNON_RECORD_BYTES = 8
CANNON_GFX_BYTES = 64
CANNON_UPPER_VRAM = 0x800
CANNON_LOWER_VRAM = 0xC00
# SamusDraw adds SUB_PIXEL_TO_PIXEL(EIGHTH_BLOCK_SIZE) to the Y position.
DRAW_Y_OFFSET = 2
MAX_FRAMES = 256

# Visual suit -> (body table family, cannon table family, palette symbol).
VISUAL_SUITS = {
    "PowerSuit": ("PowerSuit", "Suit", "sSamusPal_PowerSuit_Default"),
    "VariaSuit": ("PowerSuit", "Suit", "sSamusPal_VariaSuit_Default"),
    "FullSuit": ("FullSuit", "Suit", "sSamusPal_FullSuit_Default"),
    "GravitySuit": ("FullSuit", "Suit", "sSamusPal_GravitySuit_Default"),
    "Suitless": ("Suitless", "Suitless", "sSamusPal_Suitless_Default"),
}
ACD_SELECTORS = ("ACD_FORWARD", "ACD_DIAGONALLY_UP", "ACD_DIAGONALLY_DOWN",
                 "ACD_UP", "ACD_DOWN")
# Table name -> native pose, only where the cannon graphics rule depends on it
# or where no dedicated cannon table exists.
TABLE_POSES = {
    "Running": "SPOSE_RUNNING",
    "Running_Speedboosting": "SPOSE_RUNNING",
    "Skidding": "SPOSE_SKIDDING",
    "ScrewAttacking": "SPOSE_SCREW_ATTACKING",
    "AimingWhileHanging": "SPOSE_AIMING_WHILE_HANGING",
    "ShootingWhileHanging": "SPOSE_SHOOTING_WHILE_HANGING",
    "OnZipline": "SPOSE_ON_ZIPLINE",
    "ShootingOnZipline": "SPOSE_SHOOTING_ON_ZIPLINE",
}
# Selectors spelled differently in paired tables but with equal values.
SELECTOR_ALIASES = {
    "FALSE": "0", "TRUE": "1",
    "FORCED_MOVEMENT_CRAWLING_ARM_CANNON_DOWN": "0",
    "FORCED_MOVEMENT_CRAWLING_ARM_CANNON_UP": "1",
}
HANGING_POSES = {"SPOSE_AIMING_WHILE_HANGING", "SPOSE_SHOOTING_WHILE_HANGING"}
ZIPLINE_POSES = {"SPOSE_ON_ZIPLINE", "SPOSE_SHOOTING_ON_ZIPLINE"}

TABLE_RE = re.compile(
    r"const\s+(?:struct\s+\w+|u8)\s*\*\s*const\s+"
    r"(sSamusAnimPointers_\w+|sArmCannonAnimPointers_\w+|sArmCannonGfxPointers_\w+)"
    r"\s*(?:\[[^\]]*\])+\s*=\s*\{")
ROW_RE = re.compile(r"\[\s*([A-Za-z0-9_]+)\s*\]\s*=\s*(\{[^{}]*\}|[A-Za-z0-9_]+)")
SYMBOL_RE = re.compile(r"\b(?:sSamusAnim|sArmCannonAnim|sArmCannonGfx)_\w+\b")
NM_RE = re.compile(r"^([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+[A-Za-z]\s+(\S+)$")


def _matching_brace(text: str, start: int) -> int:
    depth = 0
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("unclosed pointer table")


def parse_tables(source: str) -> dict[str, dict[str, list[str]]]:
    """Return ``{table: {selector: [symbols]}}`` for the Samus pointer tables."""
    tables: dict[str, dict[str, list[str]]] = {}
    for match in TABLE_RE.finditer(source):
        name = match.group(1)
        start = source.index("{", match.end() - 1)
        body = source[start + 1:_matching_brace(source, start)]
        rows = {}
        for row in ROW_RE.finditer(body):
            symbols = SYMBOL_RE.findall(row.group(2))
            if symbols:
                if row.group(1) in rows:
                    raise ValueError(f"duplicate selector {row.group(1)} in {name}")
                rows[row.group(1)] = symbols
        if name in tables:
            raise ValueError("duplicate table " + name)
        if rows:
            tables[name] = rows
    if not any(name.startswith("sSamusAnimPointers_") for name in tables):
        raise ValueError("no Samus animation pointer tables found")
    return tables


def parse_symbols(nm_output: str) -> dict[str, tuple[int, int]]:
    """Sized ELF symbols relevant to Samus graphics (``nm -S`` output)."""
    found: dict[str, tuple[int, int]] = {}
    for line in nm_output.splitlines():
        match = NM_RE.fullmatch(line.strip())
        if not match:
            continue
        address, size, name = match.groups()
        if not name.startswith(("sSamusAnim_", "sArmCannonAnim_",
                                "sArmCannonGfx_", "sSamusPal_")):
            continue
        value = (int(address, 16), int(size, 16))
        if name in found and found[name] != value:
            raise ValueError("conflicting ELF symbol " + name)
        found[name] = value
    return found


def verify_symbols_against_rom(rom: bytes, image: bytes, symbols) -> None:
    """Fail unless every consumed ELF symbol holds the same bytes as the ROM.

    ``image`` is the reference ELF converted with ``objcopy -O binary``. The
    two files differ outside the Samus data (header logo and unrelated
    blocks), so only the symbol ranges the composer reads are compared.
    """
    mismatched = []
    for name, (address, size) in sorted(symbols.items()):
        start = rom_offset(address, rom)
        if start + size > len(rom) or start + size > len(image):
            mismatched.append(name)
        elif rom[start:start + size] != image[start:start + size]:
            mismatched.append(name)
    if mismatched:
        raise ValueError("reference ELF does not match the ROM for: " +
                         ", ".join(mismatched[:5]))


@dataclass(frozen=True)
class Variant:
    family: str          # PowerSuit, FullSuit or Suitless graphics
    table: str           # "pose" or the per-selector table suffix
    selector: str
    side: str            # right or left
    body: str
    cannon: str | None
    cannon_gfx: tuple[str, str] | None
    pose: str | None


def _cannon_gfx(tables, pose: str | None, acd: str, side: str):
    right = side == "right"
    if pose in HANGING_POSES:
        family = f"{'Right' if right else 'Left'}_Hanging"
    elif pose in ZIPLINE_POSES and right:
        family = "Right_OnZipline"
    elif pose == "SPOSE_RUNNING" and right:
        family = "Standing"
    else:
        family = f"{'Right' if right else 'Left'}_Default"
    result = []
    for part in ("Upper", "Lower"):
        row = tables.get(f"sArmCannonGfxPointers_{part}_{family}", {}).get(acd)
        if not row:
            return None
        result.append(row[0])
    return tuple(result)


def build_variants(tables) -> tuple[list[Variant], list[str]]:
    variants: list[Variant] = []
    problems: list[str] = []
    for family in ("PowerSuit", "FullSuit", "Suitless"):
        cannon_family = "Suitless" if family == "Suitless" else "Suit"
        prefix = f"sSamusAnimPointers_{family}"
        for name in sorted(t for t in tables if t == prefix or t.startswith(prefix + "_")):
            suffix = name[len(prefix) + 1:] if name != prefix else "pose"
            cannon_all = tables.get(f"sArmCannonAnimPointers_{cannon_family}_All", {})
            cannon_table = tables.get(
                f"sArmCannonAnimPointers_{cannon_family}_{suffix}") if suffix != "pose" else None
            for selector, symbols in sorted(tables[name].items()):
                pose = selector if suffix == "pose" else TABLE_POSES.get(suffix)
                acd = selector if selector in ACD_SELECTORS else "ACD_FORWARD"
                for index, side in enumerate(("right", "left")):
                    if index >= len(symbols):
                        problems.append(f"{name}[{selector}] has no {side} entry")
                        continue
                    if cannon_table is not None:
                        cannon_row = cannon_table.get(selector)
                        if cannon_row is None:
                            wanted = SELECTOR_ALIASES.get(selector, selector)
                            cannon_row = next(
                                (row for key, row in cannon_table.items()
                                 if SELECTOR_ALIASES.get(key, key) == wanted), None)
                    else:
                        cannon_row = cannon_all.get(pose) if pose else None
                    cannon = (cannon_row[index]
                              if cannon_row and index < len(cannon_row) else None)
                    if cannon is None:
                        problems.append(f"{name}[{selector}] {side}: no native arm cannon table")
                    variants.append(Variant(
                        family, suffix, selector, side, symbols[index], cannon,
                        _cannon_gfx(tables, pose, acd, side), pose))
    return variants, problems


def _slice(rom: bytes, pointer: int, size: int, label: str) -> bytes:
    offset = rom_offset(pointer, rom)
    if offset + size > len(rom):
        raise ValueError("truncated " + label)
    return rom[offset:offset + size]


def _oam_entries(rom: bytes, pointer: int) -> tuple[int, list[dict]]:
    header = struct.unpack("<H", _slice(rom, pointer, 2, "OAM header"))[0]
    count = header & 0xFFF
    if count > 128:
        raise ValueError("excessive OAM part count")
    raw = _slice(rom, pointer + 2, count * 6, "OAM entries") if count else b""
    entries = []
    for index in range(count):
        a0, a1, a2 = struct.unpack_from("<HHH", raw, index * 6)
        entries.append(unpack_oam(struct.pack("<HHHH", a0, a1, a2, 0)))
    return header, entries


def body_frame_count(rom: bytes, address: int, size: int) -> list[int]:
    """Durations of the zero-terminated native body animation."""
    if size % ANIMATION_RECORD_BYTES or not size:
        raise ValueError("invalid body animation size")
    durations = []
    for index in range(min(size // ANIMATION_RECORD_BYTES, MAX_FRAMES)):
        record = _slice(rom, address + index * ANIMATION_RECORD_BYTES,
                        ANIMATION_RECORD_BYTES, "body animation")
        if record[12] == 0:
            break
        durations.append(record[12])
    if not durations:
        raise ValueError("body animation has no frames")
    return durations


def render_indices(rom: bytes, symbols, variant: Variant, frame: int):
    """Render one frame into ``{(x, y): palette index}`` in OAM coordinates."""
    body_address = symbols[variant.body][0] + frame * ANIMATION_RECORD_BYTES
    vram, metadata = stage(rom, body_address)
    _, body = _oam_entries(rom, metadata["oam_pointer"])
    entries = list(body)
    cannon_parts = 0
    if variant.cannon is not None and variant.pose != "SPOSE_DYING":
        cannon_address = symbols[variant.cannon][0] + frame * CANNON_RECORD_BYTES
        _, cannon_oam = struct.unpack(
            "<II", _slice(rom, cannon_address, CANNON_RECORD_BYTES, "cannon record"))
        header, cannon = _oam_entries(rom, cannon_oam)
        if cannon:
            if variant.cannon_gfx is None:
                raise ValueError("arm cannon OAM without native graphics table")
            vram = bytearray(vram)
            for name, base in zip(variant.cannon_gfx,
                                  (CANNON_UPPER_VRAM, CANNON_LOWER_VRAM)):
                vram[base:base + CANNON_GFX_BYTES] = _slice(
                    rom, symbols[name][0], CANNON_GFX_BYTES, "cannon graphics")
            # SamusDraw order: cannon in front, body, cannon behind.
            entries = ((cannon if header & 0x1000 else []) + list(body) +
                       (cannon if header & 0x2000 else []))
            cannon_parts = len(cannon)
    pixels: dict[tuple[int, int], int] = {}
    for entry in reversed(entries):  # OAM slot zero has the highest priority.
        if entry["bank"] > 1:
            raise ValueError("OAM uses a palette bank outside Samus's two rows")
        for iy in range(entry["h"]):
            for ix in range(entry["w"]):
                sx = entry["w"] - 1 - ix if entry["hflip"] else ix
                sy = entry["h"] - 1 - iy if entry["vflip"] else iy
                tile = entry["tile"] + (sy // 8) * 32 + sx // 8
                offset = tile * 32 + (sy % 8) * 4 + (sx % 8) // 2
                if offset >= len(vram):
                    raise ValueError("OBJ tile outside VRAM")
                index = (vram[offset] >> (4 * (sx & 1))) & 15
                if index:
                    pixels[(entry["x"] + ix, entry["y"] + iy)] = entry["bank"] * 16 + index
    return pixels, metadata["duration"], cannon_parts


def palette_colors(rom: bytes, symbols, name: str) -> list[tuple[int, int, int]]:
    address, size = symbols[name]
    if size < 64:
        raise ValueError("palette symbol too small: " + name)
    raw = _slice(rom, address, 64, name)
    return [bgr555(struct.unpack_from("<H", raw, i * 2)[0]) for i in range(32)]


def to_bmp(pixels, colors) -> tuple[bytes, int, int]:
    """Cropped 32-bit BGRA BMP plus its top-left OAM coordinate."""
    return bmp_from_pixels({point: colors[index] for point, index in pixels.items()})


def variant_key(visual: str, variant: Variant) -> str:
    return f"{visual}/{variant.table}/{variant.selector}/{variant.side}"


def compose_all(rom: bytes, tables, symbols, sink) -> dict:
    """Compose every variant for every visual suit and return a report.

    ``sink(key, frames, info)`` receives ``frames`` as a list of
    ``(bmp, duration, offset_x, offset_y)`` tuples.
    """
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError("original Zero Mission USA ROM SHA-1 mismatch")
    variants, problems = build_variants(tables)
    palettes = {visual: palette_colors(rom, symbols, palette)
                for visual, (_, _, palette) in VISUAL_SUITS.items()}
    report = {"variants": len(variants), "sequences": 0, "frames": 0,
              "unresolved": {}, "table_problems": problems}
    for variant in variants:
        label = f"{variant.family}/{variant.table}/{variant.selector}/{variant.side}"
        try:
            needed = [variant.body]
            needed += [variant.cannon] if variant.cannon else []
            needed += list(variant.cannon_gfx or ())
            missing = [name for name in needed if name not in symbols]
            if missing:
                raise ValueError("missing ELF symbols " + ", ".join(missing))
            durations = body_frame_count(rom, *symbols[variant.body])
            rendered = [render_indices(rom, symbols, variant, index)
                        for index in range(len(durations))]
            if any(not pixels for pixels, _, _ in rendered):
                raise ValueError("frame has no visible pixels")
        except (ValueError, KeyError) as exc:
            report["unresolved"][label] = str(exc)
            continue
        for visual, (family, _, palette) in VISUAL_SUITS.items():
            if family != variant.family:
                continue
            frames = []
            for pixels, duration, _ in rendered:
                bmp, left, top = to_bmp(pixels, palettes[visual])
                frames.append((bmp, duration, left, top + DRAW_Y_OFFSET))
            report["sequences"] += 1
            report["frames"] += len(frames)
            sink(variant_key(visual, variant), frames, {
                "body": variant.body, "cannon": variant.cannon,
                "cannon_gfx": list(variant.cannon_gfx or ()),
                "cannon_parts": [parts for _, _, parts in rendered],
                "palette": palette})
    return report
