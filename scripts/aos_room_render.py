#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render native Aria room backgrounds from a verified private USA ROM.

The decoder mirrors cvaos room loading: graphics and palette resources populate
virtual GBA memory, room metadata expands 4x4 tile blocks, and text backgrounds
are rendered with native flips and palette banks. Generated files remain under
the ignored assets/extracted directory.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.gba_tiles import bgr555
from scripts.import_aos_world import (
    AREA_COUNT,
    DEFAULT_ROM,
    EXPECTED_SHA1,
    GBA_ROM_BASE,
    decode_world,
)
from scripts.import_game_assets import OUTPUT, write_generated
from scripts.mzm_room_render import bmp24


MAX_LZ77_OUTPUT = 256 * 1024
MAX_ROOM_PIXELS = 4096 * 4096
VRAM_SIZE = 0x10000
PALETTE_SIZE = 0x200


def _offset(pointer: int, size: int, rom_size: int) -> int:
    offset = pointer - GBA_ROM_BASE
    if size < 0 or pointer < GBA_ROM_BASE or offset < 0 or offset + size > rom_size:
        raise ValueError(f"invalid Aria ROM pointer 0x{pointer:08x}")
    return offset


def lz77_at(rom: bytes, pointer: int, *, limit: int = MAX_LZ77_OUTPUT) -> bytes:
    """Decode a bounded BIOS 0x10 stream beginning at a ROM pointer."""
    position = _offset(pointer, 4, len(rom))
    if rom[position] != 0x10:
        raise ValueError(f"invalid LZ77 header at 0x{pointer:08x}")
    size = int.from_bytes(rom[position + 1:position + 4], "little")
    if not 0 < size <= limit:
        raise ValueError(f"invalid LZ77 size at 0x{pointer:08x}")
    position += 4
    output = bytearray()
    while len(output) < size:
        if position >= len(rom):
            raise ValueError(f"truncated LZ77 controls at 0x{pointer:08x}")
        flags = rom[position]
        position += 1
        for mask in (0x80, 0x40, 0x20, 0x10, 8, 4, 2, 1):
            if len(output) == size:
                break
            if flags & mask:
                if position + 2 > len(rom):
                    raise ValueError(f"truncated LZ77 token at 0x{pointer:08x}")
                token = (rom[position] << 8) | rom[position + 1]
                position += 2
                amount = (token >> 12) + 3
                distance = (token & 0xFFF) + 1
                if distance > len(output) or len(output) + amount > size:
                    raise ValueError(f"invalid LZ77 backreference at 0x{pointer:08x}")
                for _ in range(amount):
                    output.append(output[-distance])
            else:
                if position >= len(rom):
                    raise ValueError(f"truncated LZ77 literal at 0x{pointer:08x}")
                output.append(rom[position])
                position += 1
    return bytes(output)


def resource_payload(rom: bytes, pointer: int) -> tuple[bytes, dict]:
    """Resolve a cvaos graphics resource header to its uncompressed payload."""
    offset = _offset(pointer, 8, len(rom))
    encoding, color_mode, units, parameter = struct.unpack_from("<4B", rom, offset)
    if encoding == 0:
        payload = rom[offset + 4:]
    elif encoding == 1:
        compressed_pointer = struct.unpack_from("<I", rom, offset + 4)[0]
        expanded = lz77_at(rom, compressed_pointer)
        if len(expanded) < 4 or expanded[0] != 0:
            raise ValueError(f"invalid expanded resource at 0x{pointer:08x}")
        payload = expanded[4:]
    else:
        raise ValueError(f"unsupported resource encoding {encoding} at 0x{pointer:08x}")
    return payload, {
        "pointer": f"0x{pointer:08x}",
        "encoding": encoding,
        "color_mode": color_mode,
        "units": units,
        "parameter": parameter,
    }


