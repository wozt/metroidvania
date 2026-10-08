#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a complete verified Power Suit idle frame from a personal MZM ROM."""
import argparse
import hashlib
from pathlib import Path
import struct

try:
    from scripts.gba_oam import compose, to_bmp
    from scripts.mzm_samus_body import load_palette_banks
    from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset, stage
    from scripts.mzm_samus_oam_decode import decode_raw_samus_oam
except ModuleNotFoundError:
    from gba_oam import compose, to_bmp
    from mzm_samus_body import load_palette_banks
    from mzm_samus_frame import EXPECTED_SHA1, rom_offset, stage
    from mzm_samus_oam_decode import decode_raw_samus_oam


# Exact symbols from the pinned matching mzm_us build. These addresses are for
# the SHA-1-gated USA ROM only, so they are never applied to another revision.
POWER_SUIT_IDLE_RIGHT_FRAME = 0x08248744
POWER_SUIT_DEFAULT_PALETTE_OFFSET = 0x002376A8
ARM_CANNON_IDLE_RIGHT_ANIMATION = 0x08234430
ARM_CANNON_FORWARD_UPPER_GFX = 0x082337EC
ARM_CANNON_FORWARD_LOWER_GFX = 0x082338AC
ARM_CANNON_GFX_BYTES = 64
ARM_CANNON_UPPER_VRAM = 0x800
ARM_CANNON_LOWER_VRAM = 0xC00
POWER_SUIT_IDLE_FRAME_COUNT = 4
SAMUS_ANIMATION_RECORD_BYTES = 16
ARM_CANNON_ANIMATION_RECORD_BYTES = 8


def _rom_slice(rom, pointer, size, label):
    offset = rom_offset(pointer, rom)
    if offset + size > len(rom):
        raise ValueError(f"truncated {label}")
    return rom[offset:offset + size]


def parse_arm_cannon_animation(rom, pointer):
    raw = _rom_slice(rom, pointer, 8, "arm cannon animation")
    offset_pointer, oam_pointer = struct.unpack("<II", raw)
    offset_raw = _rom_slice(rom, offset_pointer, 4, "arm cannon offset")
    y, x = struct.unpack("<HH", offset_raw)
    y = y - 0x100 if y & 0x80 else y
    x = x - 0x200 if x & 0x100 else x
    return {
        "animation_pointer": pointer,
        "offset_pointer": offset_pointer,
        "oam_pointer": oam_pointer,
        # The game increments the decoded Y value after loading it.
        "muzzle_offset": (x, y + 1),
    }


def stage_arm_cannon(rom, body_vram, upper_pointer, lower_pointer):
    vram = bytearray(body_vram)
    if len(vram) < ARM_CANNON_LOWER_VRAM + ARM_CANNON_GFX_BYTES:
        raise ValueError("OBJ VRAM buffer is too small for the arm cannon")
    upper = _rom_slice(rom, upper_pointer, ARM_CANNON_GFX_BYTES,
                       "upper arm cannon graphics")
    lower = _rom_slice(rom, lower_pointer, ARM_CANNON_GFX_BYTES,
                       "lower arm cannon graphics")
    vram[ARM_CANNON_UPPER_VRAM:ARM_CANNON_UPPER_VRAM + ARM_CANNON_GFX_BYTES] = upper
    vram[ARM_CANNON_LOWER_VRAM:ARM_CANNON_LOWER_VRAM + ARM_CANNON_GFX_BYTES] = lower
    return bytes(vram)


def ordered_entries(body, cannon):
    entries = []
    if cannon["arm_cannon_front"]:
        entries.extend(cannon["entries"])
    entries.extend(body["entries"])
    if cannon["arm_cannon_behind"]:
        entries.extend(cannon["entries"])
    return entries


def crop_rgba(rgba, width, height):
    points = [(x, y) for y in range(height) for x in range(width)
              if rgba[(y * width + x) * 4 + 3]]
    if not points:
        raise ValueError("composed sprite is fully transparent")
    left = min(x for x, _ in points)
    top = min(y for _, y in points)
    right = max(x for x, _ in points) + 1
    bottom = max(y for _, y in points) + 1
    cropped = bytearray((right - left) * (bottom - top) * 4)
    for y in range(top, bottom):
        src = (y * width + left) * 4
        dst = ((y - top) * (right - left)) * 4
        size = (right - left) * 4
        cropped[dst:dst + size] = rgba[src:src + size]
    return bytes(cropped), right - left, bottom - top


