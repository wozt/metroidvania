#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Zero Mission room backgrounds as the game loads them.

``RoomLoadTileset`` and ``RoomLoadBackgrounds`` are reproduced on a model of
BG VRAM and BG palette RAM:

* VRAM 0x4800: ``sCommonTilesGfx`` (doors, hatches, breakable blocks, tanks);
  VRAM 0x5800: the tileset graphics (LZ77); VRAM ``0xFDE0 - size``: the
  tileset's background graphics (LZ77, BG3 and LZ77 BG0), 0xFFE0 cleared.
* Palette rows 0-2: ``sCommonTilesPal`` and the start of
  ``sDoorTransitionPal``, which follows it in ROM (without the first color,
  the black backdrop); rows 3-15: rows 1-13 of the tileset palette.
* BG0 (RLE), BG1 and BG2 draw 16x16 blocks: a block value below
  ``CLIPDATA_TILEMAP_FLAG`` indexes the tileset metatiles (``gTilemap``, from
  ``pTilemap + 2``), a value above it ``sCommonTilemap`` (``gCommonTilemap``
  follows ``gTilemap`` in RAM); tile numbers are relative to character base
  1 (VRAM 0x4000).
* BG3 and an LZ77 BG0 are GBA text tilemaps (``bgSize`` from the data's
  first byte, the LZ77 stream at +4) on character base 2 (VRAM 0x8000).

Color 0 of each tile row is transparent. The per-tileset metatile sheet
(32 blocks per row: the 1024 tileset metatiles, then the 208 common ones)
lets the runtime redraw any block, as the game does when hatches open and
blocks break. Every resource is read from the private raw extraction
(``assets/extracted/metroid/raw/data``) through the pinned decompilation's
``#include`` paths; nothing is stored in this repository.
"""
from __future__ import annotations

from functools import lru_cache
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import mzm_room_render as render  # noqa: E402
from scripts.gba_tiles import bgr555  # noqa: E402

DECOMP = ROOT / "third_party/mzm"
TILEMAP_FLAG = 1 << 10          # CLIPDATA_TILEMAP_FLAG
COMMON_METATILES = 208          # sCommonTilemap[832] / 4
TILESET_METATILES = 1024        # gTilemap (0x2000 bytes)
SHEET_COLUMNS = 32
SHEET_BLOCKS = TILESET_METATILES + COMMON_METATILES
INCLUDE_RE = re.compile(
    r"const\s+u(?:8|16|32)\s+(s\w+)\s*\[[^\]]*\]\s*=\s*\{\s*#include\s+\"extracted/data/([^\"]+)\.inc\"")
# GBA text background sizes by BGCNT size (TransparencyGetBgSizeFlag: 3 and
# above become 512x256).
BG_SIZES = {0: (256, 256), 1: (512, 256), 2: (256, 512)}


@lru_cache(maxsize=1)
def resource_paths(decomp: Path = DECOMP) -> dict[str, str]:
    """Data symbol -> raw path below raw/data, from the #include lines."""
    paths = {}
    for source in sorted((decomp / "src/data").rglob("*.c")):
        for name, path in INCLUDE_RE.findall(source.read_text(encoding="utf-8")):
            paths[name] = path
    return paths


def symbol_bytes(symbol: str) -> bytes:
    path = resource_paths().get(symbol)
    if path is None:
        raise ValueError(f"no extracted resource for {symbol}")
    return render.read_raw(path)


def tileset_entry(index: int) -> dict[str, str]:
    source = render.TILESET_TABLE.read_text(encoding="utf-8")
    table = source.split("const struct TilesetEntry sTilesetEntries[79] = {", 1)[1]
    entry = re.search(rf"\[{index}\]\s*=\s*\{{(.*?)\n\s*\}}", table, re.S)
    if entry is None:
        raise ValueError(f"missing tileset entry {index}")
    return dict(re.findall(r"\.(\w+)\s*=\s*(\w+)", entry.group(1)))


@lru_cache(maxsize=79)
def tileset_video_memory(index: int) -> tuple[bytes, bytes]:
    """Raw BG VRAM and palette RAM after the standard RoomLoadTileset path."""
    entry = tileset_entry(index)
    vram = bytearray(0x10000)
    common = symbol_bytes("sCommonTilesGfx")
    vram[0x4800:0x4800 + len(common)] = common
    tiles = render.lz77(symbol_bytes(entry["pTileGraphics"]))
    vram[0x5800:0x5800 + len(tiles)] = tiles[:0x10000 - 0x5800]
    background = symbol_bytes(entry["pBackgroundGraphics"])
    size = background[2] << 8 | background[1]
    data = render.lz77(background)
    start = 0xFDE0 - size
    vram[start:start + len(data)] = data[:0x10000 - start]
    vram[0xFFE0:0x10000] = bytes(0x20)
    raw_palette = bytearray(0x200)
    # The DMA copies 3 rows minus a color from sCommonTilesPal + 1, a
    # one-row table that sDoorTransitionPal directly follows in ROM
    # (src/data/common_pals.c, mzm_us.map): rows 1-2 come from it.
    common_pal = symbol_bytes("sCommonTilesPal") + symbol_bytes("sDoorTransitionPal")
    raw_palette[2:96] = common_pal[2:96]
    palette = symbol_bytes(entry["pPalette"])
    raw_palette[96:512] = palette[32:448]
    return bytes(vram), bytes(raw_palette)


