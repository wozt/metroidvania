#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Zero Mission ROM data bank for the runtime's native screens.

The HUD, the minimap, the menus and the cutscenes of the decompilation read
their graphics, tilemaps, palettes, OAM and tables through ROM addresses
(often compressed, and often through pointer tables). Instead of exporting
each object separately, this exporter copies the verified ROM into a private
bank and lists the data symbols of the reference build
(``third_party/mzm/mzm_us.elf``: name, GBA address and size), which the
runtime (src/runtime/mzm_rom.c) looks up by name. Output stays private:

    assets/extracted/metroid/rom/rom_bank.bin
    assets/extracted/metroid/rom/rom_symbols.tsv

``rom_bank.bin`` is ``MZMROM01``, the GBA base address and the size (two
little-endian u32), then the bytes. ``rom_symbols.tsv`` has one
``symbol<TAB>name<TAB>address<TAB>size`` line per sized data object in ROM;
a name defined by several objects (file-local statics) keeps its first
definition and is reported.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.asset_layout import METROID, private_path  # noqa: E402
from scripts.mzm_samus_frame import EXPECTED_SHA1  # noqa: E402
from scripts.sprite_library import write_atomic  # noqa: E402

DECOMP = ROOT / "third_party/mzm"
ROM_RUNTIME = METROID / "rom"
ROM_BASE = 0x08000000
STT_OBJECT = 1


def elf_symbols(elf: bytes) -> list[tuple[str, int, int]]:
    """The sized STT_OBJECT symbols of a little-endian ELF32 file."""
    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 1:
        raise ValueError("not a little-endian ELF32 file")
    shoff, = struct.unpack_from("<I", elf, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", elf, 0x2E)
    sections = [struct.unpack_from("<IIIIIIIIII", elf, shoff + i * shentsize)
                for i in range(shnum)]
    symbols = []
    for _name, kind, _flags, _addr, offset, size, link, _info, _align, entsize in sections:
        if kind != 2:   # SHT_SYMTAB
            continue
        strings = sections[link]
        names = elf[strings[4]:strings[4] + strings[5]]
        for i in range(size // entsize):
            st_name, value, st_size, info, _other, _shndx = struct.unpack_from(
                "<IIIBBH", elf, offset + i * entsize)
            if info & 0xF != STT_OBJECT or st_size == 0:
                continue
            end = names.index(b"\0", st_name)
            symbols.append((names[st_name:end].decode("ascii"), value, st_size))
    if not symbols:
        raise ValueError("no object symbols in the ELF symbol table")
    return symbols


def symbol_lines(symbols: list[tuple[str, int, int]], rom_size: int) -> tuple[list[str], list[str]]:
    """``symbol`` lines of the objects inside the ROM, and the names defined
    more than once (their first definition wins)."""
    lines, seen, duplicates = [], set(), []
    for name, address, size in sorted(symbols, key=lambda item: (item[1], item[0])):
        if not ROM_BASE <= address < ROM_BASE + rom_size:
            continue
        if address + size > ROM_BASE + rom_size:
            raise ValueError(f"{name} ends past the ROM")
        if name in seen:
            duplicates.append(name)
            continue
        seen.add(name)
        lines.append(f"symbol\t{name}\t{address}\t{size}")
    return lines, sorted(set(duplicates))


def produce(root: Path, rom_path: Path, decomp: Path = DECOMP) -> dict:
    rom = Path(rom_path).read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError("original Zero Mission USA ROM SHA-1 mismatch")
    lines, duplicates = symbol_lines(elf_symbols((decomp / "mzm_us.elf").read_bytes()), len(rom))
    folder = private_path(Path(root), ROM_RUNTIME, create=True)
    folder.mkdir(parents=True, exist_ok=True)
    write_atomic(folder / "rom_bank.bin", b"MZMROM01" + struct.pack("<II", ROM_BASE, len(rom)) + rom)
    write_atomic(folder / "rom_symbols.tsv",
                 "\n".join(["schema\tmetroidvania-mzm-rom-v1", *lines]) + "\n")
    return {"folder": folder, "symbols": len(lines), "duplicates": duplicates}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--rom", type=Path, default=ROOT / "roms/Metroid - Zero Mission (USA).gba")
    parser.add_argument("--decomp", type=Path, default=DECOMP)
    args = parser.parse_args(argv)
    try:
        result = produce(args.root, args.rom, args.decomp)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Zero Mission ROM bank: {result['symbols']} data symbols, "
          f"{len(result['duplicates'])} names defined more than once")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