def make_power_suit_idle_right(rom, frame_index=0):
    if not 0 <= frame_index < POWER_SUIT_IDLE_FRAME_COUNT:
        raise ValueError("Power Suit idle frame index must be 0..3")
    palette = load_palette_banks(rom, POWER_SUIT_DEFAULT_PALETTE_OFFSET, 2, 0)
    frame_pointer = (POWER_SUIT_IDLE_RIGHT_FRAME +
                     frame_index * SAMUS_ANIMATION_RECORD_BYTES)
    vram, frame = stage(rom, frame_pointer)
    body = decode_raw_samus_oam(rom, frame["oam_pointer"])
    cannon_pointer = (ARM_CANNON_IDLE_RIGHT_ANIMATION +
                      frame_index * ARM_CANNON_ANIMATION_RECORD_BYTES)
    cannon_animation = parse_arm_cannon_animation(rom, cannon_pointer)
    cannon = decode_raw_samus_oam(rom, cannon_animation["oam_pointer"])
    vram = stage_arm_cannon(rom, vram, ARM_CANNON_FORWARD_UPPER_GFX,
                            ARM_CANNON_FORWARD_LOWER_GFX)
    entries = ordered_entries(body, cannon)
    banks = {entry["bank"] for entry in entries}
    if not banks <= {0, 1}:
        raise ValueError(f"idle frame uses unexpected OBJ palette banks: {sorted(banks)}")

    left = min(entry["x"] for entry in entries)
    top = min(entry["y"] for entry in entries)
    right = max(entry["x"] + entry["w"] for entry in entries)
    bottom = max(entry["y"] + entry["h"] for entry in entries)
    width, height = right - left, bottom - top
    rgba = compose(vram, palette, entries, left, top, width, height, "2d")
    rgba, width, height = crop_rgba(rgba, width, height)
    metadata = {
        "frame": frame,
        "body_oam": body,
        "arm_cannon": cannon,
        "arm_cannon_animation": cannon_animation,
        "width": width,
        "height": height,
    }
    return to_bmp(rgba, width, height), metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    output_group = parser.add_mutually_exclusive_group(required=True)
    output_group.add_argument("--output", type=Path,
                              help="write one BMP selected by --frame")
    output_group.add_argument("--output-dir", type=Path,
                              help="write idle_0.bmp through idle_3.bmp")
    parser.add_argument("--frame", type=int, choices=range(POWER_SUIT_IDLE_FRAME_COUNT),
                        default=0)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    allowed = (root / "assets" / "extracted").resolve()
    if args.output:
        outputs = [(args.frame, args.output.resolve())]
    else:
        output_dir = args.output_dir.resolve()
        outputs = [(index, output_dir / f"idle_{index}.bmp")
                   for index in range(POWER_SUIT_IDLE_FRAME_COUNT)]
    for _, output in outputs:
        if output.suffix.lower() != ".bmp" or allowed not in output.parents:
            parser.error(f"output must be a BMP below {allowed}")
        if any(path.is_symlink() for path in (output, *output.parents)
               if path == allowed or allowed in path.parents):
            parser.error("symlink output path refused")
    try:
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            parser.error("expected unmodified Zero Mission USA ROM")
        results = []
        for index, output in outputs:
            bmp, metadata = make_power_suit_idle_right(rom, index)
            output.parent.mkdir(parents=True, exist_ok=True)
            if not output.exists() or output.read_bytes() != bmp:
                output.write_bytes(bmp)
            results.append((index, output, metadata))
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    for index, output, metadata in results:
        cannon = metadata["arm_cannon"]
        muzzle = metadata["arm_cannon_animation"]["muzzle_offset"]
        print(f"Wrote verified Power Suit idle frame {index}: {output}")
        print(f"Size={metadata['width']}x{metadata['height']}; "
              f"duration={metadata['frame']['duration']}; "
              f"body parts={metadata['body_oam']['count']}; "
              f"arm cannon parts={cannon['count']}; muzzle offset={muzzle}")


if __name__ == "__main__":
    main()