@lru_cache(maxsize=79)
def tileset_vram(index: int) -> tuple[bytes, list]:
    """BG VRAM and the 256 BG colors (None: transparent index 0 of a row)
    after RoomLoadTileset."""
    vram, raw_palette = tileset_video_memory(index)
    colors = [None] * 256
    for i in range(1, 256):
        colors[i] = bgr555(struct.unpack_from("<H", raw_palette, i * 2)[0])
    return vram, colors


@lru_cache(maxsize=79)
def tileset_metatiles(index: int) -> list[tuple[int, int, int, int]]:
    """gTilemap then gCommonTilemap: four tile entries per block value."""
    entry = tileset_entry(index)
    data = symbol_bytes(entry["pTilemap"])[2:]
    words = list(struct.unpack_from(f"<{len(data) // 2}H", data))
    words = (words + [0] * (TILESET_METATILES * 4))[:TILESET_METATILES * 4]
    common = symbol_bytes("sCommonTilemap")
    words += list(struct.unpack_from(f"<{COMMON_METATILES * 4}H", common))
    return [tuple(words[i * 4:i * 4 + 4]) for i in range(SHEET_BLOCKS)]


def draw_tile(rgba: bytearray, stride: int, x0: int, y0: int, word: int, vram: bytes,
              colors: list, char_base: int) -> None:
    """One 8x8 text tile entry (4bpp; index 0 transparent)."""
    tile = word & 0x3FF
    bank = word >> 12 & 15
    address = char_base + tile * 32
    if address + 32 > len(vram):
        return
    for y in range(8):
        sy = 7 - y if word & 0x800 else y
        for x in range(8):
            sx = 7 - x if word & 0x400 else x
            value = vram[address + sy * 4 + sx // 2] >> (4 * (sx & 1)) & 15
            color = colors[bank * 16 + value] if value else None
            if color is None:
                continue
            offset = ((y0 + y) * stride + x0 + x) * 4
            rgba[offset:offset + 4] = bytes((*color, 255))


def metatile_sheet(index: int) -> tuple[int, int, bytes]:
    """RGBA sheet of every block value of a tileset (common ones after the
    1024 tileset metatiles)."""
    vram, colors = tileset_vram(index)
    width = SHEET_COLUMNS * 16
    height = (SHEET_BLOCKS + SHEET_COLUMNS - 1) // SHEET_COLUMNS * 16
    rgba = bytearray(width * height * 4)
    for block, words in enumerate(tileset_metatiles(index)):
        bx, by = block % SHEET_COLUMNS * 16, block // SHEET_COLUMNS * 16
        for cell, word in enumerate(words):
            draw_tile(rgba, width, bx + cell % 2 * 8, by + cell // 2 * 8, word, vram, colors,
                      0x4000)
    return width, height, bytes(rgba)


def sheet_block(value: int) -> int:
    """The sheet index of a BG block value."""
    if value & TILEMAP_FLAG:
        common = value - TILEMAP_FLAG
        return TILESET_METATILES + common if common < COMMON_METATILES else 0
    return value if value < TILESET_METATILES else 0


def render_block_background(width: int, height: int, blocks: tuple[int, ...],
                            tileset: int, *, sheet: tuple | None = None) -> bytes:
    """Assemble one RLE 16x16-block layer from its validated tileset sheet."""
    if width <= 0 or height <= 0 or len(blocks) != width * height:
        raise ValueError("invalid Zero Mission block background")
    sheet_width, sheet_height, sheet_rgba = sheet or metatile_sheet(tileset)
    if (sheet_width != SHEET_COLUMNS * 16 or
            sheet_height * sheet_width * 4 != len(sheet_rgba) or
            sheet_height < ((SHEET_BLOCKS + SHEET_COLUMNS - 1) //
                            SHEET_COLUMNS) * 16):
        raise ValueError("invalid Zero Mission metatile sheet")
    output_width = width * 16
    rgba = bytearray(output_width * height * 16 * 4)
    for block_y in range(height):
        for block_x in range(width):
            block = sheet_block(blocks[block_x + block_y * width])
            source_x = block % SHEET_COLUMNS * 16
            source_y = block // SHEET_COLUMNS * 16
            for row in range(16):
                source = ((source_y + row) * sheet_width + source_x) * 4
                destination = ((block_y * 16 + row) * output_width +
                               block_x * 16) * 4
                rgba[destination:destination + 64] = \
                    sheet_rgba[source:source + 64]
    return bytes(rgba)


def text_background(symbol: str, tileset: int) -> tuple[int, int, int, bytes]:
    """An LZ77 text background (BG3, LZ77 BG0): size flag and RGBA image."""
    data = symbol_bytes(symbol)
    size = data[0]
    width, height = BG_SIZES.get(size, (512, 256))
    words = render.lz77(data[4:])
    vram, colors = tileset_vram(tileset)
    rgba = bytearray(width * height * 4)
    for screen_y in range(height // 256):
        for screen_x in range(width // 256):
            screen = screen_y * (width // 256) + screen_x
            for ty in range(32):
                for tx in range(32):
                    offset = (screen * 1024 + ty * 32 + tx) * 2
                    if offset + 2 > len(words):
                        continue
                    word = struct.unpack_from("<H", words, offset)[0]
                    draw_tile(rgba, width, screen_x * 256 + tx * 8, screen_y * 256 + ty * 8,
                              word, vram, colors, 0x8000)
    return size, width, height, bytes(rgba)


def bmp32(width: int, height: int, rgba: bytes) -> bytes:
    """A top-down 32-bit BMP with an alpha mask (BITMAPV4HEADER)."""
    pixels = bytearray(width * height * 4)
    for i in range(width * height):
        r, g, b, a = rgba[i * 4:i * 4 + 4]
        pixels[i * 4:i * 4 + 4] = bytes((b, g, r, a))
    header_size = 108
    offset = 14 + header_size
    file_header = struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
    info = struct.pack("<IiiHHIIiiII", header_size, width, -height, 1, 32, 3, len(pixels),
                       2835, 2835, 0, 0)
    masks = struct.pack("<IIII", 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    info += masks + b"BGRs" + bytes(36) + bytes(12)
    return file_header + info + bytes(pixels)


@lru_cache(maxsize=1)
def bg_properties(decomp: Path = DECOMP) -> dict[str, int]:
    """BgProperty values (include/constants/room.h): flags and OR-ed names."""
    header = (decomp / "include/constants/room.h").read_text(encoding="utf-8")
    body = re.search(r"MAKE_ENUM\(u8,\s*BgProperty\)[^{]*\{(.*?)\};", header, re.S)
    if body is None:
        raise ValueError("BgProperty enum not found")
    values, current = {}, -1
    for item in re.sub(r"//[^\n]*", "", body.group(1)).split(","):
        item = item.strip()
        if not item:
            continue
        name, _, expression = (part.strip() for part in item.partition("="))
        if expression:
            current = 0
            for term in expression.split("|"):
                term = term.strip()
                shift = re.fullmatch(r"(\d+)\s*<<\s*(\d+)", term)
                current |= (int(shift.group(1)) << int(shift.group(2)) if shift
                            else values[term] if term in values else int(term, 0))
        else:
            current += 1
        values[name] = current
    return values


def room_background_lines(fields: dict[str, str], layers: dict[str, tuple],
                          tileset: int) -> tuple[list[str], dict[str, tuple]]:
    """room.tsv lines of the backgrounds and the text BG images to write.

    ``L``: tileset, transparency, BG0/BG2/BG3 properties, BG3 scrolling and
    visual effect; ``G layer width height`` then ``R layer row values`` for
    the block layers (BG0 when RLE, BG1, BG2 when RLE); ``T layer size``
    for an LZ77 text layer written as ``bg<layer>.bmp``.
    """
    props = bg_properties()
    prop = {key: props.get(fields.get(f"{key}Prop", "BG_PROP_NONE"), 0)
            for key in ("bg0", "bg2", "bg3")}
    lines = [f"L\t{tileset}\t{int(fields.get('transparency', '0'), 0)}\t{prop['bg0']}\t"
             f"{prop['bg2']}\t{prop['bg3']}\t{int(fields.get('bg3Scrolling', '0'), 0)}\t"
             f"{fields.get('effect', 'EFFECT_NONE')}"]
    # ``E``: the room entry's effectY in blocks (255: no effect).
    effect_y = fields.get("effectY", "UCHAR_MAX")
    lines.append(f"E\t{255 if effect_y == 'UCHAR_MAX' else int(effect_y, 0)}")
    images = {}
    for layer in (0, 1, 2):
        if f"Bg{layer}" not in layers:
            continue
        width, height, blocks = layers[f"Bg{layer}"]
        lines.append(f"G\t{layer}\t{width}\t{height}")
        for row in range(height):
            values = blocks[row * width:(row + 1) * width]
            lines.append(f"R\t{layer}\t{row}\t" + ",".join(str(sheet_block(v)) for v in values))
    lz77 = props["BG_PROP_LZ77_COMPRESSED"]
    for layer, key in ((0, "bg0"), (3, "bg3")):
        if not prop[key] & lz77:
            continue
        size, width, height, rgba = text_background(fields[f"p{key.capitalize()}Data"], tileset)
        lines.append(f"T\t{layer}\t{size}\t{width}\t{height}")
        images[f"bg{layer}.bmp"] = (width, height, rgba)
    return lines, images
