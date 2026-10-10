# SPDX-License-Identifier: GPL-3.0-only
"""Extract native Samus animation BODY frames using exact ELF symbol sizes.

Images are private, ROM-derived, and must never be committed. Arm cannon,
effects, and suit-specific palettes remain explicitly separate concerns.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset, stage
from scripts.mzm_samus_oam_decode import decode_raw_samus_oam
from scripts.mzm_samus_body import load_palette_banks
from scripts.gba_oam import compose, to_bmp
from scripts.asset_layout import METROID_SAMUS_BODY_SOURCE, METROID_SAMUS_METADATA

SYMBOL_RE = re.compile(r"^sSamusAnim_[A-Za-z0-9_]+$")
NM_LINE = re.compile(r"^([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s+([a-zA-Z])\s+(sSamusAnim_[A-Za-z0-9_]+)$")
RECORD_BYTES = 16
MAX_FRAMES = 256
DEFAULT_PALETTE_OFFSET = 0x2376A8


def parse_nm_sizes(output):
    result = {}
    for line in output.splitlines():
        match = NM_LINE.fullmatch(line.strip())
        if not match:
            continue
        address, size, kind, name = match.groups()
        address, size = int(address, 16), int(size, 16)
        if kind.lower() not in ("r", "d") or not 0x08000000 <= address < 0x0E000000:
            continue
        if not size or size % RECORD_BYTES or size // RECORD_BYTES > MAX_FRAMES:
            continue
        if name in result and result[name] != (address, size):
            raise ValueError("conflicting ELF symbol " + name)
        result[name] = (address, size)
    return result


def frame_metadata(rom, address, size):
    if size % RECORD_BYTES or not 1 <= size // RECORD_BYTES <= MAX_FRAMES:
        raise ValueError("invalid animation symbol size")
    off = rom_offset(address, rom)
    if off + size > len(rom):
        raise ValueError("animation outside ROM")
    frames = []
    for index in range(size // RECORD_BYTES):
        frame_pointer = address + index * RECORD_BYTES
        raw = rom[off + index * RECORD_BYTES:off + (index + 1) * RECORD_BYTES]
        # Native frame sequences can end before the ELF symbol boundary.
        # Do not interpret the following data as more animation frames.
        if raw == bytes(RECORD_BYTES):
            if not frames:
                raise ValueError("animation begins with empty frame")
            break
        _, meta = stage(rom, frame_pointer)
        if not 1 <= meta["duration"] <= 255:
            raise ValueError("invalid frame duration")
        frames.append(meta)
    return frames


def body_image(rom, frame_pointer, palette):
    vram, metadata = stage(rom, frame_pointer)
    oam = decode_raw_samus_oam(rom, metadata["oam_pointer"])
    entries = oam["entries"]
    if not entries:
        raise ValueError("frame has no OAM sprites")
    if {e["bank"] for e in entries} - {0, 1}:
        raise ValueError("OAM uses unsupported palette bank")
    left = min(e["x"] for e in entries)
    top = min(e["y"] for e in entries)
    right = max(e["x"] + e["w"] for e in entries)
    bottom = max(e["y"] + e["h"] for e in entries)
    if not 1 <= right - left <= 512 or not 1 <= bottom - top <= 512:
        raise ValueError("invalid OAM canvas")
    rgba = compose(vram, palette, entries, left, top, right-left, bottom-top, "2d")
    return to_bmp(rgba, right-left, bottom-top), {
        "oam_bounds": [left, top, right, bottom],
        "width": right-left, "height": bottom-top,
        "body_oam_parts": len(entries)
    }



PALETTE_SYMBOLS = {
    "PowerSuit": "sSamusPal_PowerSuit_Default",
    "VariaSuit": "sSamusPal_VariaSuit_Default",
    "FullSuit": "sSamusPal_FullSuit_Default",
    "GravitySuit": "sSamusPal_GravitySuit_Default",
    "Suitless": "sSamusPal_Suitless_Default",
}
# Verified against the SHA-1 matching USA MZM rebuild. Fail closed if
# the ELF symbol layout differs; do not infer palettes from nearby data.
PALETTE_EXPECTED_ADDRESSES = {
    "PowerSuit": 0x082376A8,
    "VariaSuit": 0x08237BE8,
    "FullSuit": 0x08237FA8,
    "GravitySuit": 0x082383C8,
    "Suitless": 0x082387E8,
}
PALETTE_NM = re.compile(r"^([0-9a-fA-F]{8})\s+[A-Za-z]\s+(sSamusPal_[A-Za-z0-9_]+)$")


def resolve_suit_palettes(nm_output, rom):
    found = {}
    for line in nm_output.splitlines():
        match = PALETTE_NM.fullmatch(line.strip())
        if match is None:
            continue
        address, symbol = match.groups()
        if symbol in PALETTE_SYMBOLS.values():
            address = int(address, 16)
            if symbol in found and found[symbol] != address:
                raise ValueError("conflicting palette symbol: " + symbol)
            found[symbol] = address
    offsets = {}
    for suit, symbol in PALETTE_SYMBOLS.items():
        address = found.get(symbol)
        if address is None:
            raise ValueError("missing palette symbol: " + symbol)
        if address != PALETTE_EXPECTED_ADDRESSES[suit]:
            raise ValueError("unexpected ROM location of " + symbol)
        offset = rom_offset(address, rom)
        load_palette_banks(rom, offset, 2, 0)
        offsets[suit] = offset
    return offsets

def suit_group(name):
    for label in ("PowerSuit", "VariaSuit", "FullSuit", "GravitySuit", "Suitless"):
        if name.startswith("sSamusAnim_" + label + "_"):
            return label
    return None


def export(rom, addresses, sizes, output, limit=0, palette_offsets=None):
    palette_offsets = dict(palette_offsets or {"PowerSuit": DEFAULT_PALETTE_OFFSET})
    result = {"schema": "metroidvania-mzm-samus-body-export-v1",
              "rom_sha1": EXPECTED_SHA1, "animations": {},
              "unresolved": {}}
    entries = addresses["entries"]
    for index, (name, info) in enumerate(sorted(entries.items())):
        if limit and index >= limit:
            break
        if not SYMBOL_RE.fullmatch(name):
            raise ValueError("invalid symbol name")
        expected_address = int(info["address"], 16)
        if name not in sizes:
            result["unresolved"][name] = "missing exact ELF symbol size"
            continue
        address, size = sizes[name]
        if address != expected_address:
            raise ValueError("ELF/ROM address mismatch: " + name)
        try:
            frames = frame_metadata(rom, address, size)
        except ValueError as exc:
            result["unresolved"][name] = str(exc)
            continue
        record = {"address": info["address"], "frames": [],
                  "body_status": "not-exported",
                  "arm_cannon_status": "not-extracted",
                  "effects_status": "not-extracted"}
        suit = suit_group(name)
        palette_offset = palette_offsets.get(suit)
        palette = (load_palette_banks(rom, palette_offset, 2, 0)
                   if palette_offset is not None else None)
        record["palette_source"] = (f"explicit-rom-offset:0x{palette_offset:08x}"
                                    if palette_offset is not None else "unverified")
        images = []
        if palette is not None:
            try:
                for fr in frames:
                    images.append(body_image(rom, fr["frame_pointer"], palette))
            except ValueError as exc:
                images = []
                record["body_status"] = "unsupported: " + str(exc)
        else:
            record["body_status"] = "unsupported: suit palette not verified"
        for i, fr in enumerate(frames):
            row = {"index": i, "duration_ticks": fr["duration"],
                   "frame_pointer": f"0x{fr['frame_pointer']:08x}",
                   "oam_pointer": f"0x{fr['oam_pointer']:08x}"}
            if images:
                bmp, metadata = images[i]
                relative = Path("body") / name / f"{i:03d}.bmp"
                dest = output / relative
                dest.parent.mkdir(parents=True, exist_ok=True)
                if not dest.exists() or dest.read_bytes() != bmp:
                    dest.write_bytes(bmp)
                row.update(metadata)
                row["body_bmp"] = relative.as_posix()
            record["frames"].append(row)
        if images:
            record["body_status"] = "exported-body-only"
        result["animations"][name] = record
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--elf", type=Path, default=Path("third_party/mzm/mzm_us.elf"))
    parser.add_argument("--addresses", type=Path,
                        default=METROID_SAMUS_METADATA / "addresses.json")
    parser.add_argument("--output-dir", type=Path,
                        default=METROID_SAMUS_BODY_SOURCE)
    parser.add_argument("--fullsuit-palette-offset", type=lambda s: int(s, 0),
                        help="verified ROM offset of 2-bank FullSuit OBJ palette")
    parser.add_argument("--suitless-palette-offset", type=lambda s: int(s, 0),
                        help="verified ROM offset of 2-bank Suitless OBJ palette")
    parser.add_argument("--limit", type=int, default=0,
                        help="initial smoke test; 0 processes all animations")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    output = args.output_dir.absolute()
    allowed = (root / "assets/extracted").resolve()
    if allowed not in output.parents or args.limit < 0:
        parser.error("output must be below assets/extracted; limit must be >= 0")
    if any(p.is_symlink() for p in (output, *output.parents)
           if p == allowed or allowed in p.parents):
        parser.error("symlink output refused")
    try:
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("unexpected ROM SHA-1")
        addresses = json.loads(args.addresses.read_text(encoding="utf-8"))
        if addresses.get("schema") != "metroidvania-mzm-samus-addresses-v1":
            raise ValueError("unexpected address catalogue schema")
        nm = subprocess.run(["arm-none-eabi-nm", "-S", "--defined-only", str(args.elf)],
                            capture_output=True, text=True, check=True)
        sizes = parse_nm_sizes(nm.stdout)
        if not sizes:
            raise ValueError("ELF contains no sized Samus animation symbols")
        palette_nm = subprocess.run(
            ["arm-none-eabi-nm", "--defined-only", str(args.elf)],
            capture_output=True, text=True, check=True)
        palette_offsets = resolve_suit_palettes(palette_nm.stdout, rom)
        for suit, offset in (("FullSuit", args.fullsuit_palette_offset),
                             ("Suitless", args.suitless_palette_offset)):
            if offset is not None and offset != palette_offsets[suit]:
                raise ValueError("explicit palette differs from verified ELF palette: " + suit)
        result = export(rom, addresses, sizes, output, args.limit, palette_offsets)
        output.mkdir(parents=True, exist_ok=True)
        dest = output / "manifest.json"
        content = json.dumps(result, indent=2, sort_keys=True) + "\n"
        if not dest.exists() or dest.read_text(encoding="utf-8") != content:
            dest.write_text(content, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    exported = sum(x["body_status"] == "exported-body-only"
                   for x in result["animations"].values())
    print(f"Animations indexed: {len(result['animations'])}; body exported: {exported}; "
          f"unresolved frame records: {len(result['unresolved'])}.")
    print("Suit palettes resolved from matching ELF; arm cannon and effects NOT exported.")
    print("Manifest:", dest)


if __name__ == "__main__":
    main()
