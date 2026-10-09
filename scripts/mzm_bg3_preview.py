#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Experimental original MZM BG3 text-background preview from verified ROM data.

Output is a private, partial *diagnostic*, not a complete GBA frame. Missing
common palette rows and special effects remain unresolved rather than faked.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import struct
from scripts import mzm_room_render as native
from scripts.import_game_assets import OUTPUT, write_generated

SOURCE = native.ROOT / 'third_party/mzm/src/data/rooms_data.c'


def source_bg3_blob(symbol: str) -> bytes:
    if symbol in ('sSaveRoom_Bg3', 'sMapRoom_Bg3'):
        kind = 'save_room_bg3.rle' if symbol == 'sSaveRoom_Bg3' else 'map_room_bg3.rle'
        return native.read_raw('rooms/' + kind)
    if symbol == 'sBg3_Empty':
        return native.read_raw('rooms/bg3_empty.gfx.lz')
    m = re.fullmatch(r's([A-Za-z0-9]+)_Bg3_(\d+)', symbol)
    if m:
        area, number = m.groups()
        return native.read_raw(f'rooms/{area.lower()}/{area.lower()}_bg3_{number}.gfx')
    raise ValueError(f'BG3 resource not decoded: {symbol}')


def tileset_background_resource(tileset: int) -> bytes:
    source = SOURCE.read_text(encoding='utf-8')
    table = source.split('const struct TilesetEntry sTilesetEntries[79] = {', 1)[1]
    table = table.split('\n};', 1)[0]
    m = re.search(r'\[' + str(tileset) + r'\]\s*=\s*\{([^{}]+)\}', table)
    if not m:
        raise ValueError(f'no native tileset entry for {tileset}')
    gfx = re.search(r'\.pBackgroundGraphics\s*=\s*(s[A-Za-z0-9_]+)', m.group(1))
    if not gfx:
        raise ValueError(f'no background tileset reference for {tileset}')
    symbol = gfx.group(1)
    if symbol == 'sTileset_0_Bg_Gfx':
        return native.read_raw('rooms/tileset_0_background.gfx.lz')
    match = re.fullmatch(r'sTileset_(\d+)_Bg_Gfx', symbol)
    if not match:
        raise ValueError(f'unknown BG tileset resource {symbol}')
    return native.read_raw(f'tilesets/{match.group(1)}_bg.gfx.lz')


# PATCH_0128_BG3_DIAGNOSTICS: distinguish missing graphics from missing
# palette data. Do not silently pick a different charbase or invent pixels.
def analyze_references(map_data: bytes, tile_count: int, base: int) -> dict:
    if len(map_data) not in (2048, 4096):
        raise ValueError('unexpected BG3 tilemap size')
    counts = {'total_cells': len(map_data) // 2,
              'referenced_graphics_cells': 0,
              'missing_graphics_cells': 0,
              'common_palette_cells': 0,
              'tileset_palette_cells': 0}
    banks = set()
    for (word,) in struct.iter_unpack('<H', map_data):
        index = word & 1023
        bank = (word >> 12) & 15
        banks.add(bank)
        if base <= index < base + tile_count:
            counts['referenced_graphics_cells'] += 1
        else:
            counts['missing_graphics_cells'] += 1
        if bank < 3:
            counts['common_palette_cells'] += 1
        else:
            counts['tileset_palette_cells'] += 1
    counts['palette_banks'] = sorted(banks)
    counts['graphics_base_tile'] = base
    counts['graphics_tile_count'] = tile_count
    # Diagnostic only: alternatives are NOT automatically selected as correct.
    counts['candidate_base_coverage'] = {
        str(candidate): sum(candidate <= (word & 1023) < candidate + tile_count
                            for (word,) in struct.iter_unpack('<H', map_data))
        for candidate in (0, 192, 704, base)
    }
    return counts


