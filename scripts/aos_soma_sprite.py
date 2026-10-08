#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build verified Soma animations from a personal Aria of Sorrow ROM."""
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
KNIFE_GRAPHICS_DESCRIPTOR = 0x081AC54C
KNIFE_PALETTE_DESCRIPTOR = 0x082098B8
KNIFE_ANIMATION_DESCRIPTOR = 0x0822B6C0
# Raw weapon OAM coordinates are relative to Soma's origin. Soma's 64x64 cell
# begins 32 pixels left and 47 pixels above that origin in this attack.
KNIFE_CELL_ANCHOR = (32, 47)
SOMA_ANIMATIONS = {
    "idle": {"segments": ((0, None),), "frame_count": 4},
    "run": {"segments": ((1, None),), "frame_count": 17},
    # A normal jump observed on level ground: takeoff, airborne, then landing.
    "jump": {"segments": ((50, None), (12, 2), (13, None)), "frame_count": 12},
    # The first body segment and knife animation both last exactly 27 updates.
    # Recovery continues without the weapon for another 21 updates.
    "attack": {
        "segments": ((4, None), (5, None)),
        "weapon_animation": 0,
        "weapon_active_segment_count": 1,
        "frame_count": 11,
    },
}
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


def load_palette(rom, descriptor_pointer=SOMA_PALETTE_DESCRIPTOR,
                 palette_index=0):
    header = _rom_slice(rom, descriptor_pointer, 4, "Soma palette descriptor")
    encoding, colors_per_bank, bank_count, destination = header
    if ((encoding, colors_per_bank, destination) != (0, 4, 0) or
            not 0 <= palette_index < bank_count):
        raise ValueError("unexpected Soma palette descriptor")
    return _rom_slice(rom, descriptor_pointer + 4 + palette_index * 32, 32,
                      "Soma OBJ palette")


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