def load_video_memory(rom: bytes, room: dict) -> tuple[bytes, bytes, list[dict]]:
    """Apply the room's native graphics and palette load lists."""
    vram = bytearray(VRAM_SIZE)
    palette = bytearray(PALETTE_SIZE)
    loads = []
    for entry in room["graphics_loads"]:
        pointer = int(entry["resource_pointer"], 16)
        payload, header = resource_payload(rom, pointer)
        destination_block, source_block, block_count = entry["parameters"]
        source = source_block * 0x800
        destination = destination_block * 0x800
        length = block_count * 0x800
        if source + length > len(payload) or destination + length > len(vram):
            raise ValueError(f"graphics load exceeds bounds at {entry['entry_pointer']}")
        vram[destination:destination + length] = payload[source:source + length]
        loads.append({
            "kind": "graphics",
            "entry_pointer": entry["entry_pointer"],
            "destination": destination,
            "source": source,
            "length": length,
            "resource": header,
        })

    for entry in room["palette_loads"]:
        pointer = int(entry["resource_pointer"], 16)
        offset = _offset(pointer, 4, len(rom))
        encoding, color_mode, row_count, parameter = struct.unpack_from(
            "<4B", rom, offset
        )
        if encoding != 0:
            raise ValueError(f"unsupported palette encoding at 0x{pointer:08x}")
        destination_row, source_row, count = entry["parameters"]
        if source_row + count > row_count or destination_row + count > 16:
            raise ValueError(f"palette load exceeds bounds at {entry['entry_pointer']}")
        source = offset + 4 + source_row * 0x20
        length = count * 0x20
        if source + length > len(rom):
            raise ValueError(f"palette resource is truncated at 0x{pointer:08x}")
        destination = destination_row * 0x20
        palette[destination:destination + length] = rom[source:source + length]
        loads.append({
            "kind": "palette",
            "entry_pointer": entry["entry_pointer"],
            "destination_row": destination_row,
            "source_row": source_row,
            "row_count": count,
            "resource": {
                "pointer": f"0x{pointer:08x}",
                "encoding": encoding,
                "color_mode": color_mode,
                "units": row_count,
                "parameter": parameter,
            },
        })
    return bytes(vram), bytes(palette), loads


def _metadata_payload(rom: bytes, pointer: int, compressed: bool) -> bytes:
    if compressed:
        expanded = lz77_at(rom, pointer)
        if len(expanded) < 4 or expanded[0] != 0:
            raise ValueError(f"invalid expanded room metadata at 0x{pointer:08x}")
        return expanded[4:]
    return rom[_offset(pointer, 1, len(rom)):]


def decode_background(rom: bytes, descriptor: dict) -> dict:
    """Expand one native text-background block map to 8x8 tile entries."""
    metadata_pointer_text = descriptor["metadata_pointer"]
    if metadata_pointer_text is None:
        return {"status": "ABSENT", "layer": descriptor["layer"]}
    metadata_pointer = int(metadata_pointer_text, 16)
    offset = _offset(metadata_pointer, 16, len(rom))
    width, height, flags, blocks_pointer, collision_pointer, map_pointer = (
        struct.unpack_from("<BBHIII", rom, offset)
    )
    if not width or not height or width > 16 or height > 16:
        raise ValueError(f"invalid background dimensions at {metadata_pointer_text}")
    if flags & 1:
        return {
            "status": "UNSUPPORTED_AFFINE",
            "layer": descriptor["layer"],
            "metadata_pointer": metadata_pointer_text,
            "width_screens": width,
            "height_screens": height,
            "flags": flags,
        }

    block_map_count = width * height * 64
    map_offset = _offset(map_pointer, block_map_count * 2, len(rom))
    block_map = struct.unpack_from(f"<{block_map_count}H", rom, map_offset)
    maximum_block = max((value & 0x3FFF for value in block_map), default=0)
    compressed = bool(flags & 2)
    block_data = _metadata_payload(rom, blocks_pointer, compressed)
    required = maximum_block * 16 * 2
    if required > len(block_data):
        raise ValueError(f"background block table is truncated at 0x{blocks_pointer:08x}")
    collision_data = None
    if collision_pointer:
        collision_data = _metadata_payload(rom, collision_pointer, compressed)
        if maximum_block * 16 > len(collision_data):
            raise ValueError(
                f"background collision table is truncated at 0x{collision_pointer:08x}"
            )

    width_tiles = width * 32
    height_tiles = height * 32
    if width_tiles * height_tiles * 64 > MAX_ROOM_PIXELS:
        raise ValueError("Aria background exceeds pixel safety cap")
    tiles = [0] * (width_tiles * height_tiles)
    collision = [0] * len(tiles) if collision_data is not None else None
    for tile_y in range(height_tiles):
        block_y, local_y = divmod(tile_y, 4)
        for tile_x in range(width_tiles):
            block_x, local_x = divmod(tile_x, 4)
            map_value = block_map[block_x + block_y * width * 8]
            block = map_value & 0x3FFF
            if block == 0:
                continue
            if map_value & 0x4000:
                local_x = 3 - local_x
            if map_value & 0x8000:
                local_y = 3 - local_y
            cell = (block - 1) * 16 + local_x + local_y * 4
            word = struct.unpack_from("<H", block_data, cell * 2)[0]
            word ^= ((map_value >> 12) & 0xC) << 8
            destination = tile_x + tile_y * width_tiles
            tiles[destination] = word
            if collision is not None:
                collision[destination] = collision_data[cell]

    return {
        "status": "DECODED_TEXT_BACKGROUND",
        "layer": descriptor["layer"],
        "metadata_pointer": metadata_pointer_text,
        "width_screens": width,
        "height_screens": height,
        "width_tiles": width_tiles,
        "height_tiles": height_tiles,
        "flags": flags,
        "control": descriptor["control"],
        "depth_key": descriptor["field_0"],
        "tiles": tiles,
        "collision": collision,
    }


