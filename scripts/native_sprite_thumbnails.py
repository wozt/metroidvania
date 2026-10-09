# SPDX-License-Identifier: GPL-3.0-only
"""Private Aria of Sorrow 16x16 item-icons, generated only from verified local ROM.

Based on native US ROM item table and icon graphics addresses documented by
LagoLunatic/DSVEdit (aos_constants.rb). The decoder deliberately fails closed
when graphics page format is unknown. No assets are checked into Git.
"""
from __future__ import annotations
import struct
import zlib
from pathlib import Path

from scripts.import_game_assets import OUTPUT, verified_rom
from scripts.import_aos_world import DEFAULT_ROM, EXPECTED_SHA1, GBA_ROM_BASE

# DSVEdit constants/aos_constants.rb (USA only)
GFX_BANKS = (0x081C5E00, 0x081C7E04, 0x081C9E08)
PALETTE = 0x082099FC
ITEM_TABLES = ((2, 0x08505B3C, 32, 16),
               (3, 0x08505D3C, 59, 28),
               (4, 0x085063B0, 45, 20))
THUMBNAIL_DIR = OUTPUT / "sprite_previews" / "aria"


def _offset(rom: bytes, pointer: int, size: int) -> int:
    pos = pointer - GBA_ROM_BASE
    if pos < 0 or pos + size > len(rom):
        raise ValueError("icon graphics outside verified Aria ROM")
    return pos


def _page(rom: bytes, pointer: int) -> bytes:
    """Only accept a native 0x2000-byte raw gfx page with an explicit size word.

    This intentionally does not guess at undocumented compression wrappers.
    """
    off = _offset(rom, pointer, 4)
    size = struct.unpack_from("<I", rom, off)[0]
    if size != 0x2000:
        raise ValueError("unsupported Aria icon graphics bank wrapper")
    off = _offset(rom, pointer + 4, size)
    return rom[off:off + size]


def _png_rgba(width: int, height: int, pixels: bytes) -> bytes:
    def chunk(tag: bytes, body: bytes) -> bytes:
        return (struct.pack(">I", len(body)) + tag + body +
                struct.pack(">I", zlib.crc32(tag + body) & 0xffffffff))
    raw = b"".join(b"\x00" + pixels[y * width * 4:(y + 1) * width * 4]
                   for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def _decode_icon(rom: bytes, pages: list[bytes], index: int) -> bytes:
    if not 0 <= index < 64 * len(pages):
        raise ValueError("unsupported icon index")
    data = pages[index // 64][index % 64 * 128:index % 64 * 128 + 128]
    palette_offset = _offset(rom, PALETTE, 32)
    palette = [struct.unpack_from("<H", rom, palette_offset + i * 2)[0]
               for i in range(16)]
    pixels = bytearray()
    for y in range(16):
        for x in range(16):
            tile = (y // 8) * 2 + (x // 8)
            position = tile * 32 + (y % 8) * 4 + (x % 8) // 2
            code = data[position] >> (4 if x % 2 else 0) & 15
            rgb = palette[code]
            pixels.extend(((rgb & 31) * 255 // 31,
                           (rgb >> 5 & 31) * 255 // 31,
                           (rgb >> 10 & 31) * 255 // 31,
                           0 if code == 0 else 255))
    return _png_rgba(16, 16, bytes(pixels))


def generate_aria_item_thumbnails(rom: bytes | None = None,
                                   root: Path | None = None) -> int:
    if rom is None:
        rom = verified_rom(DEFAULT_ROM, EXPECTED_SHA1)
    pages = [_page(rom, pointer) for pointer in GFX_BANKS]
    output = Path(root) if root is not None else THUMBNAIL_DIR
    # Generate representative item icons for native *subtype* dropdown rows;
    # do not claim to know a particular item without an item-id parameter.
    count = 0
    for subtype, pointer, num, stride in ITEM_TABLES:
        for item_id in range(num):
            entry = _offset(rom, pointer + stride * item_id, 4)
            icon_index = struct.unpack_from("<H", rom, entry + 2)[0]
            if icon_index >= len(pages) * 64:
                continue  # Unknown icon bank -> honest placeholder.
            image = _decode_icon(rom, pages, icon_index)
            folder = output / "items"
            folder.mkdir(parents=True, exist_ok=True)
            dest = folder / f"{subtype:02X}_{item_id:03d}.png"
            if not dest.exists():
                dest.write_bytes(image)
                count += 1
            if item_id == 0:
                for family in ("pickup", "hard-mode-pickup", "all-souls-reward"):
                    representative = output / f"{family}:{subtype:02X}.png"
                    if not representative.exists():
                        representative.write_bytes(image)
                        count += 1
    return count


if __name__ == "__main__":
    print("Private Aria icon previews generated:", generate_aria_item_thumbnails())
