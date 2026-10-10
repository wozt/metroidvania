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
Output stays private:

    assets/extracted/aria/metadata/weapons.tsv
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
from scripts.aos_soma_sprite import EXPECTED_SHA1, _rom_slice
from scripts.asset_layout import ARIA_METADATA, private_path
from scripts.sprite_library import write_atomic

SCHEMA = "metroidvania-aos-weapons-v1"
WEAPON_TABLE = 0x08505D3C
UNARMED_RECORD = 0x084F1270
POSTURE_TABLES = 0x084F1238
RECORD_SIZE = 0x1C
MAX_WEAPONS = 256


def weapon_record(rom: bytes, pointer: int) -> dict:
    record = _rom_slice(rom, pointer, RECORD_SIZE, "weapon record")
    return {"item": record[0], "class": record[8],
            "flags": struct.unpack_from("<H", record, 0x10)[0], "variant": record[0x16]}


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
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Aria weapons: {len(rows) - 1} weapons and the unarmed record")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