def render_background(background: dict, vram: bytes, palette: bytes) -> tuple[bytes, int]:
    """Render a decoded 4 bpp GBA text background to RGBA."""
    if background["status"] != "DECODED_TEXT_BACKGROUND":
        raise ValueError("background is not a decoded text layer")
    control = background["control"]
    if control & 0x80:
        raise ValueError("8 bpp text backgrounds are not supported")
    width = background["width_tiles"] * 8
    height = background["height_tiles"] * 8
    rgba = bytearray(width * height * 4)
    unresolved = 0
    character_base = ((control >> 2) & 3) * 0x4000
    for tile_y in range(background["height_tiles"]):
        for tile_x in range(background["width_tiles"]):
            word = background["tiles"][tile_x + tile_y * background["width_tiles"]]
            tile = word & 0x3FF
            tile_offset = character_base + tile * 32
            if tile_offset + 32 > len(vram):
                unresolved += 1
                continue
            palette_bank = (word >> 12) & 0xF
            for y in range(8):
                source_y = 7 - y if word & 0x800 else y
                for x in range(8):
                    source_x = 7 - x if word & 0x400 else x
                    packed = vram[tile_offset + source_y * 4 + source_x // 2]
                    color_index = (packed >> (4 * (source_x & 1))) & 0xF
                    if color_index == 0:
                        continue
                    palette_index = palette_bank * 16 + color_index
                    color_offset = palette_index * 2
                    if color_offset + 2 > len(palette):
                        unresolved += 1
                        continue
                    color = bgr555(struct.unpack_from("<H", palette, color_offset)[0])
                    destination = (
                        ((tile_y * 8 + y) * width + tile_x * 8 + x) * 4
                    )
                    rgba[destination:destination + 4] = bytes((*color, 255))
    return bytes(rgba), unresolved


def composite_backgrounds(rendered: list[tuple[dict, bytes]]) -> tuple[int, int, bytes]:
    """Build a static-origin diagnostic composite using native depth keys."""
    # Some original rooms have no text BG1. Display the first available
    # native layer rather than rejecting an otherwise decodable room.
    if not rendered:
        raise ValueError("room has no supported text background layers")
    primary = next((item for item in rendered if item[0]["layer"] == 1),
                   rendered[0])
    width = primary[0]["width_tiles"] * 8
    height = primary[0]["height_tiles"] * 8
    rgb = bytearray(width * height * 3)
    for background, rgba in sorted(rendered, key=lambda item: item[0]["depth_key"],
                                   reverse=True):
        source_width = background["width_tiles"] * 8
        source_height = background["height_tiles"] * 8
        for y in range(height):
            source_row = (y % source_height) * source_width
            destination_row = y * width
            for x in range(width):
                source = (source_row + (x % source_width)) * 4
                if rgba[source + 3] == 0:
                    continue
                destination = (destination_row + x) * 3
                rgb[destination:destination + 3] = rgba[source:source + 3]
    return width, height, bytes(rgb)


def collision_preview(background: dict) -> bytes:
    """Render raw collision bytes with an explicitly diagnostic color key."""
    collision = background.get("collision")
    if collision is None:
        raise ValueError("BG1 has no collision payload")
    width = background["width_tiles"] * 8
    height = background["height_tiles"] * 8
    rgb = bytearray(width * height * 3)
    for tile_y in range(background["height_tiles"]):
        for tile_x in range(background["width_tiles"]):
            value = collision[tile_x + tile_y * background["width_tiles"]]
            if value == 0:
                color = (8, 8, 12)
            elif value == 0xFF:
                color = (240, 240, 240)
            elif value & 0xC0:
                color = (245, 160, 48)
            elif value & 2:
                color = (220, 55, 75)
            else:
                color = (70, 145, 235)
            for y in range(8):
                for x in range(8):
                    if x == 0 or y == 0:
                        pixel = tuple(channel // 2 for channel in color)
                    else:
                        pixel = color
                    destination = ((tile_y * 8 + y) * width + tile_x * 8 + x) * 3
                    rgb[destination:destination + 3] = bytes(pixel)
    return bmp24(width, height, rgb)


def render_room(rom: bytes, area: int, room_number: int,
                *, verify_hash: bool = True) -> dict:
    if not 0 <= area < AREA_COUNT or room_number < 0:
        raise ValueError("invalid Aria room selection")
    if verify_hash and hashlib.sha1(rom, usedforsecurity=False).hexdigest() != EXPECTED_SHA1:
        raise ValueError("expected unmodified Aria of Sorrow USA ROM")
    world = decode_world(rom, verify_hash=verify_hash)
    try:
        room = next(
            entry for entry in world["rooms"]
            if entry["engine_area"] == area and entry["room"] == room_number
        )
    except StopIteration as exc:
        raise ValueError(f"unknown Aria room {area}:{room_number}") from exc

    vram, palette, loads = load_video_memory(rom, room)
    backgrounds = [decode_background(rom, item) for item in room["backgrounds"]]
    rendered = []
    layer_records = []
    prefix = f"rooms/aria/previews/area_{area:02}_room_{room_number:03}"
    for background in backgrounds:
        record = {
            key: value for key, value in background.items()
            if key not in ("tiles", "collision")
        }
        if background["status"] == "DECODED_TEXT_BACKGROUND":
            rgba, unresolved = render_background(background, vram, palette)
            width = background["width_tiles"] * 8
            height = background["height_tiles"] * 8
            rgb = bytearray(width * height * 3)
            for pixel in range(width * height):
                rgb[pixel * 3:pixel * 3 + 3] = rgba[pixel * 4:pixel * 4 + 3]
            path = f"{prefix}_bg{background['layer']}.bmp"
            write_generated(path, bmp24(width, height, rgb))
            record.update({"path": path, "unresolved_tiles_or_pixels": unresolved})
            rendered.append((background, rgba))
        layer_records.append(record)

    width, height, composite = composite_backgrounds(rendered)
    composite_path = f"{prefix}_composite.bmp"
    write_generated(composite_path, bmp24(width, height, composite))
    bg1 = next(item for item in backgrounds if item["layer"] == 1)
    collision_path = None
    if bg1.get("collision") is not None:
        collision_path = f"{prefix}_collision.bmp"
        write_generated(collision_path, collision_preview(bg1))

    result = {
        "format": "MV_AOS_RENDER_1",
        "status": "NATIVE_TEXT_BACKGROUND_RENDER",
        "source_revision": world["source_revision"],
        "engine_area": area,
        "area": room["area"],
        "room": room_number,
        "room_pointer": room["source_pointer"],
        "map_x": room["map_x"],
        "map_y": room["map_y"],
        "width": width,
        "height": height,
        "composite_path": composite_path,
        "collision_path": collision_path,
        "loads": loads,
        "layers": layer_records,
        "entity_count": len(room["entities"]),
        "transition_count": len(room["transitions"]),
        "limitations": [
            "static-origin composite does not simulate parallax camera movement",
            "affine/8 bpp backgrounds, blending, animation and entities are omitted",
            "collision preview colors are diagnostic and are not original game art",
        ],
    }
    metadata_path = f"{prefix}.json"
    write_generated(metadata_path, (json.dumps(result, indent=2) + "\n").encode())
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    parser.add_argument("--area", type=int, default=0)
    parser.add_argument("--room", type=int, default=10)
    args = parser.parse_args()
    try:
        result = render_room(args.rom.read_bytes(), args.area, args.room)
    except (OSError, ValueError, KeyError, IndexError, struct.error) as exc:
        parser.error(str(exc))
    print(
        f"Rendered Aria room {result['engine_area']}:{result['room']} "
        f"({result['width']}x{result['height']}) -> "
        f"{OUTPUT / result['composite_path']}"
    )
    if result["collision_path"]:
        print(f"Collision diagnostic -> {OUTPUT / result['collision_path']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
