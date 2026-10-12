#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Zero Mission sound data for the runtime's sound engine.

The engine (src/audio.c, src/audio_wrappers.c, asm/audio_internal.s,
asm/soundcode.s, ported in src/runtime/mzm_audio.c) reads the songs, track
data, voice groups, DirectSound samples and programmable wave samples
through ROM addresses. This exporter copies the ROM range of the sound
objects (the ``sound/`` ``.rodata`` sections of the reference link map,
``sSoundDataEntries`` included) from the verified ROM, and evaluates the
engine tables of src/data/audio.c with scripts.mzm_c_tables. Output stays
private:

    assets/extracted/metroid/sound/runtime/sound_bank.bin
    assets/extracted/metroid/sound/runtime/sound_tables.tsv

``sound_bank.bin`` is ``MZMSND01``, the GBA base address and the size (two
little-endian u32), then the bytes.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import mzm_c_tables  # noqa: E402
from scripts.asset_layout import METROID, private_path  # noqa: E402
from scripts.mzm_samus_frame import EXPECTED_SHA1  # noqa: E402
from scripts.sprite_library import write_atomic  # noqa: E402

DECOMP = ROOT / "third_party/mzm"
SOUND_RUNTIME = METROID / "sound" / "runtime"
ROM_BASE = 0x08000000
SECTION_RE = re.compile(r"^ \.rodata\s+0x(08[0-9a-f]{6})\s+0x([0-9a-f]+)\s+sound/\S+\.o$", re.M)
# Engine tables of src/data/audio.c, in the order the runtime reads them.
TABLES = ("sUnk_808cad0", "sClockTable", "sNoiseTable", "sCgb3Vol", "sUnk_808cc4d",
          "sNativeSampleRate", "sMusicPitchData", "sPcmSamplesPerVBlankTable",
          "sArray_808cce2", "sArray_808cee2")
SOUND_ENTRY_SIZE = 8
# DetermineNewMusicTrack (src/music_wrappers.c) and the track of
# CheckSetNewMusicTrack for a room without music.
MUSIC_ALIASES = (("MUSIC_CHOZODIA_SURFACE", "MUSIC_CHOZO_RUINS"),
                 ("MUSIC_MAP_ROOM_2", "MUSIC_MAP_ROOM"),
                 ("MUSIC_SAVE_ELEVATOR_ROOM_2", "MUSIC_SAVE_ELEVATOR_ROOM"),
                 ("MUSIC_BEFORE_RUINS_TEST_ROOM", "MUSIC_BEFORE_RUINS_TEST_UNUSED"),
                 ("MUSIC_STEALTH_2", "MUSIC_STEALTH"))
DEFAULT_TRACK = "SOUND_A9"


def sound_range(link_map: str) -> tuple[int, int]:
    """The ROM span of every sound object's read-only data."""
    sections = [(int(start, 16), int(size, 16)) for start, size in SECTION_RE.findall(link_map)]
    if not sections:
        raise ValueError("no sound/ .rodata sections in the link map")
    start = min(begin for begin, _ in sections)
    end = max(begin + size for begin, size in sections)
    return start, end


def symbol(link_map: str, name: str) -> int:
    match = re.search(r"^\s+0x([0-9a-f]{8})\s+" + re.escape(name) + r"$", link_map, re.M)
    if not match:
        raise ValueError(f"{name} not in the link map")
    return int(match.group(1), 16)


# Element widths of the tables that are not u8.
WIDTHS = {"sUnk_808cad0": 2, "sNativeSampleRate": 4, "sMusicPitchData": 4,
          "sPcmSamplesPerVBlankTable": 2}


def table_lines(decomp: Path = DECOMP, rom: bytes | None = None,
                link_map: str | None = None) -> list[str]:
    """``table`` lines; with the ROM, every table is checked against the
    bytes at its symbol."""
    lines = []
    for name in TABLES:
        values = mzm_c_tables.table("audio.c", name, decomp)
        count = max(values) + 1 if values else 0
        dense = [int(v) for v in mzm_c_tables.dense(values, count)]
        if rom is not None and link_map is not None:
            width = WIDTHS.get(name, 1)
            code = {1: "B", 2: "H", 4: "I"}[width]
            stored = list(struct.unpack_from(f"<{count}{code}", rom,
                                             symbol(link_map, name) - ROM_BASE))
            if stored != [v & ((1 << 8 * width) - 1) for v in dense]:
                raise ValueError(f"{name} differs from the ROM")
            if name == "sUnk_808cad0":
                # GetNoteDelay reads one entry past the table for key 127.
                past, = struct.unpack_from("<H", rom, symbol(link_map, name) - ROM_BASE
                                           + 2 * count)
                dense.append(past)
        lines.append(f"table\t{name}\t" + ",".join(map(str, dense)))
    if rom is not None and link_map is not None:
        # unk_1ccc indexes sUnk_808cc4d with the LFO phase (-64 to 64).
        start = symbol(link_map, "sUnk_808cc4d") - ROM_BASE - 64
        lines.append("window\tsUnk_808cc4d\t" + ",".join(map(str, rom[start:start + 129])))
    return lines


def music_lines(decomp: Path = DECOMP) -> list[str]:
    """``alias`` pairs of DetermineNewMusicTrack and the ``default`` track."""
    names = mzm_c_tables.enum_values(decomp)
    return [*(f"alias\t{names[a]}\t{names[b]}" for a, b in MUSIC_ALIASES),
            f"default\t{names[DEFAULT_TRACK]}"]


def produce(root: Path, rom_path: Path, decomp: Path = DECOMP) -> dict:
    rom = Path(rom_path).read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError("original Zero Mission USA ROM SHA-1 mismatch")
    link_map = (decomp / "mzm_us.map").read_text(encoding="utf-8", errors="replace")
    start, end = sound_range(link_map)
    entries = symbol(link_map, "sSoundDataEntries")
    entries_size = next(int(size, 16) for begin, size in SECTION_RE.findall(link_map)
                        if int(begin, 16) == entries)
    if not start <= entries < end:
        raise ValueError("sSoundDataEntries outside the sound range")
    blob = rom[start - ROM_BASE:end - ROM_BASE]
    folder = private_path(Path(root), SOUND_RUNTIME, create=True)
    folder.mkdir(parents=True, exist_ok=True)
    write_atomic(folder / "sound_bank.bin",
                 b"MZMSND01" + struct.pack("<II", start, len(blob)) + blob)
    lines = ["schema\tmetroidvania-mzm-sound-v1",
             f"entries\t{entries}\t{entries_size // SOUND_ENTRY_SIZE}",
             *table_lines(decomp, rom, link_map), *music_lines(decomp)]
    write_atomic(folder / "sound_tables.tsv", "\n".join(lines) + "\n")
    return {"folder": folder, "start": start, "size": len(blob),
            "entries": entries_size // SOUND_ENTRY_SIZE}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--rom", type=Path, default=ROOT / "roms/Metroid - Zero Mission (USA).gba")
    parser.add_argument("--decomp", type=Path, default=DECOMP)
    args = parser.parse_args(argv)
    try:
        result = produce(args.root, args.rom, args.decomp)
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    print(f"Zero Mission sound bank: 0x{result['start']:08x}, {result['size']} bytes, "
          f"{result['entries']} sound entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