def preview(bg3_blob: bytes, background_gfx: bytes, palette: bytes) -> tuple[bytes, dict]:
    # BG3 size/header layout from native RoomLoadBg3 data path.
    if len(bg3_blob) < 9:
        raise ValueError('BG3 header too short')
    map_data = native.lz77(bg3_blob[4:])
    if len(map_data) not in (2048, 4096):
        raise ValueError(f'unexpected BG3 tilemap size {len(map_data)}')
    gfx = native.lz77(background_gfx)
    if not gfx or len(gfx) % 32:
        raise ValueError('BG3 background graphics not 4bpp tile-aligned')
    # Original location candidate; do not treat it as hardware-verified.
    base_byte = 0xfde0 - len(gfx) - 0xc000
    if base_byte < 0 or base_byte % 32:
        raise ValueError('BG3 charbase offset not tile aligned')
    base = base_byte // 32
    report = analyze_references(map_data, len(gfx) // 32, base)
    tw = 32
    th = len(map_data) // (tw * 2)
    pixels = bytearray(tw * 8 * th * 8 * 3)
    visible = 0
    unresolved_tiles = 0
    missing_palette_pixels = 0
    transparent_pixels = 0
    missing_graphics_cells = 0
    for i, (word,) in enumerate(struct.iter_unpack('<H', map_data)):
        tile = (word & 1023) - base
        bank = word >> 12
        if tile < 0 or tile >= len(gfx) // 32:
            missing_graphics_cells += 1
            unresolved_tiles += 1
            continue
        x0 = (i % tw) * 8
        y0 = (i // tw) * 8
        for y in range(8):
            for x in range(8):
                tx = 7 - x if word & 0x400 else x
                ty = 7 - y if word & 0x800 else y
                index = native.tile_pixel(gfx, tile, tx, ty)
                if index == 0:
                    transparent_pixels += 1
                    continue
                color = native.palette_color(palette, bank, index)
                if color is None:
                    missing_palette_pixels += 1
                    continue
                dst = ((y0 + y) * tw * 8 + x0 + x) * 3
                pixels[dst:dst + 3] = bytes(color)
                visible += 1
    report.update({
        'status': 'EXPERIMENTAL_BG3_TEXT_MAP' if visible else 'BG3_UNRESOLVED',
        'visible_pixels': visible,
        'unresolved_tiles': unresolved_tiles + missing_palette_pixels,
        'missing_graphics_cells': missing_graphics_cells,
        'missing_palette_pixels': missing_palette_pixels,
        'transparent_pixels': transparent_pixels,
        'width': tw * 8, 'height': th * 8,
        'caveat': ('BG3 charbase/priority/scroll are not verified against GBA hardware; '
                   'common palette rows 0..2 are not yet decoded'),
    })
    if not visible:
        raise ValueError(
            'BG3 has no validated visible pixels: '
            f"graphics-missing cells={missing_graphics_cells}/{report['total_cells']}; "
            f"palette-missing pixels={missing_palette_pixels}; "
            f"transparent pixels={transparent_pixels}; "
            f"palette banks={report['palette_banks']}; "
            f"candidate base coverage={report['candidate_base_coverage']}. "
            'Missing common GBA graphics/palette resources must be recovered '
            'from the native loader; do not infer colors or select a base blindly.'
        )
    return native.bmp24(tw * 8, th * 8, pixels), report


# PATCH_0129_BG3_VRAM_PROBE: all counts come from native decoded map words.
# Coverage is a hypothesis score, never evidence that a VRAM base is correct.
def probe_vram_references(map_data: bytes, gfx_size: int) -> dict:
    """Summarize real BG3 tilemap words against possible GBA 4bpp VRAM bases.

    This intentionally never chooses a base, maps missing palettes, or renders.
    BG screen entries use 10-bit tile indices and four palette-bank bits.
    """
    from collections import Counter
    if len(map_data) not in (2048, 4096) or gfx_size <= 0 or gfx_size % 32:
        raise ValueError('invalid BG3 tilemap/4bpp graphics for VRAM probe')
    words = [word for (word,) in struct.iter_unpack('<H', map_data)]
    tile_ids = [word & 1023 for word in words]
    tiles = gfx_size // 32
    histogram = Counter(tile_ids)
    bank_histogram = Counter((word >> 12) & 15 for word in words)
    # Candidate base addresses are CHARBASE-relative: 0x0000, 0x1800,
    # and 0x5800. Preserve the loader-derived base separately.
    candidate_bases = (0, 192, 704)
    candidates = []
    for base in candidate_bases:
        covered = sum(count for tile_id, count in histogram.items()
                      if base <= tile_id < base + tiles)
        candidates.append({
            'tile_base': base,
            'offset_bytes': base * 32,
            'covered_cells': covered,
            'uncovered_cells': len(words) - covered,
            'coverage_pct': round(100 * covered / len(words), 2),
        })
    return {
        'status': 'DIAGNOSTIC_ONLY_UNVERIFIED_VRAM_LAYOUT',
        'total_cells': len(words),
        'tilemap_width_cells': 32,
        'tilemap_height_cells': len(words) // 32,
        'gfx_bytes': gfx_size,
        'gfx_tiles': tiles,
        'unique_tile_indices': len(histogram),
        'most_common_tile_indices': [
            {'tile': key, 'cells': value}
            for key, value in sorted(histogram.items(),
                                     key=lambda item: (-item[1], item[0]))[:16]
        ],
        'palette_bank_cells': {str(i): bank_histogram.get(i, 0)
                               for i in range(16)},
        'horizontal_flip_cells': sum(bool(word & 0x400) for word in words),
        'vertical_flip_cells': sum(bool(word & 0x800) for word in words),
        'candidate_bases': candidates,
        'note': ('Coverage alone does not establish the actual BG3 charbase, '
                 'VRAM graphics copy, or palette origin.'),
    }


def probe_native_room(area: str, room_number: int) -> dict:
    """Inspect privately imported native room resources; never write assets."""
    room = native.read_source_room(area, room_number)
    fields = room['fields']
    symbol = fields['pBg3Data']
    blob = source_bg3_blob(symbol)
    if len(blob) < 9:
        raise ValueError('BG3 header too short')
    map_data = native.lz77(blob[4:])
    compressed_gfx = tileset_background_resource(int(fields['tileset']))
    gfx = native.lz77(compressed_gfx)
    report = probe_vram_references(map_data, len(gfx))
    report.update({
        'area': area, 'room': room_number,
        'tileset': int(fields['tileset']),
        'bg3_symbol': symbol,
        'inferred_loader_base': (0xfde0 - len(gfx) - 0xc000) // 32,
        'inferred_loader_offset_aligned': (0xfde0 - len(gfx) - 0xc000) % 32 == 0,
    })
    return report


def render_room(area: str, room_number: int) -> dict:
    room = native.read_source_room(area, room_number)
    fields = room['fields']
    symbol = fields['pBg3Data']
    palette = native.tileset_blobs(int(fields['tileset']))[1]
    result, report = preview(source_bg3_blob(symbol),
                             tileset_background_resource(int(fields['tileset'])), palette)
    rel = f'rooms/metroid/previews/{area.lower()}_{room_number:03}_bg3.bmp'
    write_generated(rel, result)
    report['path'] = rel
    report['source_symbol'] = symbol
    return report


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--area', default='Brinstar')
    p.add_argument('--room', type=int, default=33)
    p.add_argument('--probe-vram', action='store_true',
                   help='print native BG3 reference/VRAM candidate evidence as JSON')
    args = p.parse_args()
    if args.probe_vram:
        import json
        try:
            report = probe_native_room(args.area, args.room)
        except (ValueError, OSError, IndexError, KeyError) as exc:
            p.error(str(exc))
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0
    try:
        info = render_room(args.area, args.room)
    except (ValueError, OSError, IndexError, KeyError) as e:
        p.error(str(e))
    print('Private experimental MZM BG3 preview:', OUTPUT/info['path'])
    print('Visible pixels:', info['visible_pixels'], 'unresolved tiles:', info['unresolved_tiles'])
    print(info['caveat'])
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
