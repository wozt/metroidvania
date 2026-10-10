# SPDX-License-Identifier: GPL-3.0-only
"""Export special native poses only when cannon OAM is explicitly empty.

The body OAM is kept unchanged; unexpected cannon headers are rejected.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset
from scripts.mzm_samus_export_0150 import frame_metadata
from scripts.mzm_samus_sprite import (
    _make_power_suit_frame, aligned_canvas_bounds,
    SAMUS_ANIMATION_RECORD_BYTES, ARM_CANNON_ANIMATION_RECORD_BYTES,
)
from scripts.mzm_samus_compositions_0160 import parse_sized_symbols, safe_dir
from scripts.mzm_samus_special_0171 import CANDIDATES
from scripts.asset_layout import METROID_SAMUS_SPECIAL_SOURCE


def cannon_oam_header(rom, record_pointer):
    offset = rom_offset(record_pointer, rom)
    if offset + 8 > len(rom):
        raise ValueError("truncated cannon animation record")
    _, oam = struct.unpack_from("<II", rom, offset)
    pos = rom_offset(oam, rom)
    if pos + 2 > len(rom):
        raise ValueError("truncated cannon OAM header")
    return struct.unpack_from("<H", rom, pos)[0]


def compose(rom, symbols, output, selected):
    report = {"schema": "metroidvania-mzm-samus-special-body-only-v1",
              "warning": "body-only diagnostics, NOT fully verified game-accurate sprites",
              "sequences": {}}
    for name in selected:
        body, cannon, _, _ = CANDIDATES[name]
        missing = [symbol for symbol in (body, cannon) if symbol not in symbols]
        if missing:
            report["sequences"][name] = {"status": "missing-symbols", "symbols": missing}
            continue
        try:
            body_addr, body_size = symbols[body]
            cannon_addr, cannon_size = symbols[cannon]
            frames = frame_metadata(rom, body_addr, body_size)
            if not frames or cannon_size % 8 or cannon_size // 8 < len(frames):
                raise ValueError("cannon table shorter than body sequence")
            headers = [cannon_oam_header(rom, cannon_addr + i * 8)
                       for i in range(len(frames))]
            if any(header != 0 for header in headers):
                report["sequences"][name] = {
                    "status": "nonempty-or-special-cannon-oam",
                    "headers": [f"0x{header:04x}" for header in headers],
                }
                continue
            probes = []
            for index in range(len(frames)):
                _, meta = _make_power_suit_frame(
                    rom, body_addr + index * SAMUS_ANIMATION_RECORD_BYTES,
                    name, cannon_addr + index * ARM_CANNON_ANIMATION_RECORD_BYTES,
                    cannon_gfx=None)
                probes.append(meta)
            bounds = aligned_canvas_bounds(probes)
            rendered = []
            # Complete decoding before touching output so invalid sequences do not
            # leave partially written BMP collections.
            for index in range(len(frames)):
                bmp, meta = _make_power_suit_frame(
                    rom, body_addr + index * SAMUS_ANIMATION_RECORD_BYTES,
                    name, cannon_addr + index * ARM_CANNON_ANIMATION_RECORD_BYTES,
                    cannon_gfx=None, canvas_bounds=bounds)
                rendered.append((bmp, meta))
            rows = []
            folder = output / "composed" / name
            folder.mkdir(parents=True, exist_ok=True)
            for index, (bmp, meta) in enumerate(rendered):
                destination = folder / f"{index:03d}.bmp"
                if destination.is_symlink():
                    raise ValueError("symlink output frame refused")
                if not destination.exists() or destination.read_bytes() != bmp:
                    destination.write_bytes(bmp)
                rows.append({"index": index,
                             "bmp": destination.relative_to(output).as_posix(),
                             "duration_ticks": meta["frame"]["duration"]})
            report["sequences"][name] = {
                "status": "body-only-diagnostic-composed",
                "body_symbol": body, "cannon_symbol": cannon,
                "cannon_oam_headers": ["0x0000"] * len(frames),
                "canvas_bounds": list(bounds), "frames": rows,
            }
        except (ValueError, KeyError, IndexError, struct.error, OSError) as exc:
            report["sequences"][name] = {"status": "unsupported-native-frame",
                                          "reason": str(exc)}
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--elf", type=Path, default=Path("third_party/mzm/mzm_us.elf"))
    parser.add_argument("--output-dir", type=Path,
                        default=METROID_SAMUS_SPECIAL_SOURCE)
    parser.add_argument("--sequence", action="append", choices=sorted(CANDIDATES))
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    try:
        destination = safe_dir(root, args.output_dir)
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("original Zero Mission USA ROM SHA-1 mismatch")
        nm = subprocess.run(["arm-none-eabi-nm", "-S", "--defined-only", str(args.elf)],
                            text=True, capture_output=True, check=True)
        symbols = parse_sized_symbols(nm.stdout)
        report = compose(rom, symbols, destination, args.sequence or sorted(CANDIDATES))
        destination.mkdir(parents=True, exist_ok=True)
        manifest = destination / "manifest.json"
        if manifest.is_symlink():
            raise ValueError("symlink manifest refused")
        content = json.dumps(report, indent=2, sort_keys=True) + "\n"
        if not manifest.exists() or manifest.read_text(encoding="utf-8") != content:
            manifest.write_text(content, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    summary = {}
    for name, record in report["sequences"].items():
        status = record["status"]
        summary[status] = summary.get(status, 0) + 1
        print(f"{name}: {status} ({len(record.get('frames', []))} frames)")
        if "reason" in record:
            print(" ", record["reason"])
    print("Summary:", summary)
    print("Manifest:", manifest)


if __name__ == "__main__":
    main()
