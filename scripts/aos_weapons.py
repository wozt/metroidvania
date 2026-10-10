#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Export Soma's weapon attack data from the original Aria of Sorrow ROM.

``sub_08023368`` returns the weapon record ``sUnk_08505D3C[weapon]``
(0x1C bytes: item id at +0, class at +8, flags at +0x10, variant at +0x16)
or the fallback record ``sUnk_084F1270`` when nothing is equipped (0xFF).
``sub_080233BC(posture)`` picks Soma's body animation from the tables of
``sUnk_084F1238``: postures 0-2 (standing, crouched, airborne attack) are
indexed by class * 3 + variant, postures 3-4 (standing and crouched
recovery) by class. Records are read while their item id follows 0x20 + n.

The weapon entity of classes 0, 2 and 3 (``sub_080221CC``) loads the tile
sheet ``sUnk_084F10C0[record + 0x12]``, the frame/animation descriptor
``sUnk_084F117C[record + 0x13]`` (u16 record count, u16 animation count, OAM
frame records, unused word, animation table), bank ``record + 0x15`` of the
palette descriptor 0x082098B8, and plays animation ``record + 0x14`` once. A
frame record has its component count at +5, a hitbox flag at +4 and a
pointer at +8 to the hitbox (signed x, y, then width and height, relative to
Soma's position while facing right). Each weapon's animation is exported as
a sprite sequence ``Weapon/<name>`` and its per-frame hitboxes as rows of
``weapon_frames.tsv``. Classes 1, 4 and 5 use other entities
(``sub_080224BC``, ``sub_08022A54``, ``sub_08022DEC``) and are exported the
same way, as their tables are shared; their behaviour is not ported.
Components whose source lies outside the weapon's tile sheet (for example a
64x64 component at Soma's body anchor in the knife frames) read other VRAM
and are skipped and counted.
Output stays private:

    assets/extracted/aria/metadata/weapons.tsv
    assets/extracted/aria/metadata/weapon_frames.tsv
    assets/extracted/aria/sprites/weapons/runtime/runtime_index.tsv
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
from scripts.aos_object_sprites import frame_components, render_frame, tile_sheet
from scripts.aos_soma_sprite import EXPECTED_SHA1, _rom_slice, bgr555
from scripts.asset_layout import ARIA_METADATA, ARIA_SPRITES, private_path
from scripts.sprite_library import LibraryWriter, bmp_from_pixels, write_atomic

SCHEMA = "metroidvania-aos-weapons-v1"
WEAPON_TABLE = 0x08505D3C
UNARMED_RECORD = 0x084F1270
POSTURE_TABLES = 0x084F1238
RECORD_SIZE = 0x1C
MAX_WEAPONS = 256
WEAPON_TILES = 0x084F10C0
WEAPON_FRAMES = 0x084F117C
WEAPON_PALETTE = 0x082098B8
ARIA_WEAPONS_RUNTIME = ARIA_SPRITES / "weapons" / "runtime"
FRAME_SCHEMA = "metroidvania-aos-weapon-frames-v1"


def weapon_record(rom: bytes, pointer: int) -> dict:
    record = _rom_slice(rom, pointer, RECORD_SIZE, "weapon record")
    return {"item": record[0], "class": record[8],
            "flags": struct.unpack_from("<H", record, 0x10)[0], "variant": record[0x16],
            "tiles": record[0x12], "frames": record[0x13], "animation": record[0x14],
            "bank": record[0x15]}


def _pointer(rom: bytes, table: int, index: int) -> int:
    return struct.unpack("<I", _rom_slice(rom, table + index * 4, 4, "pointer table"))[0]


def weapon_animation(rom: bytes, record: dict) -> list[dict]:
    """Frames of the weapon entity animation, with sprite pixels and hitbox."""
    descriptor = _pointer(rom, WEAPON_FRAMES, record["frames"])
    record_count, animation_count, records, _, animations = struct.unpack(
        "<HHIII", _rom_slice(rom, descriptor, 16, "weapon descriptor"))
    if not 0 <= record["animation"] < animation_count:
        raise ValueError("weapon animation outside its descriptor")
    animation = _pointer(rom, animations, record["animation"])
    count, encoding = struct.unpack("<HH", _rom_slice(rom, animation, 4, "weapon animation"))
    if encoding != 1 or not 0 < count <= 64:
        raise ValueError("unsupported weapon animation encoding")
    tiles, sheet_width, _ = tile_sheet(rom, _pointer(rom, WEAPON_TILES, record["tiles"]))
    palette = _rom_slice(rom, WEAPON_PALETTE + 4 + record["bank"] * 32, 32, "weapon palette")
    colors = [bgr555(struct.unpack_from("<H", palette, i * 2)[0]) for i in range(16)]
    frames = []
    for index in range(count):
        frame_id, duration = _rom_slice(rom, animation + 4 + index * 4, 2, "weapon frame")
        if not 0 <= frame_id < record_count or not duration:
            raise ValueError("invalid weapon animation frame")
        frame_record = _rom_slice(rom, records + frame_id * 16, 16, "weapon frame record")
        hitbox = None
        if frame_record[4]:
            box = struct.unpack_from("<I", frame_record, 8)[0]
            hitbox = struct.unpack("<bbBB", _rom_slice(rom, box, 4, "weapon hitbox"))
        components = frame_components(rom, descriptor, frame_id)
        outside = []
        pixels = render_frame(tiles, sheet_width, components, colors, outside)
        frames.append({"frame": frame_id, "duration": duration, "pixels": pixels,
                       "hitbox": hitbox, "outside": len(outside)})
    return frames


def posture_tables(rom: bytes) -> list[int]:
    return list(struct.unpack("<5I", _rom_slice(rom, POSTURE_TABLES, 20, "posture tables")))


def attack_animations(rom: bytes, tables: list[int], weapon: dict) -> list[int]:
    """sub_080233BC for postures 0-4."""
    anims = []
    for posture, table in enumerate(tables):
        index = weapon["class"] * 3 + weapon["variant"] if posture <= 2 else weapon["class"]
        anims.append(_rom_slice(rom, table + index, 1, "posture table")[0])
    return anims


def weapons(rom: bytes) -> list[tuple[str, dict, list[int]]]:
    tables = posture_tables(rom)
    rows = [("none", weapon_record(rom, UNARMED_RECORD))]
    for index in range(MAX_WEAPONS):
        record = weapon_record(rom, WEAPON_TABLE + index * RECORD_SIZE)
        if record["item"] != 0x20 + index or record["class"] > 5 or record["variant"] > 2:
            break
        rows.append((str(index), record))
    return [(name, record, attack_animations(rom, tables, record)) for name, record in rows]


def encode(rows) -> str:
    lines = ["schema\t" + SCHEMA,
             "# weapon\titem\tclass\tvariant\tflags\tstand\tcrouch\tair\trecover\tcrouch_recover"]
    for name, record, anims in rows:
        lines.append("\t".join([name, f"0x{record['item']:02x}", str(record["class"]),
                                str(record["variant"]), f"0x{record['flags']:04x}",
                                *map(str, anims)]))
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba")
    args = parser.parse_args(argv)
    try:
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("original Aria of Sorrow USA ROM SHA-1 mismatch")
        rows = weapons(rom)
        folder = private_path(Path(args.root), ARIA_METADATA, create=True)
        folder.mkdir(parents=True, exist_ok=True)
        write_atomic(folder / "weapons.tsv", encode(rows))
        library = LibraryWriter(Path(args.root), ARIA_WEAPONS_RUNTIME)
        frame_lines = ["schema\t" + FRAME_SCHEMA,
                       "# weapon\tindex\tframe\tticks\thit\tx\ty\twidth\theight"]
        skipped, outside = [], 0
        for name, record, _ in rows:
            try:
                frames = weapon_animation(rom, record)
            except ValueError as exc:
                skipped.append(f"{name}: {exc}")
                continue
            sprites = []
            outside += sum(frame["outside"] for frame in frames)
            for index, frame in enumerate(frames):
                hit = frame["hitbox"] or (0, 0, 0, 0)
                frame_lines.append("\t".join(map(str, (
                    name, index, frame["frame"], frame["duration"],
                    1 if frame["hitbox"] else 0, *hit))))
                if frame["pixels"]:
                    bmp, left, top = bmp_from_pixels(frame["pixels"])
                else:   # an empty frame: one transparent pixel keeps the timing
                    bmp, left, top = bmp_from_pixels({(0, 0): (0, 0, 0)})
                sprites.append((bmp, frame["duration"], left, top))
            library.add(f"Weapon/{name}", sprites)
        write_atomic(folder / "weapon_frames.tsv", "\n".join(frame_lines) + "\n")
        totals = library.finish()
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Aria weapons: {len(rows) - 1} weapons and the unarmed record; "
          f"{totals['sequences']} weapon sprite sequences; {outside} components "
          "outside their tile sheet skipped")
    for line in skipped:
        print("  skipped", line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
