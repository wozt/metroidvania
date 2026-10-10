#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the canonical deduplicated private Samus runtime library.

The current discovery adapter reads validated manifests produced by the native
MZM decoders. Output names are stable and contain no patch sequence numbers.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.asset_layout import (
    EXTRACTED,
    METROID_SAMUS_BODY_SOURCE,
    METROID_SAMUS_COMPOSED_SOURCE,
    METROID_SAMUS_METADATA,
    METROID_SAMUS_RUNTIME,
    METROID_SAMUS_SPECIAL_SOURCE,
    private_path,
)
from scripts.mzm_samus_catalog import collect as collect_symbolic_catalog
from scripts.mzm_samus_resolve import parse_symbols, resolve
from scripts.mzm_samus_export_0150 import (
    export as export_bodies,
    parse_nm_sizes,
    resolve_suit_palettes,
)
from scripts.mzm_samus_bulk_0170 import CANDIDATES as COMPOSED_CANDIDATES
from scripts.mzm_samus_bulk_0170 import collect as collect_compositions
from scripts.mzm_samus_compositions_0160 import parse_sized_symbols
from scripts.mzm_samus_frame import EXPECTED_SHA1
from scripts.mzm_samus_runtime_library_0175 import build
from scripts.mzm_samus_special_0171 import CANDIDATES as SPECIAL_CANDIDATES
from scripts.mzm_samus_special_body_0173 import compose as compose_special

SUITS = ("PowerSuit", "VariaSuit", "GravitySuit", "FullSuit", "Suitless")
SOURCE_SUIT = {
    "PowerSuit": "PowerSuit",
    "VariaSuit": "PowerSuit",
    "GravitySuit": "PowerSuit",
    "FullSuit": "FullSuit",
    "Suitless": "Suitless",
}
AIM_ACTIONS = {
    "idle": ("standing", "loop"),
    "run": ("running", "loop"),
    "midair": ("midair", "loop"),
    "crouch": ("crouching", "loop"),
    "fire": ("shooting", "loop"),
    "crouch_fire": ("shootingandcrouching", "loop"),
    "turn": ("turningaround", "once"),
    "turn_midair": ("turningaroundmidair", "once"),
    "turn_crouch": ("turningaroundandcrouching", "once"),
    "landing": ("landing", "once"),
}
SIMPLE_ACTIONS = {
    "skid": ("skidding", "once"),
    "spin_start": ("startingspinjump", "once"),
    "spin": ("spinning", "loop"),
    "space_jump": ("spacejumping", "loop"),
    "screw_attack": ("screwattacking", "loop"),
    "wall_jump": ("startingwalljump", "once"),
    "morph_start": ("morphing", "once"),
    "morph_ball": ("morphball", "loop"),
    "unmorph": ("unmorphing", "once"),
    "ledge_hang": ("hangingonledge", "loop"),
    "ledge_pull_forward": ("pullingyourselfforwardfromhanging", "once"),
    "ledge_pull_up": ("pullingyourselfupfromhanging", "once"),
    "hurt": ("gettingknockedback", "once"),
    "death": ("dying", "once"),
    "shinespark_charge": ("delaybeforeshinesparking", "once"),
    "shinespark": ("shinesparking", "loop"),
    "shinespark_end": ("delayaftershinesparking", "once"),
    "shinespark_side": ("sidewards_shinesparking", "loop"),
    "ball_spark": ("ballsparking", "loop"),
}


def safe_asset_path(root: Path, relative: str) -> Path:
    if not isinstance(relative, str) or not relative.startswith(
            EXTRACTED.as_posix() + "/"):
        raise ValueError("asset path is not private")
    path = root / relative
    base = root / EXTRACTED
    if not path.resolve().is_relative_to(base.resolve()):
        raise ValueError("asset escapes private root")
    current = root
    for part in Path(relative).parts:
        current /= part
        if current.is_symlink():
            raise ValueError("symlink source refused: " + str(current))
    if not path.is_file():
        raise ValueError("missing source: " + str(path))
    return path


def _write_atomic(path: Path, content: str) -> None:
    if path.is_symlink():
        raise ValueError("symlink output refused")
    temporary = path.with_suffix(path.suffix + ".tmp")
    if temporary.is_symlink():
        raise ValueError("symlink temporary output refused")
    temporary.write_text(content, encoding="utf-8")
    os.replace(temporary, path)


def _write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    _write_atomic(path, json.dumps(value, indent=2, sort_keys=True) + "\n")


