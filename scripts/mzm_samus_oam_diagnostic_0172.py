# SPDX-License-Identifier: GPL-3.0-only
"""Diagnose native body versus cannon OAM headers for special animations."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset, parse_frame
from scripts.mzm_samus_export_0150 import frame_metadata
from scripts.mzm_samus_compositions_0160 import parse_sized_symbols, safe_dir
from scripts.mzm_samus_special_0171 import CANDIDATES
from scripts.asset_layout import METROID_SAMUS_DIAGNOSTICS


def header(rom, pointer):
    off = rom_offset(pointer, rom)
    if off + 2 > len(rom):
        raise ValueError("truncated OAM header")
    value = struct.unpack_from("<H", rom, off)[0]
    count = value & 0xfff
    return {"pointer": f"0x{pointer:08x}", "header": f"0x{value:04x}",
            "parts": count, "empty": count == 0, "plausible": 1 <= count <= 128}


def inspect(rom, symbols, selected):
    report = {"schema": "metroidvania-mzm-special-oam-diagnostic-v1", "sequences": {}}
    for name in selected:
        body, cannon, upper, lower = CANDIDATES[name]
        missing = [s for s in (body, cannon, upper, lower) if s not in symbols]
        if missing:
            report["sequences"][name] = {"status": "missing-symbols", "symbols": missing}
            continue
        item = {"body": body, "cannon": cannon, "frames": []}
        report["sequences"][name] = item
        try:
            baddr, bsize = symbols[body]
            caddr, csize = symbols[cannon]
            frames = frame_metadata(rom, baddr, bsize)
            if csize % 8 or csize // 8 < len(frames):
                raise ValueError("cannon array shorter than playable body animation")
            for index in range(len(frames)):
                bp = baddr + 16 * index
                cp = caddr + 8 * index
                body_oam = parse_frame(rom, bp)[2]
                off = rom_offset(cp, rom)
                if off + 8 > len(rom):
                    raise ValueError("truncated cannon record")
                cannon_oam = struct.unpack_from("<II", rom, off)[1]
                item["frames"].append({
                    "index": index,
                    "body_oam": header(rom, body_oam),
                    "cannon_oam": header(rom, cannon_oam),
                })
            bad_body = sum(not f["body_oam"]["plausible"] for f in item["frames"])
            bad_cannon = sum(not f["cannon_oam"]["plausible"] for f in item["frames"])
            item["status"] = ("requires-special-body-decoder" if bad_body else
                              "empty-or-special-cannon-oam" if bad_cannon else
                              "oam-headers-plausible")
            item["invalid_body_frames"] = bad_body
            item["invalid_cannon_frames"] = bad_cannon
        except (ValueError, KeyError, struct.error) as exc:
            item["status"] = "diagnostic-error"
            item["reason"] = str(exc)
    return report


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", required=True, type=Path)
    p.add_argument("--elf", type=Path, default=Path("third_party/mzm/mzm_us.elf"))
    p.add_argument("--output-dir", type=Path,
                   default=METROID_SAMUS_DIAGNOSTICS / "special/oam")
    args = p.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    try:
        out = safe_dir(root, args.output_dir)
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("ROM SHA-1 mismatch")
        result = subprocess.run(["arm-none-eabi-nm", "-S", "--defined-only", str(args.elf)],
                                capture_output=True, text=True, check=True)
        sizes = parse_sized_symbols(result.stdout)
        report = inspect(rom, sizes, sorted(CANDIDATES))
        out.mkdir(parents=True, exist_ok=True)
        path = out / "oam_diagnostic.json"
        if path.is_symlink():
            raise ValueError("symlink report refused")
        text = json.dumps(report, indent=2, sort_keys=True) + "\n"
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            path.write_text(text, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        p.error(str(exc))
    summary = {}
    for entry in report["sequences"].values():
        s = entry["status"]
        summary[s] = summary.get(s, 0) + 1
    print("OAM diagnostic summary:", summary)
    for name, entry in report["sequences"].items():
        if entry["status"] not in ("missing-symbols", "oam-headers-plausible"):
            print(name, entry["status"],
                  "invalid body:", entry.get("invalid_body_frames", "?"),
                  "invalid cannon:", entry.get("invalid_cannon_frames", "?"))
    print("Report:", path)


if __name__ == "__main__":
    main()
