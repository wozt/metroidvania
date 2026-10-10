#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the private Aria object sprite library from the original ROM.

Only objects whose graphics setup is traced are exported. The wooden door
(special object 0x00) is set up by ``sub_0804D8F0``: ``sub_0806E0D0`` loads
the tile sheet descriptor 0x081CBE0C (encoding 1: LZ77 tiles), the palette
descriptor 0x08209AE0 and the OAM frame list 0x0820F160; the entity shows
sprite frame 0 (parameter 0) or 5, and ``sub_0803CC70`` cycles its palette
with the script 0x08525564 or 0x08525574 (u16 entry count, then 4-byte
entries: palette bank, duration in frames). Each door style is exported as a
looping sequence: its sprite frame colorized with each bank of the script.
Enemies whose create routine is traced (``ENEMIES``) are exported the same
way: every animation of their descriptor, colorized with their palette bank,
as ``Enemy/<name>/anim_<n>``, with per-frame boxes (frame record +4 / +8) in
``enemy_frames.tsv``. Offsets are relative to the entity position (the door
base); components
listed first are drawn on top (assumed OAM order; the door's components do
not overlap). Output is private:

    assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv
    assets/extracted/aria/metadata/enemy_frames.tsv
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.aos_room_render import lz77_at
from scripts.aos_soma_sprite import EXPECTED_SHA1, TILE_BYTES, _rom_slice, bgr555
from scripts.asset_layout import ARIA_METADATA, ARIA_SPRITES, private_path
from scripts.sprite_library import LibraryWriter, bmp_from_pixels, write_atomic

ARIA_OBJECTS_RUNTIME = ARIA_SPRITES / "objects" / "runtime"
ENEMY_FRAMES_SCHEMA = "metroidvania-aos-enemy-frames-v1"
# Enemies whose create routine is traced: graphics setup of sub_0806E0D0
# (tile descriptor, palette descriptor and bank, frame/animation descriptor).
ENEMIES = {
    "bat": {"id": 0x00, "graphics": 0x081F422C, "palette": 0x0820BD4C, "bank": 0,
            "frames": 0x0824B2C4},     # EnemyBatCreate 0x080AD2A8
}
WOODEN_DOOR = {
    "graphics": 0x081CBE0C,
    "palette": 0x08209AE0,
    "frames": 0x0820F160,
    "styles": ((0, 0x08525564), (5, 0x08525574)),
}


def tile_sheet(rom: bytes, descriptor: int) -> tuple[bytes, int, int]:
    """Tiles of a graphics descriptor: (encoding, bpp, width, height, data)."""
    encoding, bits, width, height = _rom_slice(rom, descriptor, 4, "graphics descriptor")
    if bits != 4 or not width or not height:
        raise ValueError("unsupported object graphics descriptor")
    size = width * height * TILE_BYTES
    if encoding == 1:
        pointer = struct.unpack("<I", _rom_slice(rom, descriptor + 4, 4, "tile pointer"))[0]
        data = lz77_at(rom, pointer)
        if len(data) < size:
            raise ValueError("compressed object tiles are shorter than their sheet")
        return data[:size], width, height
    if encoding == 0:
        return _rom_slice(rom, descriptor + 4, size, "object tiles"), width, height
    raise ValueError(f"unsupported object graphics encoding {encoding}")


def palette_bank(rom: bytes, descriptor: int, bank: int) -> list[tuple[int, int, int]]:
    encoding, _, count, _ = _rom_slice(rom, descriptor, 4, "palette descriptor")
    if encoding != 0 or not 0 <= bank < count:
        raise ValueError("unsupported object palette bank")
    data = _rom_slice(rom, descriptor + 4 + bank * 32, 32, "object palette")
    return [bgr555(struct.unpack_from("<H", data, i * 2)[0]) for i in range(16)]


def palette_script(rom: bytes, pointer: int) -> list[tuple[int, int]]:
    """Entries (bank, duration) of a sub_0803CC70 palette script."""
    count = struct.unpack("<H", _rom_slice(rom, pointer, 2, "palette script"))[0]
    if not 0 < count <= 64:
        raise ValueError("invalid palette script length")
    data = _rom_slice(rom, pointer + 4, count * 4, "palette script entries")
    return [(data[i * 4], data[i * 4 + 1]) for i in range(count)]


def frame_components(rom: bytes, frames_descriptor: int, frame: int) -> list[tuple]:
    count, _, records = struct.unpack("<HHI", _rom_slice(rom, frames_descriptor, 8,
                                                         "frame descriptor"))
    if not 0 <= frame < count:
        raise ValueError("object frame outside its frame list")
    record = _rom_slice(rom, records + frame * 16, 16, "object frame record")
    components = struct.unpack_from("<I", record, 12)[0]
    result = []
    for index in range(record[5]):
        x, y, reserved, source_x, source_y, width, height, flags = struct.unpack(
            "<bbHBBBBI", _rom_slice(rom, components + index * 12, 12, "OAM component"))
        if reserved or source_x % 8 or source_y % 8 or width % 8 or height % 8:
            raise ValueError("unsupported object OAM component")
        result.append((x, y, source_x, source_y, width, height, flags))
    return result