def _run_nm(nm: str, elf: Path, *, sizes: bool) -> str:
    command = [nm]
    if sizes:
        command.append("-S")
    command.extend(("--defined-only", str(elf)))
    return subprocess.run(command, check=True, capture_output=True,
                          text=True).stdout


def prepare_sources(root: Path, rom_path: Path, elf: Path,
                    decomp: Path, nm: str = "arm-none-eabi-nm") -> dict:
    """Rebuild every source manifest consumed by the canonical library."""
    root = Path(root).resolve()
    rom_path = Path(rom_path).resolve()
    elf = Path(elf).resolve()
    decomp = Path(decomp).resolve()
    rom = rom_path.read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError("original Zero Mission USA ROM SHA-1 mismatch")
    if elf.is_symlink() or not elf.is_file():
        raise ValueError("missing or symlink MZM reference ELF")

    pointers = decomp / "src/data/samus/samus_animation_pointers.c"
    constants = decomp / "include/constants/samus.h"
    symbolic = collect_symbolic_catalog(
        pointers.read_text(encoding="utf-8"),
        constants.read_text(encoding="utf-8"))
    metadata = private_path(root, METROID_SAMUS_METADATA, create=True)
    metadata.mkdir(exist_ok=True)
    _write_json(metadata / "catalog.json", symbolic)

    nm_plain = _run_nm(nm, elf, sizes=False)
    addresses = resolve(symbolic, parse_symbols(nm_plain), rom)
    _write_json(metadata / "addresses.json", addresses)

    nm_sized = _run_nm(nm, elf, sizes=True)
    body_output = private_path(root, METROID_SAMUS_BODY_SOURCE, create=True)
    body_output.mkdir(exist_ok=True)
    body = export_bodies(
        rom, addresses, parse_nm_sizes(nm_sized), body_output,
        palette_offsets=resolve_suit_palettes(nm_plain, rom))
    _write_json(body_output / "manifest.json", body)

    symbols = parse_sized_symbols(nm_sized)
    composed_output = private_path(root, METROID_SAMUS_COMPOSED_SOURCE, create=True)
    composed_output.mkdir(exist_ok=True)
    composed = collect_compositions(
        rom, symbols, composed_output, list(COMPOSED_CANDIDATES))
    _write_json(composed_output / "manifest.json", composed)

    special_output = private_path(root, METROID_SAMUS_SPECIAL_SOURCE, create=True)
    special_output.mkdir(exist_ok=True)
    special = compose_special(
        rom, symbols, special_output, sorted(SPECIAL_CANDIDATES))
    _write_json(special_output / "manifest.json", special)
    return {
        "symbolic_variants": symbolic["variant_count"],
        "resolved_symbols": addresses["resolved_symbols"],
        "body_animations": sum(
            item.get("body_status") == "exported-body-only"
            for item in body["animations"].values()),
        "composed_sequences": sum(
            item.get("status") == "diagnostic-composed"
            for item in composed["sequences"].values()),
        "special_sequences": sum(
            item.get("status") == "body-only-diagnostic-composed"
            for item in special["sequences"].values()),
    }


def _animation_candidates(suit: str, side: str, native: str,
                          aim: str | None) -> list[str]:
    source_suit = SOURCE_SUIT[suit]
    values = []
    if aim is not None:
        values.append(f"{source_suit}/{native}_{aim}_{side}")
        direction = "" if aim == "forward" else aim + "_"
        values.append(f"{source_suit}/{side}_{direction}{native}")
        if aim == "forward":
            values.append(f"{source_suit}/{side}_forward_{native}")
    else:
        values.extend((f"{source_suit}/{native}_{side}",
                       f"{source_suit}/{side}_{native}"))
    return list(dict.fromkeys(values))


