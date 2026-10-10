# SPDX-License-Identifier: GPL-3.0-only
"""Inventory symbolic MZM Samus animation tables from a local decomp checkout.

Symbol names alone do not establish image availability or implemented gameplay.
"""
import argparse
import json
from pathlib import Path
import re

TABLE = re.compile(
    r"const\s+struct\s+SamusAnimationData\s*\*\s*const\s+"
    r"(sSamusAnimPointers_[A-Za-z0-9_]+)\s*(?:\[[^\]]*\])+\s*=\s*\{")
ENTRY = re.compile(r"\[\s*([A-Z][A-Z0-9_]*)\s*\]\s*=\s*\{\s*"
                   r"([^{}]*?)\}", re.S)
SYMBOL = re.compile(r"\bsSamusAnim_[A-Za-z0-9_]+\b")
POSE = re.compile(r"^\s*(SPOSE_[A-Z0-9_]+)\s*(?:,|=)")


def matching_brace(source, start):
    depth = 0
    for i in range(start, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    raise ValueError("unclosed animation pointer table")


def poses_from_header(content):
    match = re.search(r"MAKE_ENUM\(u8,\s*SamusPose\)\s*\{(.*?)\};",
                      content, re.S)
    if not match:
        raise ValueError("SamusPose enum missing")
    block = match.group(1).split("SPOSE_COUNT")[0]
    result = [match.group(1) for line in block.splitlines()
              if (match := POSE.match(line))]
    if not result or len(result) != len(set(result)):
        raise ValueError("invalid/duplicate SamusPose")
    return result


def collect(pointers, constants):
    poses = poses_from_header(constants)
    tables = {}
    for match in TABLE.finditer(pointers):
        name = match.group(1)
        start = pointers.index("{", match.start())
        end = matching_brace(pointers, start)
        body = pointers[start + 1:end]
        rows = []
        for entry in ENTRY.finditer(body):
            symbols = SYMBOL.findall(entry.group(2))
            if symbols:
                rows.append({"selector": entry.group(1),
                             "right": symbols[0],
                             "left": symbols[1] if len(symbols) > 1 else None,
                             "all_symbols": symbols})
        if rows:
            if name in tables:
                raise ValueError("duplicate table: " + name)
            tables[name] = rows
    if not tables:
        raise ValueError("no Samus animation pointer tables")
    return {
        "schema": "metroidvania-mzm-samus-symbolic-catalog-v1",
        "provenance": "metroidret/mzm local checkout; symbolic references only",
        "poses": poses,
        "pose_count": len(poses),
        "table_count": len(tables),
        "variant_count": sum(len(rows) for rows in tables.values()),
        "tables": tables,
        "unresolved": {
            "rom_addresses": "not resolved by symbolic table inventory",
            "frames_and_durations": "not decoded by this command",
            "graphics": "not exported by this command",
            "runtime_support": "not inferred from this catalogue"
        }
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decomp", required=True, type=Path,
                        help="local checkout of metroidret/mzm")
    parser.add_argument("--output", required=True, type=Path,
                        help="private JSON path under assets/extracted")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    allowed = (root / "assets" / "extracted").resolve()
    output = args.output.absolute()
    if output.suffix != ".json" or allowed not in output.parents:
        parser.error("output must be JSON under assets/extracted")
    if any(p.is_symlink() for p in (output, *output.parents)
           if p == allowed or allowed in p.parents):
        parser.error("symlink output refused")
    src = args.decomp.resolve()
    try:
        catalog = collect(
            (src / "src/data/samus/samus_animation_pointers.c").read_text(encoding="utf-8"),
            (src / "include/constants/samus.h").read_text(encoding="utf-8"))
        output.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps(catalog, indent=2, sort_keys=True) + "\n"
        if not output.exists() or output.read_text(encoding="utf-8") != data:
            output.write_text(data, encoding="utf-8")
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Indexed {catalog['pose_count']} poses, {catalog['table_count']} tables, "
          f"{catalog['variant_count']} symbolic selector entries.")
    print("No ROM or graphics data was exported.")
    print("Wrote", output)


if __name__ == "__main__":
    main()