def render_frame(tiles: bytes, sheet_width: int, components: list[tuple],
                 colors: list[tuple[int, int, int]], skipped: list | None = None) -> dict:
    """Opaque pixels of a frame facing right, relative to the entity position.

    The component word is the bytes +8..+0xB that ``sub_0804311C`` (the
    IWRAM OAM builder) reads: +8 shape | size << 4, +9 an alternate tile base
    used only in animation mode 2, +0xA bit 0 vertical flip and bit 1
    horizontal flip relative to the entity facing, +0xB a palette offset
    mask. Components whose source lies outside the sheet read other VRAM and
    are skipped (counted in ``skipped``)."""
    pixels = {}
    sheet_height = len(tiles) // (sheet_width * TILE_BYTES) * 8
    for x, y, source_x, source_y, width, height, flags in reversed(components):
        if flags >> 24 or (flags >> 16) & ~3:
            raise ValueError("unsupported OAM component attributes")
        if source_x + width > sheet_width * 8 or source_y + height > sheet_height:
            if skipped is not None:
                skipped.append((x, y, source_x, source_y, width, height))
            continue
        vflip, hflip = (flags >> 16) & 1, (flags >> 17) & 1
        for py in range(height):
            for px in range(width):
                sx = source_x + (width - 1 - px if hflip else px)
                sy = source_y + (height - 1 - py if vflip else py)
                tile = (sy // 8) * sheet_width + sx // 8
                packed = tiles[tile * TILE_BYTES + (sy % 8) * 4 + (sx % 8) // 2]
                index = (packed >> (4 * (sx & 1))) & 15
                if index:
                    pixels[(x + px, y + py)] = colors[index]
    return pixels


def descriptor_animations(rom: bytes, entity: dict) -> list[list[dict]]:
    """Every animation of an entity's frame/animation descriptor (u16 frame
    record count, u16 animation count, records, a word, animation table;
    encoding 1 animations of (frame, duration) entries)."""
    record_count, animation_count, records, _, table = struct.unpack(
        "<HHIII", _rom_slice(rom, entity["frames"], 16, "entity descriptor"))
    tiles, sheet_width, _ = tile_sheet(rom, entity["graphics"])
    colors = palette_bank(rom, entity["palette"], entity["bank"])
    result = []
    for index in range(animation_count):
        pointer = struct.unpack("<I", _rom_slice(rom, table + index * 4, 4, "animation"))[0]
        count, encoding = struct.unpack("<HH", _rom_slice(rom, pointer, 4, "animation header"))
        if encoding != 1 or not 0 < count <= 64:
            raise ValueError("unsupported entity animation encoding")
        frames = []
        for step in range(count):
            frame_id, duration = _rom_slice(rom, pointer + 4 + step * 4, 2, "animation frame")
            if not 0 <= frame_id < record_count or not duration:
                raise ValueError("invalid entity animation frame")
            record = _rom_slice(rom, records + frame_id * 16, 16, "frame record")
            box = None
            if record[4]:
                box_pointer = struct.unpack_from("<I", record, 8)[0]
                box = struct.unpack("<bbBB", _rom_slice(rom, box_pointer, 4, "frame box"))
            pixels = render_frame(tiles, sheet_width,
                                  frame_components(rom, entity["frames"], frame_id), colors)
            frames.append({"frame": frame_id, "duration": duration, "pixels": pixels,
                           "box": box})
        result.append(frames)
    return result


def produce(root: Path, rom_path: Path) -> dict:
    rom = Path(rom_path).read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError("original Aria of Sorrow USA ROM SHA-1 mismatch")
    library = LibraryWriter(root, ARIA_OBJECTS_RUNTIME)
    door = WOODEN_DOOR
    tiles, sheet_width, _ = tile_sheet(rom, door["graphics"])
    for style, (frame, script) in enumerate(door["styles"]):
        components = frame_components(rom, door["frames"], frame)
        frames = []
        for bank, duration in palette_script(rom, script):
            pixels = render_frame(tiles, sheet_width, components,
                                  palette_bank(rom, door["palette"], bank))
            bmp, left, top = bmp_from_pixels(pixels)
            frames.append((bmp, duration, left, top))
        library.add(f"WoodenDoor/style_{style}", frames)
    lines = ["schema\t" + ENEMY_FRAMES_SCHEMA,
             "# enemy\tanimation\tindex\tframe\tticks\tbox\tx\ty\twidth\theight"]
    for name, entity in ENEMIES.items():
        for number, animation in enumerate(descriptor_animations(rom, entity)):
            sprites = []
            for index, frame in enumerate(animation):
                pixels = frame["pixels"] or {(0, 0): (0, 0, 0)}
                bmp, left, top = bmp_from_pixels(pixels)
                sprites.append((bmp, frame["duration"], left, top))
                box = frame["box"] or (0, 0, 0, 0)
                lines.append("\t".join(map(str, (name, number, index, frame["frame"],
                                                  frame["duration"], 1 if frame["box"] else 0,
                                                  *box))))
            library.add(f"Enemy/{name}/anim_{number}", sprites)
    folder = private_path(Path(root), ARIA_METADATA, create=True)
    folder.mkdir(parents=True, exist_ok=True)
    write_atomic(folder / "enemy_frames.tsv", "\n".join(lines) + "\n")
    return library.finish()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba")
    args = parser.parse_args(argv)
    try:
        totals = produce(args.root, args.rom)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Aria object library: {totals['sequences']} sequences, {totals['frames']} frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
