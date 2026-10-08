#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a verified Soma idle frame from a personal Aria of Sorrow ROM."""
import argparse
import hashlib
from pathlib import Path
import struct

try:
    from scripts.gba_oam import to_bmp
    from scripts.gba_tiles import bgr555
except ModuleNotFoundError:
    from gba_oam import to_bmp
    from gba_tiles import bgr555


EXPECTED_SHA1 = "abd71fe01ebb201bcc133074db1dd8c5253776c7"
SOMA_GRAPHICS_DESCRIPTOR = 0x080E11D4
SOMA_PALETTE_DESCRIPTOR = 0x082097D4
SOMA_ANIMATION_DESCRIPTOR = 0x080E11C4
SOMA_IDLE_ANIMATION = 0
CELL_TILES = 8
SHEET_TILES = 16
TILE_BYTES = 32


def rom_offset(pointer, rom):
    if not 0x08000000 <= pointer < 0x08000000 + len(rom):
        raise ValueError(f"ROM pointer outside image: 0x{pointer:08X}")
    return pointer - 0x08000000


def _rom_slice(rom, pointer, size, label):
    offset = rom_offset(pointer, rom)
    if size < 0 or size > len(rom) - offset:
        raise ValueError(f"truncated {label}")
    return rom[offset:offset + size]


def parse_animation(rom, animation_index,
                    descriptor_pointer=SOMA_ANIMATION_DESCRIPTOR):
    descriptor = _rom_slice(rom, descriptor_pointer, 16,
                            "Soma animation descriptor")
    _, animation_count, _, _, table_pointer = struct.unpack("<HHIII", descriptor)
    if not 0 <= animation_index < animation_count:
        raise ValueError(f"Soma animation index must be 0..{animation_count - 1}")
    animation_pointer = struct.unpack(
        "<I", _rom_slice(rom, table_pointer + animation_index * 4, 4,
                         "Soma animation pointer"))[0]
    frame_count, encoding = struct.unpack(
        "<HH", _rom_slice(rom, animation_pointer, 4, "Soma animation header"))
    if encoding != 1:
        raise ValueError(f"unsupported Soma animation encoding: {encoding}")
    raw_frames = _rom_slice(rom, animation_pointer + 4, frame_count * 4,
                            "Soma animation frames")
    frames = []
    for index in range(frame_count):
        frame_id, duration, reserved = struct.unpack_from("<BBH", raw_frames,
                                                          index * 4)
        if reserved:
            raise ValueError("unexpected data in Soma animation frame")
        frames.append({"frame_id": frame_id, "duration": duration})
    return {
        "animation_pointer": animation_pointer,
        "encoding": encoding,
        "frames": frames,
    }


def load_palette(rom, descriptor_pointer=SOMA_PALETTE_DESCRIPTOR):
    header = _rom_slice(rom, descriptor_pointer, 4, "Soma palette descriptor")
    encoding, colors_per_bank, bank_count, destination = header
    if (encoding, colors_per_bank, destination) != (0, 4, 0) or bank_count < 1:
        raise ValueError("unexpected Soma palette descriptor")
    return _rom_slice(rom, descriptor_pointer + 4, 32, "Soma OBJ palette")


