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


# PATCH_0083_ARIA_NATIVE_DECODERS: genuine GBA GfxWrapper and LZ10.
def _gba_lz10(rom: bytes, address: int) -> bytes:
    """Bounded GBA 0x10 decompression with checked backreferences."""
    ptr = _offset(rom, address, 4)
    if rom[ptr] != 0x10:
        raise ValueError('unsupported GBA compressed page header')
    size = int.from_bytes(rom[ptr+1:ptr+4], 'little')
    if size not in (0x2000, 0x2004):
        raise ValueError('unexpected item graphic output size')
    ptr += 4
    out = bytearray()
    while len(out) < size:
        if ptr >= len(rom):
            raise ValueError('truncated LZ10 flags')
        flags = rom[ptr]
        ptr += 1
        for bit in range(7, -1, -1):
            if len(out) == size:
                break
            if flags & (1 << bit):
                if ptr + 2 > len(rom):
                    raise ValueError('truncated LZ10 match')
                pair = (rom[ptr] << 8) | rom[ptr+1]
                ptr += 2
                count = (pair >> 12) + 3
                distance = (pair & 0xFFF) + 1
                if distance > len(out) or len(out) + count > size:
                    raise ValueError('invalid LZ10 backreference')
                for _ in range(count):
                    out.append(out[-distance])
            else:
                if ptr >= len(rom):
                    raise ValueError('truncated LZ10 literal')
                out.append(rom[ptr])
                ptr += 1
    return bytes(out)


def _page(rom: bytes, pointer: int) -> bytes:
    """Decode GBA GfxWrapper: type,u8 bpp,u8 unknown,u8 count512.

    DSVEdit dsvlib/gfx_wrapper.rb defines type 0 raw at +4 and type 1
    compressed via the LE pointer at +4, NOT a 32-bit 0x2000 size word.
    """
    off = _offset(rom, pointer, 4)
    kind, bpp, _unknown, chunks = rom[off:off+4]
    if bpp != 4 or kind not in (0, 1):
        raise ValueError(f'unsupported Aria icon gfx wrapper: type={kind}, bpp={bpp}')
    if kind == 0:
        if chunks != 16:
            raise ValueError(f'unexpected uncompressed Aria icon page size: {chunks}')
        data = rom[_offset(rom, pointer + 4, 0x2000):
                   _offset(rom, pointer + 4, 0x2000) + 0x2000]
    else:
        at = _offset(rom, pointer + 4, 4)
        address = struct.unpack_from('<I', rom, at)[0]
        data = _gba_lz10(rom, address)
        if len(data) == 0x2004:
            # Some source wrappers include four extra bytes in the decompressed
            # length. Accept only an actual recognizable 4-byte gfx header.
            if data[:4] not in (b'\x00\x04\x00\x10', b'\x00\x00\x00\x00'):
                raise ValueError('unrecognized decoded gfx prefix')
            data = data[4:]
    if len(data) != 0x2000:
        raise ValueError('incomplete Aria item graphics page')
    return data


def _palette(rom: bytes, palette_index: int) -> list[int]:
    """AoS icons store their palette bank at bits 8..15, offset by +4."""
    if type(palette_index) is not int or not 0 <= palette_index <= 4:
        raise ValueError('unsupported Aria icon palette number')
    off = _offset(rom, PALETTE, 4)
    a, _b, num_palettes, d = rom[off:off+4]
    if a != 0 or d != 0 or num_palettes <= palette_index:
        raise ValueError('invalid Aria palette wrapper')
    start = _offset(rom, PALETTE + 4 + palette_index * 32, 32)
    return list(struct.unpack_from('<16H', rom, start))


def _png_rgba(width: int, height: int, pixels: bytes) -> bytes:
    def chunk(tag: bytes, body: bytes) -> bytes:
        return (struct.pack(">I", len(body)) + tag + body +
                struct.pack(">I", zlib.crc32(tag + body) & 0xffffffff))
    raw = b"".join(b"\x00" + pixels[y * width * 4:(y + 1) * width * 4]
                   for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def _decode_icon(rom: bytes, pages: list[bytes], index: int,
                 palette_index: int = 0) -> bytes:
    """Decode one actual 16x16 icon using 1D 4bpp tiles and native palette."""
    if not 0 <= index < 64 * len(pages):
        raise ValueError("unsupported icon index")
    data = pages[index // 64][index % 64 * 128:index % 64 * 128 + 128]
    if len(data) != 128:
        raise ValueError('incomplete icon tile data')
    palette = _palette(rom, palette_index)
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
    """Extract real per-item ROM icons; never write source assets or the ROM."""
    if rom is None:
        rom = verified_rom(DEFAULT_ROM, EXPECTED_SHA1)
    pages = [_page(rom, pointer) for pointer in GFX_BANKS]
    output = Path(root) if root is not None else THUMBNAIL_DIR
    count = 0
    for subtype, pointer, num, stride in ITEM_TABLES:
        for item_id in range(num):
            entry = _offset(rom, pointer + stride * item_id, 4)
            packed = struct.unpack_from('<H', rom, entry + 2)[0]
            if (packed & 0xFF) == 0:
                continue  # AoS icon slot 0 indicates no icon.
            icon_index = (packed & 0xFF) - 1
            palette_index = (packed >> 8) - 4
            if not 0 <= palette_index <= 4 or icon_index >= len(pages) * 64:
                continue
            image = _decode_icon(rom, pages, icon_index, palette_index)
            folder = output / 'items'
            folder.mkdir(parents=True, exist_ok=True)
            dest = folder / f'{subtype:02X}_{item_id:03d}.png'
            # Regenerate older caches: v0081 could have used the wrong palette
            # or 0-based icon index, and must not poison corrected thumbnails.
            if not dest.exists() or dest.read_bytes() != image:
                dest.write_bytes(image)
                count += 1
            if item_id == 0:
                for family in ('pickup', 'hard-mode-pickup', 'all-souls-reward'):
                    representative = output / f'{family}:{subtype:02X}.png'
                    if not representative.exists() or representative.read_bytes() != image:
                        representative.write_bytes(image)
                        count += 1
    return count


if __name__ == "__main__":
    print("Private Aria icon previews generated:", generate_aria_item_thumbnails())
