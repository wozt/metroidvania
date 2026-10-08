# SPDX-License-Identifier: GPL-3.0-only
"""Render Samus BODY OAM using local ROM and a caller-verified OBJ palette."""
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


def make_body(rom, frame_pointer, palette_offset, width=128, height=128,
              origin_x=-64, origin_y=-64):
    if not 1 <= width <= 512 or not 1 <= height <= 512:
        raise ValueError("invalid canvas dimensions")
    if palette_offset < 0 or palette_offset % 2 or palette_offset + 32 > len(rom):
        raise ValueError("invalid 16-color palette offset")
    vram, meta = stage(rom, frame_pointer)
    oam = decode_raw_samus_oam(rom, meta["oam_pointer"])
    if any(entry["bank"] != 0 for entry in oam["entries"]):
        raise ValueError("single-row palette mode cannot render multiple palette banks")
    palette = rom[palette_offset:palette_offset+32] + bytes(480)
    rgba = compose(vram, palette, oam["entries"],
                   origin_x, origin_y, width, height)
    return to_bmp(rgba, width, height), meta, oam


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--frame-pointer", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--palette-offset", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    allowed = root / "assets" / "extracted"
    output = args.output.absolute()
    if output.suffix.lower() != ".bmp" or allowed not in output.parents:
        parser.error("output must be a .bmp inside assets/extracted")
    if any(part.is_symlink() for part in (output, *output.parents) if part == allowed or allowed in part.parents):
        parser.error("symlink output path refused")
    try:
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            parser.error("expected unmodified Zero Mission USA ROM")
        bmp, meta, oam = make_body(rom, args.frame_pointer, args.palette_offset)
        output.parent.mkdir(parents=True, exist_ok=True)
        if not output.exists() or output.read_bytes() != bmp:
            output.write_bytes(bmp)
    except (ValueError, OSError) as exc:
        parser.error(str(exc))
    print(f"Local Samus BODY preview: {output}")
    print(f"Frame duration={meta['duration']}; body OAM parts={oam['count']}")
    print("Not a complete animation: arm cannon, effects and pose labels remain.")


if __name__ == "__main__":
    main()