def decode_tiles(tile_data, width_tiles, height_tiles, palette):
    if (width_tiles < 1 or height_tiles < 1 or
            len(tile_data) != width_tiles * height_tiles * TILE_BYTES):
        raise ValueError("tile data size does not match its dimensions")
    if len(palette) != 32:
        raise ValueError("Soma palette must contain exactly 16 colors")
    colors = [bgr555(struct.unpack_from("<H", palette, index * 2)[0])
              for index in range(16)]
    width = width_tiles * 8
    height = height_tiles * 8
    rgba = bytearray(width * height * 4)
    for tile in range(width_tiles * height_tiles):
        tile_x = tile % width_tiles
        tile_y = tile // width_tiles
        for y in range(8):
            for x in range(8):
                packed = tile_data[tile * TILE_BYTES + y * 4 + x // 2]
                color_index = (packed >> (4 * (x & 1))) & 15
                pixel = ((tile_y * 8 + y) * width + tile_x * 8 + x) * 4
                rgba[pixel:pixel + 4] = bytes((*colors[color_index],
                                               0 if color_index == 0 else 255))
    return bytes(rgba), width, height


def decode_cell(tile_data, palette):
    if len(tile_data) != CELL_TILES * CELL_TILES * TILE_BYTES:
        raise ValueError("Soma cell must contain exactly 64 4bpp tiles")
    return decode_tiles(tile_data, CELL_TILES, CELL_TILES, palette)


def extract_knife_frame(rom, frame_id,
                        graphics_pointer=KNIFE_GRAPHICS_DESCRIPTOR,
                        animation_pointer=KNIFE_ANIMATION_DESCRIPTOR):
    descriptor = _rom_slice(rom, animation_pointer, 16,
                            "knife animation descriptor")
    record_count, _, records_pointer, _, _ = struct.unpack("<HHIII", descriptor)
    if not 0 <= frame_id < record_count:
        raise ValueError(f"knife frame {frame_id} has no OAM record")
    record = _rom_slice(rom, records_pointer + frame_id * 16, 16,
                        "knife OAM frame record")
    if record[:4] != bytes(4) or record[5] != 1 or record[6:8] != bytes(2):
        raise ValueError("unsupported knife OAM frame composition")
    effect_pointer = struct.unpack_from("<I", record, 8)[0]
    component_pointer = struct.unpack_from("<I", record, 12)[0]
    component = _rom_slice(rom, component_pointer, 12, "knife OAM component")
    x, y, reserved, source_x, source_y, width, height, flags = struct.unpack(
        "<bbHBBBBI", component)
    if (reserved or source_x % 8 or source_y % 8 or width % 8 or height % 8 or
            width < 8 or height < 8):
        raise ValueError("unsupported knife OAM component")

    header = _rom_slice(rom, graphics_pointer, 4, "knife graphics descriptor")
    encoding, bits_per_pixel, sheet_width, sheet_height = header
    if (encoding, bits_per_pixel, sheet_width, sheet_height) != (0, 4, 16, 4):
        raise ValueError("unexpected knife graphics descriptor")
    width_tiles = width // 8
    height_tiles = height // 8
    source_tile_x = source_x // 8
    source_tile_y = source_y // 8
    if (source_tile_x + width_tiles > sheet_width or
            source_tile_y + height_tiles > sheet_height):
        raise ValueError("knife OAM component exceeds its graphics sheet")
    sheet = _rom_slice(rom, graphics_pointer + 4,
                       sheet_width * sheet_height * TILE_BYTES,
                       "knife graphics sheet")
    tiles = bytearray(width_tiles * height_tiles * TILE_BYTES)
    for tile_y in range(height_tiles):
        source = ((source_tile_y + tile_y) * sheet_width + source_tile_x) * TILE_BYTES
        target = tile_y * width_tiles * TILE_BYTES
        tiles[target:target + width_tiles * TILE_BYTES] = \
            sheet[source:source + width_tiles * TILE_BYTES]
    return bytes(tiles), {
        "frame_id": frame_id,
        "record_pointer": records_pointer + frame_id * 16,
        "effect_pointer": effect_pointer,
        "component_pointer": component_pointer,
        "source_pointer": graphics_pointer + 4 +
                          (source_tile_y * sheet_width + source_tile_x) * TILE_BYTES,
        "position": (KNIFE_CELL_ANCHOR[0] + x, KNIFE_CELL_ANCHOR[1] + y),
        "width": width,
        "height": height,
        "flags": flags,
    }


def opaque_bounds(rgba, width, height):
    points = [(x, y) for y in range(height) for x in range(width)
              if rgba[(y * width + x) * 4 + 3]]
    if not points:
        raise ValueError("Soma cell is fully transparent")
    return (min(x for x, _ in points), min(y for _, y in points),
            max(x for x, _ in points) + 1, max(y for _, y in points) + 1)


def crop_rgba(rgba, width, height, bounds):
    left, top, right, bottom = bounds
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        raise ValueError("invalid Soma cell crop bounds")
    cropped_width = right - left
    cropped_height = bottom - top
    cropped = bytearray(cropped_width * cropped_height * 4)
    for y in range(top, bottom):
        source = (y * width + left) * 4
        target = (y - top) * cropped_width * 4
        cropped[target:target + cropped_width * 4] = \
            rgba[source:source + cropped_width * 4]
    return bytes(cropped), cropped_width, cropped_height


def _segment_frames(rom, segments):
    frames = []
    for animation_index, limit in segments:
        animation = parse_animation(rom, animation_index)
        selected = animation["frames"] if limit is None else animation["frames"][:limit]
        for frame in selected:
            frames.append({
                **frame,
                "animation_index": animation_index,
                "animation_pointer": animation["animation_pointer"],
            })
    return frames


def merge_attack_frames(body_frames, weapon_frames, active_body_frame_count):
    if not 0 < active_body_frame_count <= len(body_frames):
        raise ValueError("invalid weapon-active body frame count")
    active_duration = sum(frame["duration"]
                          for frame in body_frames[:active_body_frame_count])
    weapon_duration = sum(frame["duration"] for frame in weapon_frames)
    if active_duration != weapon_duration:
        raise ValueError("knife and offensive body timelines have different durations")
    recovery_duration = sum(frame["duration"]
                            for frame in body_frames[active_body_frame_count:])
    weapon_timeline = [dict(frame) for frame in weapon_frames]
    if recovery_duration:
        weapon_timeline.append({"frame_id": None, "duration": recovery_duration})

    merged = []
    body_index = weapon_index = 0
    body_remaining = body_frames[0]["duration"]
    weapon_remaining = weapon_timeline[0]["duration"]
    while body_index < len(body_frames) and weapon_index < len(weapon_timeline):
        duration = min(body_remaining, weapon_remaining)
        merged.append({
            **body_frames[body_index],
            "duration": duration,
            "weapon_frame_id": weapon_timeline[weapon_index]["frame_id"],
        })
        body_remaining -= duration
        weapon_remaining -= duration
        if body_remaining == 0:
            body_index += 1
            if body_index < len(body_frames):
                body_remaining = body_frames[body_index]["duration"]
        if weapon_remaining == 0:
            weapon_index += 1
            if weapon_index < len(weapon_timeline):
                weapon_remaining = weapon_timeline[weapon_index]["duration"]
    if body_index != len(body_frames) or weapon_index != len(weapon_timeline):
        raise ValueError("body and knife timelines do not cover the same duration")
    return merged


def animation_frames(rom, animation_name):
    if animation_name not in SOMA_ANIMATIONS:
        raise ValueError(f"unsupported Soma animation: {animation_name}")
    definition = SOMA_ANIMATIONS[animation_name]
    frames = _segment_frames(rom, definition["segments"])
    if "weapon_animation" in definition:
        active_segments = definition["weapon_active_segment_count"]
        active_frame_count = len(_segment_frames(
            rom, definition["segments"][:active_segments]))
        weapon = parse_animation(rom, definition["weapon_animation"],
                                 KNIFE_ANIMATION_DESCRIPTOR)
        frames = merge_attack_frames(frames, weapon["frames"], active_frame_count)
    if len(frames) != definition["frame_count"]:
        raise ValueError(
            f"unexpected {animation_name} frame count: {len(frames)}")
    return frames


def _frame_layers(rom, frame):
    body_tiles, body_graphics = extract_cell_tiles(rom, frame["frame_id"])
    body = decode_cell(body_tiles, load_palette(rom))
    layers = [(body[0], body[1], body[2], 0, 0)]
    weapon_metadata = None
    if frame.get("weapon_frame_id") is not None:
        weapon_tiles, weapon_metadata = extract_knife_frame(
            rom, frame["weapon_frame_id"])
        weapon = decode_tiles(weapon_tiles,
                              weapon_metadata["width"] // 8,
                              weapon_metadata["height"] // 8,
                              load_palette(rom, KNIFE_PALETTE_DESCRIPTOR))
        x, y = weapon_metadata["position"]
        layers.append((weapon[0], weapon[1], weapon[2], x, y))
    return layers, body_graphics, weapon_metadata


def animation_canvas_bounds(rom, animation_name):
    frames = animation_frames(rom, animation_name)
    bounds = []
    for frame in frames:
        layers, _, _ = _frame_layers(rom, frame)
        for rgba, width, height, x, y in layers:
            left, top, right, bottom = opaque_bounds(rgba, width, height)
            bounds.append((left + x, top + y, right + x, bottom + y))
    return (min(item[0] for item in bounds), min(item[1] for item in bounds),
            max(item[2] for item in bounds), max(item[3] for item in bounds))


def compose_rgba_layers(layers, bounds):
    left, top, right, bottom = bounds
    if left >= right or top >= bottom:
        raise ValueError("invalid composite canvas bounds")
    width = right - left
    height = bottom - top
    composite = bytearray(width * height * 4)
    for rgba, layer_width, layer_height, layer_x, layer_y in layers:
        for y in range(layer_height):
            target_y = layer_y + y - top
            if not 0 <= target_y < height:
                continue
            for x in range(layer_width):
                target_x = layer_x + x - left
                if not 0 <= target_x < width:
                    continue
                source = (y * layer_width + x) * 4
                if not rgba[source + 3]:
                    continue
                target = (target_y * width + target_x) * 4
                composite[target:target + 4] = rgba[source:source + 4]
    return bytes(composite), width, height


def make_soma_animation_frame(rom, animation_name, frame_index=0,
                              canvas_bounds=None):
    frames = animation_frames(rom, animation_name)
    if not 0 <= frame_index < len(frames):
        raise ValueError(
            f"Soma {animation_name} frame index must be "
            f"0..{len(frames) - 1}")
    frame = frames[frame_index]
    layers, graphics, weapon = _frame_layers(rom, frame)
    canvas_bounds = canvas_bounds or animation_canvas_bounds(rom, animation_name)
    rgba, width, height = compose_rgba_layers(layers, canvas_bounds)
    metadata = {
        "animation_index": frame["animation_index"],
        "animation_pointer": frame["animation_pointer"],
        "frame_index": frame_index,
        "frame_id": frame["frame_id"],
        "duration": frame["duration"],
        "width": width,
        "height": height,
        "pixel_bounds": canvas_bounds,
        "weapon": weapon,
        **graphics,
    }
    return to_bmp(rgba, width, height), metadata


def make_soma_idle_frame(rom, frame_index=0, canvas_bounds=None):
    return make_soma_animation_frame(rom, "idle", frame_index, canvas_bounds)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--animation", choices=sorted(SOMA_ANIMATIONS),
                        default="idle")
    parser.add_argument("--frame", type=int, default=0)
    output_group = parser.add_mutually_exclusive_group(required=True)
    output_group.add_argument("--output", type=Path,
                              help="write one BMP selected by --frame")
    output_group.add_argument("--output-dir", type=Path,
                              help="write every frame of the selected animation")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    allowed = (root / "assets" / "extracted").resolve()
    if args.output:
        outputs = [(args.frame, args.output.resolve())]
    else:
        output_dir = args.output_dir.resolve()
        frame_count = SOMA_ANIMATIONS[args.animation]["frame_count"]
        outputs = [(index, output_dir / f"{args.animation}_{index}.bmp")
                   for index in range(frame_count)]
    for _, output in outputs:
        if output.suffix.lower() != ".bmp" or allowed not in output.parents:
            parser.error(f"output must be a BMP below {allowed}")
        if any(path.is_symlink() for path in (output, *output.parents)
               if path == allowed or allowed in path.parents):
            parser.error("symlink output path refused")
    try:
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            parser.error("expected unmodified Aria of Sorrow USA ROM")
        canvas_bounds = animation_canvas_bounds(rom, args.animation)
        results = []
        for index, output in outputs:
            bitmap, metadata = make_soma_animation_frame(
                rom, args.animation, index, canvas_bounds)
            output.parent.mkdir(parents=True, exist_ok=True)
            if not output.exists() or output.read_bytes() != bitmap:
                output.write_bytes(bitmap)
            results.append((index, output, metadata))
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    for index, output, metadata in results:
        print(f"Wrote verified Soma {args.animation} frame {index}: {output}")
        details = (f"Size={metadata['width']}x{metadata['height']}; "
                   f"frame id={metadata['frame_id']}; "
                   f"duration={metadata['duration']}; "
                   f"sheet={metadata['sheet_index']}; "
                   f"quadrant={metadata['quadrant']}")
        if metadata["weapon"]:
            details += f"; knife frame={metadata['weapon']['frame_id']}"
        print(details)


if __name__ == "__main__":
    main()