def extract_cell_tiles(rom, frame_id,
                       descriptor_pointer=SOMA_GRAPHICS_DESCRIPTOR):
    descriptor = _rom_slice(rom, descriptor_pointer, 4,
                            "Soma graphics descriptor")
    encoding, sheet_count, slot_width, slot_height = descriptor
    if (encoding, slot_width, slot_height) != (2, 1, 1):
        raise ValueError("unexpected Soma graphics descriptor")
    sheet_index = frame_id // 4
    if sheet_index >= sheet_count:
        raise ValueError(f"Soma frame {frame_id} has no graphics sheet")
    sheet_pointer = struct.unpack(
        "<I", _rom_slice(rom, descriptor_pointer + 4 + sheet_index * 4, 4,
                         "Soma graphics sheet pointer"))[0]
    sheet_header = _rom_slice(rom, sheet_pointer, 4, "Soma graphics sheet")
    compression, bits_per_pixel, width_tiles, height_tiles = sheet_header
    if (compression, bits_per_pixel, width_tiles, height_tiles) != (0, 4, 16, 16):
        raise ValueError("unexpected Soma graphics sheet format")
    sheet = _rom_slice(rom, sheet_pointer + 4,
                       SHEET_TILES * SHEET_TILES * TILE_BYTES,
                       "Soma graphics sheet data")
    quadrant_x = (frame_id & 1) * CELL_TILES
    quadrant_y = ((frame_id >> 1) & 1) * CELL_TILES
    cell = bytearray(CELL_TILES * CELL_TILES * TILE_BYTES)
    for tile_y in range(CELL_TILES):
        source = ((quadrant_y + tile_y) * SHEET_TILES + quadrant_x) * TILE_BYTES
        target = tile_y * CELL_TILES * TILE_BYTES
        cell[target:target + CELL_TILES * TILE_BYTES] = \
            sheet[source:source + CELL_TILES * TILE_BYTES]
    return bytes(cell), {
        "sheet_index": sheet_index,
        "sheet_pointer": sheet_pointer,
        "quadrant": (quadrant_x // CELL_TILES, quadrant_y // CELL_TILES),
    }


def decode_cell(tile_data, palette):
    if len(tile_data) != CELL_TILES * CELL_TILES * TILE_BYTES:
        raise ValueError("Soma cell must contain exactly 64 4bpp tiles")
    if len(palette) != 32:
        raise ValueError("Soma palette must contain exactly 16 colors")
    colors = [bgr555(struct.unpack_from("<H", palette, index * 2)[0])
              for index in range(16)]
    width = height = CELL_TILES * 8
    rgba = bytearray(width * height * 4)
    for tile in range(CELL_TILES * CELL_TILES):
        tile_x = tile % CELL_TILES
        tile_y = tile // CELL_TILES
        for y in range(8):
            for x in range(8):
                packed = tile_data[tile * TILE_BYTES + y * 4 + x // 2]
                color_index = (packed >> (4 * (x & 1))) & 15
                pixel = ((tile_y * 8 + y) * width + tile_x * 8 + x) * 4
                rgba[pixel:pixel + 4] = bytes((*colors[color_index],
                                               0 if color_index == 0 else 255))
    return bytes(rgba), width, height


def make_soma_idle_frame(rom, frame_index=0):
    animation = parse_animation(rom, SOMA_IDLE_ANIMATION)
    if not 0 <= frame_index < len(animation["frames"]):
        raise ValueError(
            f"Soma idle frame index must be 0..{len(animation['frames']) - 1}")
    frame = animation["frames"][frame_index]
    tile_data, graphics = extract_cell_tiles(rom, frame["frame_id"])
    palette = load_palette(rom)
    rgba, width, height = decode_cell(tile_data, palette)
    metadata = {
        "animation_pointer": animation["animation_pointer"],
        "frame_index": frame_index,
        "frame_id": frame["frame_id"],
        "duration": frame["duration"],
        "width": width,
        "height": height,
        **graphics,
    }
    return to_bmp(rgba, width, height), metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--frame", type=int, default=0)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    allowed = (root / "assets" / "extracted").resolve()
    output = args.output.resolve()
    if output.suffix.lower() != ".bmp" or allowed not in output.parents:
        parser.error(f"output must be a BMP below {allowed}")
    if any(path.is_symlink() for path in (output, *output.parents)
           if path == allowed or allowed in path.parents):
        parser.error("symlink output path refused")
    try:
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            parser.error("expected unmodified Aria of Sorrow USA ROM")
        bitmap, metadata = make_soma_idle_frame(rom, args.frame)
        output.parent.mkdir(parents=True, exist_ok=True)
        if not output.exists() or output.read_bytes() != bitmap:
            output.write_bytes(bitmap)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Wrote verified Soma idle frame {args.frame}: {output}")
    print(f"Size={metadata['width']}x{metadata['height']}; "
          f"frame id={metadata['frame_id']}; duration={metadata['duration']}; "
          f"sheet={metadata['sheet_index']}; quadrant={metadata['quadrant']}")


if __name__ == "__main__":
    main()
