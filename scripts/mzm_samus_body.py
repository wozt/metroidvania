# SPDX-License-Identifier: GPL-3.0-only
"""Compose Samus body OAM with explicit palette banks from an owned ROM."""
import argparse
import hashlib
from pathlib import Path
try:
    from scripts.mzm_samus_frame import EXPECTED_SHA1, stage
    from scripts.mzm_samus_oam_decode import decode_raw_samus_oam
    from scripts.gba_oam import compose, to_bmp
except ModuleNotFoundError:
    from mzm_samus_frame import EXPECTED_SHA1, stage
    from mzm_samus_oam_decode import decode_raw_samus_oam
    from gba_oam import compose, to_bmp


def load_palette_banks(rom, offset, rows=1, first_bank=0):
    if not 1 <= rows <= 16 or not 0 <= first_bank < 16 or first_bank + rows > 16:
        raise ValueError("palette bank range must fit within 0..15")
    if offset < 0 or offset % 2 or offset + rows * 32 > len(rom):
        raise ValueError("palette data out of ROM bounds or misaligned")
    palette = bytearray(512)
    palette[first_bank*32:(first_bank+rows)*32] = rom[offset:offset+rows*32]
    return bytes(palette)


def make_body(rom, frame_pointer, palette_offset, width=128, height=128,
              origin_x=-64, origin_y=-64, palette_rows=1, first_bank=0):
    if not 1 <= width <= 512 or not 1 <= height <= 512:
        raise ValueError("invalid canvas dimensions")
    palette = load_palette_banks(rom, palette_offset, palette_rows, first_bank)
    vram, meta = stage(rom, frame_pointer)
    oam = decode_raw_samus_oam(rom, meta["oam_pointer"])
    absent = sorted({e["bank"] for e in oam["entries"]
                     if not first_bank <= e["bank"] < first_bank + palette_rows})
    if absent:
        raise ValueError("OAM uses unloaded OBJ palette banks: " + repr(absent))
    rgba = compose(vram, palette, oam["entries"], origin_x, origin_y, width, height)
    return to_bmp(rgba, width, height), meta, oam


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--frame-pointer", type=lambda s: int(s, 0), required=True)
    p.add_argument("--palette-offset", type=lambda s: int(s, 0), required=True)
    p.add_argument("--palette-rows", type=int, default=1)
    p.add_argument("--first-bank", type=int, default=0)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parent.parent
    allowed = root / "assets" / "extracted"
    output = a.output.absolute()
    if output.suffix.lower() != ".bmp" or allowed not in output.parents:
        p.error("output must be a BMP inside assets/extracted")
    if any(x.is_symlink() for x in (output, *output.parents) if x == allowed or allowed in x.parents):
        p.error("symlink output path refused")
    try:
        rom = a.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            p.error("expected unmodified Zero Mission USA ROM")
        bmp, meta, oam = make_body(rom, a.frame_pointer, a.palette_offset,
                                   palette_rows=a.palette_rows, first_bank=a.first_bank)
        output.parent.mkdir(parents=True, exist_ok=True)
        if not output.exists() or output.read_bytes() != bmp:
            output.write_bytes(bmp)
    except (OSError, ValueError) as exc:
        p.error(str(exc))
    print(f"Local Samus BODY preview: {output}")
    print(f"Frame duration={meta['duration']}; body OAM parts={oam['count']}")
    print("Palette position and identity must be verified. No arm cannon or effects.")


if __name__ == "__main__":
    main()