def build_animation_map(catalogue: dict) -> tuple[str, dict]:
    """Resolve gameplay semantics to exact catalogue keys without guessing."""
    available = set(catalogue["sequences"])
    rows = []
    missing = []
    for suit in SUITS:
        for side in ("left", "right"):
            for action, (native, mode) in AIM_ACTIONS.items():
                for aim in ("forward", "diagonalup", "diagonaldown"):
                    candidates = _animation_candidates(suit, side, native, aim)
                    key = next((value for value in candidates if value in available), None)
                    if key:
                        rows.append((action, suit, side, aim, mode, key))
                    else:
                        missing.append(f"{action}/{suit}/{side}/{aim}")
            for action, (native, mode) in SIMPLE_ACTIONS.items():
                candidates = _animation_candidates(suit, side, native, None)
                key = next((value for value in candidates if value in available), None)
                if key:
                    rows.append((action, suit, side, "none", mode, key))
                else:
                    missing.append(f"{action}/{suit}/{side}/none")
    content = "schema\tmetroidvania-samus-animation-map-v1\n"
    content += "action\tsuit\tfacing\taim\tmode\tkey\n"
    content += "".join("\t".join(row) + "\n" for row in rows)
    report = {
        "schema": "metroidvania-samus-animation-map-report-v1",
        "rows": len(rows),
        "actions": len(AIM_ACTIONS) + len(SIMPLE_ACTIONS),
        "missing": missing,
        "requested_suit_sources": SOURCE_SUIT,
    }
    return content, report


def produce(root: Path, preparation: dict | None = None) -> dict:
    root = Path(root).resolve()
    destination = private_path(root, METROID_SAMUS_RUNTIME, create=True)
    if destination.is_symlink():
        raise ValueError("private Samus runtime cannot be a symlink")
    catalogue = build(root, strict=True)
    destination.mkdir(exist_ok=True)
    objects = destination / "objects"
    if objects.is_symlink():
        raise ValueError("private Samus object store cannot be a symlink")
    objects.mkdir(exist_ok=True)
    rows = []
    packed: dict[str, int] = {}
    for key, entry in sorted(catalogue["sequences"].items()):
        if len(key) >= 160 or any(character in key for character in "\t\r\n"):
            raise ValueError("invalid animation key")
        for frame in entry["frames"]:
            source = safe_asset_path(root, frame["bmp"])
            blob = source.read_bytes()
            if blob[:2] != b"BM":
                raise ValueError("invalid BMP header: " + str(source))
            digest = hashlib.sha256(blob).hexdigest()
            output = objects / f"{digest}.bmp"
            if output.is_symlink():
                raise ValueError("symlink asset destination")
            if output.exists():
                if output.read_bytes() != blob:
                    raise ValueError("object hash collision")
            else:
                temporary = output.with_suffix(".tmp")
                if temporary.is_symlink():
                    raise ValueError("symlink temporary output")
                temporary.write_bytes(blob)
                os.replace(temporary, output)
            packed[digest] = len(blob)
            relative = output.relative_to(root).as_posix()
            if len(relative) >= 320:
                raise ValueError("runtime BMP path too long")
            rows.append(
                f"{key}\t{frame['index']}\t{frame['duration_ticks']}\t{relative}")
    metadata = {
        "schema": "metroidvania-mzm-samus-runtime-v2",
        "source_schema": catalogue["schema"],
        "suits": catalogue["suits"],
        "sequences": len(catalogue["sequences"]),
        "unique_bmps": len(packed),
        "sources": catalogue["sources"],
        "object_store": "sha256",
        "note": "Private native extraction; source ROM data is never redistributed.",
    }
    if preparation is not None:
        metadata["preparation"] = preparation
    animation_map, map_report = build_animation_map(catalogue)
    metadata["animation_map"] = map_report
    expected = (
        (destination / "runtime_index.tsv", "\n".join(rows) + "\n"),
        (destination / "animation_map.tsv", animation_map),
        (destination / "manifest.json",
         json.dumps(metadata, indent=2, sort_keys=True) + "\n"),
    )
    for path, content in expected:
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            _write_atomic(path, content)
    return metadata


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=ROOT)
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "roms/Metroid - Zero Mission (USA).gba")
    parser.add_argument("--elf", type=Path,
                        default=ROOT / "third_party/mzm/mzm_us.elf")
    parser.add_argument("--decomp", type=Path,
                        default=ROOT / "third_party/mzm")
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--bundle-only", action="store_true",
                        help="reuse prepared sources and only rebuild runtime output")
    args = parser.parse_args(argv)
    try:
        preparation = None if args.bundle_only else prepare_sources(
            args.root, args.rom, args.elf, args.decomp, args.nm)
        result = produce(args.root, preparation)
    except (OSError, ValueError, KeyError, TypeError,
            subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    print("Samus runtime library:", result["sequences"], "sequences,",
          result["unique_bmps"], "unique BMPs")
    print("Index:", args.root.resolve() / METROID_SAMUS_RUNTIME / "runtime_index.tsv")
    print("State map:", args.root.resolve() / METROID_SAMUS_RUNTIME / "animation_map.tsv")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
