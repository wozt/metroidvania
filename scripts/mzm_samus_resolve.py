# SPDX-License-Identifier: GPL-3.0-only
"""Resolve MZM Samus animation symbols using explicit local symbol addresses.

Do not guess addresses: unresolved references are reported, not exported.
A matching ROM checksum is required before interpreting pointer records.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from scripts.mzm_samus_frame import EXPECTED_SHA1, rom_offset, parse_frame
from scripts.asset_layout import METROID_SAMUS_METADATA

SYMBOL = re.compile(r"^sSamusAnim_[A-Za-z0-9_]+$")
MAP_STYLE = re.compile(r"^\s*(0x[0-9a-fA-F]{8}|[0-9a-fA-F]{8})\s+"
                       r"(sSamusAnim_[A-Za-z0-9_]+)\s*$")
NM_STYLE = re.compile(r"^\s*([0-9a-fA-F]{8})\s+[a-zA-Z]\s+"
                      r"(sSamusAnim_[A-Za-z0-9_]+)\s*$")


def parse_symbols(text):
    mapping = {}
    for line in text.splitlines():
        match = MAP_STYLE.match(line) or NM_STYLE.match(line)
        if not match:
            continue
        address = int(match.group(1), 16)
        name = match.group(2)
        if not SYMBOL.fullmatch(name) or not 0x08000000 <= address < 0x0E000000:
            raise ValueError("invalid symbol address: " + name)
        if name in mapping and mapping[name] != address:
            raise ValueError("conflicting addresses for " + name)
        mapping[name] = address
    return mapping


def resolve(catalog, mapping, rom):
    if catalog.get("schema") != "metroidvania-mzm-samus-symbolic-catalog-v1":
        raise ValueError("unexpected catalogue schema")
    if not isinstance(catalog.get("tables"), dict):
        raise ValueError("catalogue tables missing")
    unique = set()
    for rows in catalog["tables"].values():
        for row in rows:
            unique.update(row.get("all_symbols", []))
    if not unique:
        raise ValueError("catalogue has no symbols")
    result = {"schema": "metroidvania-mzm-samus-addresses-v1",
              "rom_sha1": EXPECTED_SHA1, "entries": {}, "unresolved": []}
    for name in sorted(unique):
        if not isinstance(name, str) or not SYMBOL.fullmatch(name):
            raise ValueError("invalid catalogue symbol")
        if name not in mapping:
            result["unresolved"].append(name)
            continue
        address = mapping[name]
        # Pointing to a valid record does NOT establish animation length.
        try:
            off = rom_offset(address, rom)
            if off + 16 > len(rom):
                raise ValueError("truncated animation record")
            # Symbol is a frame-array start, not necessarily a pointer to one.
            # Validate that the first frame is structurally readable.
            upper, lower, oam, duration = parse_frame(rom, address)
            for ptr in (upper, lower):
                rom_offset(ptr, rom)
            result["entries"][name] = {
                "address": f"0x{address:08x}",
                "first_frame": {
                    "upper": f"0x{upper:08x}",
                    "lower": f"0x{lower:08x}",
                    "oam": f"0x{oam:08x}",
                    "duration_ticks": duration
                },
                "frame_count": None,
                "status": "first-frame-validated; frame count and graphics unresolved"
            }
        except ValueError as exc:
            # Do not silently accept a candidate address just because it
            # resembles a GBA pointer.
            raise ValueError(f"{name} at 0x{address:08x}: {exc}") from exc
    result["unique_symbols"] = len(unique)
    result["resolved_symbols"] = len(result["entries"])
    result["unresolved_symbols"] = len(result["unresolved"])
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path,
                        default=METROID_SAMUS_METADATA / "catalog.json")
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--symbols", required=True, type=Path,
                        help="local GBA address map: ADDRESS SYMBOL or nm SYMBOL lines")
    parser.add_argument("--output", type=Path,
                        default=METROID_SAMUS_METADATA / "addresses.json")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    allowed = (root / "assets/extracted").resolve()
    dest = args.output.absolute()
    if dest.suffix != ".json" or allowed not in dest.parents:
        parser.error("output must be JSON inside assets/extracted")
    if any(path.is_symlink() for path in (dest, *dest.parents)
           if path == allowed or allowed in path.parents):
        parser.error("symlink output refused")
    try:
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("ROM SHA-1 mismatch: expected original Zero Mission USA")
        catalog = json.loads(args.catalog.read_text(encoding="utf-8"))
        symbols = parse_symbols(args.symbols.read_text(encoding="utf-8"))
        if not symbols:
            raise ValueError("symbol map has no Samus animation names")
        report = resolve(catalog, symbols, rom)
        dest.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps(report, sort_keys=True, indent=2) + "\n"
        if not dest.exists() or dest.read_text(encoding="utf-8") != data:
            dest.write_text(data, encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    print(f"Resolved {report['resolved_symbols']}/{report['unique_symbols']} unique symbols; "
          f"{report['unresolved_symbols']} unresolved.")
    print("No graphics exported. Frame counts and secondary frames not guessed.")
    print("Wrote", dest)


if __name__ == "__main__":
    main()
