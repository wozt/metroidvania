#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render original MZM hatch BG metatiles from the owner's private ROM assets.

Zero Mission does not represent these hatches as OAM sprites. Its original
connection.c selects four 16px-high BG1 metatiles, sourced from the shared
0x400-based common tilemap. Graphics and palettes are copied to VRAM during
RoomLoadTileset(). The generated PNGs are PRIVATE and read-only.
"""
from __future__ import annotations

from functools import lru_cache
import json
import struct
import zlib

from scripts.import_game_assets import OUTPUT, ROOT, ROMS, verified_rom, write_generated
from scripts.gba_tiles import bgr555

# Original CLIPDATA_TILEMAP_*_DOOR_TOP_LEFT values from
# metroidret/mzm include/constants/clipdata.h. Right side = +1;
# next vertical metatile = +16.
HATCH_BASES = {
    "gray": 0x90,
    "normal": 0x92,
    "missile": 0x94,
    "super_missile": 0x96,
    "power_bomb": 0x98,
    "no_hatch": 0x9A,
}
COMMON_TILE_COUNT = 128
COMMON_TILE_BASE = 64  # (VRAM_BASE + 0x4800 - BG1_CHARBASE_0x4000) / 32


def png_rgba(width: int, height: int, pixels: bytes) -> bytes:
    if not 0 < width <= 512 or not 0 < height <= 512 or len(pixels) != width * height * 4:
        raise ValueError("invalid private hatch PNG dimensions")
    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))
    rows = b"".join(b"\0" + pixels[y * width * 4:(y + 1) * width * 4]
                    for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 9)) +
            chunk(b"IEND", b""))


@lru_cache(maxsize=1)
def _verified_common_rom() -> bytes:
    path = ROOT / "roms" / ROMS["metroid"][0]
    return verified_rom(path, ROMS["metroid"][1])


@lru_cache(maxsize=1)
def _common_offsets() -> dict:
    database = ROOT / "third_party/mzm/database.json"
    entries = json.loads(database.read_text(encoding="utf-8"))
    return {entry["path"]: entry for entry in entries
            if entry.get("path", "").startswith("common/")}


def common_resource(name: str, expected: int) -> bytes:
    """Use verified private raw extraction, or hash-verified local ROM offsets."""
    if not name.startswith("common/") or "/" in name[7:] or not 0 < expected <= 8192:
        raise ValueError("invalid common resource request")
    path = OUTPUT / "raw/metroid/data" / name
    if path.is_symlink():
        raise ValueError("symlinked MZM private common resource")
    if path.is_file():
        if path.stat().st_size != expected:
            raise ValueError("unexpected common resource size")
        return path.read_bytes()
    entry = _common_offsets()[name]
    offset = int(entry["addr"]["us"], 16)
    size = int(entry["count"], 16) * int(entry["size"])
    rom = _verified_common_rom()
    if size != expected or offset < 0 or offset + size > len(rom):
        raise ValueError("invalid original common resource address/size")
    return rom[offset:offset + size]


def common_inputs(mother_ship: bool = False) -> tuple[bytes, bytes, bytes]:
    tilemap = common_resource("common/common_tilemap.tt", 0x680)
    suffix = "_mother_ship" if mother_ship else ""
    graphics = common_resource(f"common/common_tiles{suffix}.gfx", 0x1000)
    first = common_resource(f"common/common_tiles{suffix}.pal", 32)
    rest = common_resource(f"common/door_transition{suffix}.pal", 0x1E0)
    # RoomLoadTileset copies BG palette rows 0-2 from consecutive resources.
    # First 16 colors are the common palette; the next 32 from transition.
    palette = first + rest[:64]
    return tilemap, graphics, palette


def hatch_metatile_index(style: str, facing_right: bool,
                         state: str = "closed", step: int = 0) -> int:
    """Exact indices used by ConnectionUpdateHatchAnimation in Zero Mission.

    The engine ORs the common-tile flag 0x400 into some frame values;
    this is a Clipdata flag, not part of the metatile table offset.
    """
    if style not in HATCH_BASES or type(facing_right) is not bool:
        raise ValueError("invalid native hatch style/orientation")
    if state == 'closed' and step == 0:
        return HATCH_BASES[style] + int(facing_right)
    base = 0x16 if facing_right else 0x11
    if state == 'opening' and 1 <= step <= 4:
        index = base + step - 1
    elif state == 'closing' and 1 <= step <= 3:
        index = base + 3 - step
        if style != 'no_hatch':
            index += 0x40
    else:
        raise ValueError("unsupported native hatch animation state/frame")
    if style == 'no_hatch':
        index += 0x80
    if not 0 <= index <= 0xCF:
        raise ValueError("hatch frame outside native common tilemap")
    return index


def render_hatch(tilemap: bytes, graphics: bytes, palette: bytes,
                 style: str, facing_right: bool,
                 state: str = "closed", step: int = 0) -> bytes:
    """Authentic 16x64 BG1 hatch pixels, using the game's four metatiles.

    The output is an RGBA PNG with palette-index zero transparent. Refuse
    unknown tile graphics/palette references rather than inventing pixels.
    """
    if style not in HATCH_BASES or not isinstance(facing_right, bool):
        raise ValueError("unrecognized native hatch")
    if len(tilemap) != 0x680 or len(graphics) != 0x1000 or len(palette) != 96:
        raise ValueError("invalid verified MZM common graphics inputs")
    first_index = hatch_metatile_index(style, facing_right, state, step)
    pixels = bytearray(16 * 64 * 4)
    for metatile_y in range(4):
        index = first_index + 16 * metatile_y
        words = struct.unpack_from("<4H", tilemap, index * 8)
        for quarter, word in enumerate(words):
            index_tile = word & 1023
            palette_bank = (word >> 12) & 15
            if not COMMON_TILE_BASE <= index_tile < COMMON_TILE_BASE + COMMON_TILE_COUNT:
                raise ValueError(f"native hatch references non-common GBA tile {index_tile}")
            if palette_bank > 2:
                raise ValueError(f"native hatch references palette bank {palette_bank}")
            flip_x = bool(word & 0x400)
            flip_y = bool(word & 0x800)
            tile_offset = (index_tile - COMMON_TILE_BASE) * 32
            start_x, start_y = (quarter % 2) * 8, metatile_y * 16 + (quarter // 2) * 8
            for dy in range(8):
                for dx in range(8):
                    sx = 7 - dx if flip_x else dx
                    sy = 7 - dy if flip_y else dy
                    packed = graphics[tile_offset + sy * 4 + sx // 2]
                    color_index = (packed >> (4 * (sx & 1))) & 15
                    # Transparent palette entry 0: preserve original BG layers.
                    if color_index == 0:
                        continue
                    color = struct.unpack_from(
                        "<H", palette, (palette_bank * 16 + color_index) * 2)[0]
                    red, green, blue = bgr555(color)
                    pos = ((start_y + dy) * 16 + start_x + dx) * 4
                    pixels[pos:pos + 4] = bytes((red, green, blue, 255))
    return png_rgba(16, 64, bytes(pixels))


def export_hatches() -> int:
    """Generate private previews once, without writing to tracked paths."""
    count = 0
    for mother_ship in (False, True):
        tilemap, graphics, palette = common_inputs(mother_ship)
        for style in HATCH_BASES:
            for facing_right in (False, True):
                name = f"{style}_{'right' if facing_right else 'left'}"
                folder = "mothership" if mother_ship else "zebes"
                path = f"sprite_previews/mzm_hatches/{folder}/{name}.png"
                try:
                    image = render_hatch(tilemap, graphics, palette,
                                         style, facing_right)
                except ValueError:
                    # The common VRAM image may not cover every optional style.
                    # Never invent pixels or block other native room annotations.
                    continue
                write_generated(path, image)
                count += 1
                # Genuine source animation frames. No interpolated graphics:
                # currentAnimationFrame is 1..4 for opening and 1..3 for
                # closing; step 4 of closing returns to the closed tilemap.
                for state, max_step in (('opening', 4), ('closing', 3)):
                    for step in range(1, max_step + 1):
                        animated = f"{name}_{state}_{step}"
                        frame_path = (
                            f"sprite_previews/mzm_hatches/{folder}/{animated}.png")
                        try:
                            frame = render_hatch(tilemap, graphics, palette,
                                                 style, facing_right, state, step)
                        except ValueError:
                            # Some original common graphics are genuinely absent
                            # from the currently decoded family. Preserve the
                            # original closed image as a safe preview fallback.
                            continue
                        write_generated(frame_path, frame)
                        count += 1
    if not count:
        raise ValueError("no genuine common hatch graphics could be decoded")
    return count
