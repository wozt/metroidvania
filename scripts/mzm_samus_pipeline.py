#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the canonical deduplicated private Samus runtime library.

One command verifies the ROM, reads the native Samus pointer tables from the
pinned decompilation and the matching reference ELF, composes every body,
arm cannon and suit palette combination with ``scripts.mzm_samus_compose``,
stores unique BMPs by SHA-256 and emits the runtime index and the semantic
animation registry. Output names are stable and contain no patch numbers.
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
from scripts.asset_layout import METROID_SAMUS_RUNTIME, private_path
from scripts.mzm_samus_compose import (
    VISUAL_SUITS,
    compose_all,
    parse_symbols,
    parse_tables,
)
from scripts.mzm_samus_frame import EXPECTED_SHA1

INDEX_SCHEMA = "metroidvania-samus-runtime-index-v3"
MAP_SCHEMA = "metroidvania-samus-animation-map-v1"
SUITS = tuple(VISUAL_SUITS)
AIMS = {
    "forward": "ACD_FORWARD",
    "diagonalup": "ACD_DIAGONALLY_UP",
    "diagonaldown": "ACD_DIAGONALLY_DOWN",
    "up": "ACD_UP",
    "down": "ACD_DOWN",
}
# Semantic action -> native per-ACD table used by SamusUpdateGraphicsOam.
AIM_ACTIONS = {
    "idle": ("Standing", "loop"),
    "run": ("Running", "loop"),
    "midair": ("MidAir", "loop"),
    "crouch": ("Crouching", "loop"),
    "fire": ("Shooting", "once"),
    "crouch_fire": ("ShootingAndCrouching", "once"),
    "turn": ("TurningAround", "once"),
    "turn_midair": ("TurningAroundMidAir", "once"),
    "turn_crouch": ("TurningAroundAndCrouching", "once"),
    "landing": ("Landing", "once"),
}
# Semantic action -> exact native (table, selector).
SIMPLE_ACTIONS = {
    "skid": ("Skidding", "FALSE", "once"),
    "spin_start": ("pose", "SPOSE_STARTING_SPIN_JUMP", "once"),
    "spin": ("pose", "SPOSE_SPINNING", "loop"),
    "space_jump": ("pose", "SPOSE_SPACE_JUMPING", "loop"),
    "screw_attack": ("ScrewAttacking", "FALSE", "loop"),
    "screw_attack_space": ("ScrewAttacking", "TRUE", "loop"),
    "wall_jump": ("pose", "SPOSE_STARTING_WALL_JUMP", "once"),
    "morph_start": ("pose", "SPOSE_MORPHING", "once"),
    "morph_ball": ("pose", "SPOSE_MORPH_BALL", "loop"),
    "rolling": ("pose", "SPOSE_ROLLING", "loop"),
    "morph_midair": ("pose", "SPOSE_MORPH_BALL_MIDAIR", "loop"),
    "unmorph": ("pose", "SPOSE_UNMORPHING", "once"),
    "ledge_hang": ("pose", "SPOSE_HANGING_ON_LEDGE", "loop"),
    "ledge_pull_forward": ("pose", "SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING", "once"),
    "ledge_pull_up": ("pose", "SPOSE_PULLING_YOURSELF_UP_FROM_HANGING", "once"),
    "hurt": ("pose", "SPOSE_GETTING_HURT", "once"),
    "hurt_morph": ("pose", "SPOSE_GETTING_HURT_IN_MORPH_BALL", "loop"),
    "death": ("pose", "SPOSE_DYING", "once"),
    "shinespark_charge": ("pose", "SPOSE_DELAY_BEFORE_SHINESPARKING", "once"),
    "shinespark": ("Shinesparking", "FORCED_MOVEMENT_UPWARDS_SHINESPARK", "loop"),
    "shinespark_side": ("Shinesparking", "FORCED_MOVEMENT_SIDEWARDS_SHINESPARK", "loop"),
    "shinespark_end": ("pose", "SPOSE_DELAY_AFTER_SHINESPARKING", "once"),
    "ball_spark": ("pose", "SPOSE_BALLSPARKING", "loop"),
}


def _write_atomic(path: Path, content: str | bytes) -> None:
    if path.is_symlink():
        raise ValueError("symlink output refused")
    temporary = path.with_suffix(path.suffix + ".tmp")
    if temporary.is_symlink():
        raise ValueError("symlink temporary output refused")
    if isinstance(content, str):
        temporary.write_text(content, encoding="utf-8")
    else:
        temporary.write_bytes(content)
    os.replace(temporary, path)


def build_animation_map(keys) -> tuple[str, dict]:
    """Resolve gameplay semantics to exact native composition keys."""
    available = set(keys)
    rows = []
    missing = []
    for suit in SUITS:
        for side in ("left", "right"):
            for action, (table, mode) in AIM_ACTIONS.items():
                for aim, selector in AIMS.items():
                    key = f"{suit}/{table}/{selector}/{side}"
                    if key in available:
                        rows.append((action, suit, side, aim, mode, key))
                    elif aim in ("forward", "diagonalup", "diagonaldown"):
                        missing.append(f"{action}/{suit}/{side}/{aim}")
            for action, (table, selector, mode) in SIMPLE_ACTIONS.items():
                key = f"{suit}/{table}/{selector}/{side}"
                if key in available:
                    rows.append((action, suit, side, "none", mode, key))
                else:
                    missing.append(f"{action}/{suit}/{side}/none")
    content = f"schema\t{MAP_SCHEMA}\n"
    content += "action\tsuit\tfacing\taim\tmode\tkey\n"
    content += "".join("\t".join(row) + "\n" for row in rows)
    report = {
        "schema": "metroidvania-samus-animation-map-report-v2",
        "rows": len(rows),
        "actions": len(AIM_ACTIONS) + len(SIMPLE_ACTIONS),
        "missing": missing,
        "graphics_by_suit": {suit: VISUAL_SUITS[suit][0] for suit in SUITS},
        "palette_by_suit": {suit: VISUAL_SUITS[suit][2] for suit in SUITS},
    }
    return content, report


def _run_nm(nm: str, elf: Path) -> str:
    return subprocess.run([nm, "-S", "--defined-only", str(elf)], check=True,
                          capture_output=True, text=True).stdout


def produce(root: Path, rom_path: Path, elf: Path, decomp: Path,
            nm: str = "arm-none-eabi-nm") -> dict:
    root = Path(root).resolve()
    rom = Path(rom_path).read_bytes()
    if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
        raise ValueError("original Zero Mission USA ROM SHA-1 mismatch")
    elf = Path(elf)
    if elf.is_symlink() or not elf.is_file():
        raise ValueError("missing or symlink MZM reference ELF")
    tables = parse_tables(
        (Path(decomp) / "src/data/samus/samus_animation_pointers.c")
        .read_text(encoding="utf-8"))
    symbols = parse_symbols(_run_nm(nm, elf))

    destination = private_path(root, METROID_SAMUS_RUNTIME, create=True)
    if destination.is_symlink():
        raise ValueError("private Samus runtime cannot be a symlink")
    objects = destination / "objects"
    if objects.is_symlink():
        raise ValueError("private Samus object store cannot be a symlink")
    objects.mkdir(parents=True, exist_ok=True)

    rows: list[str] = []
    packed: dict[str, int] = {}
    sequences: dict[str, dict] = {}

    def sink(key: str, frames, info: dict) -> None:
        if len(key) >= 160 or any(c in key for c in "\t\r\n"):
            raise ValueError("invalid animation key")
        if key in sequences:
            raise ValueError("duplicate animation key " + key)
        for index, (bmp, duration, offset_x, offset_y) in enumerate(frames):
            digest = hashlib.sha256(bmp).hexdigest()
            output = objects / f"{digest}.bmp"
            if output.is_symlink():
                raise ValueError("symlink asset destination")
            if output.exists():
                if output.read_bytes() != bmp:
                    raise ValueError("object hash collision")
            else:
                _write_atomic(output, bmp)
            packed[digest] = len(bmp)
            relative = output.relative_to(root).as_posix()
            rows.append(f"{key}\t{index}\t{duration}\t{offset_x}\t{offset_y}\t{relative}")
        sequences[key] = info

    report = compose_all(rom, tables, symbols, sink)
    if report["unresolved"]:
        raise ValueError("unresolved native Samus variants: " +
                         ", ".join(sorted(report["unresolved"])[:5]))
    removed = 0
    for stale in objects.glob("*.bmp"):
        if stale.stem not in packed and not stale.is_symlink():
            stale.unlink()
            removed += 1

    animation_map, map_report = build_animation_map(sequences)
    metadata = {
        "schema": "metroidvania-mzm-samus-runtime-v3",
        "index_schema": INDEX_SCHEMA,
        "sequences": len(sequences),
        "frames": len(rows),
        "unique_bmps": len(packed),
        "suits": {suit: sum(key.startswith(suit + "/") for key in sequences)
                  for suit in SUITS},
        "composition": {key: report[key] for key in
                        ("variants", "sequences", "frames", "table_problems")},
        "removed_stale_objects": removed,
        "object_store": "sha256",
        "animation_map": map_report,
        "note": "Private native extraction; source ROM data is never redistributed.",
    }
    index = f"schema\t{INDEX_SCHEMA}\n" + "\n".join(rows) + "\n"
    expected = (
        (destination / "runtime_index.tsv", index),
        (destination / "animation_map.tsv", animation_map),
        (destination / "sequences.json",
         json.dumps(sequences, indent=2, sort_keys=True) + "\n"),
        (destination / "manifest.json",
         json.dumps(metadata, indent=2, sort_keys=True) + "\n"),
    )
    for path, content in expected:
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            _write_atomic(path, content)
    return metadata


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "roms/Metroid - Zero Mission (USA).gba")
    parser.add_argument("--elf", type=Path,
                        default=ROOT / "third_party/mzm/mzm_us.elf")
    parser.add_argument("--decomp", type=Path,
                        default=ROOT / "third_party/mzm")
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    args = parser.parse_args(argv)
    try:
        result = produce(args.root, args.rom, args.elf, args.decomp, args.nm)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    print("Samus runtime library:", result["sequences"], "sequences,",
          result["frames"], "frames,", result["unique_bmps"], "unique BMPs")
    print("Semantic bindings:", result["animation_map"]["rows"])
    print("Index:", args.root.resolve() / METROID_SAMUS_RUNTIME / "runtime_index.tsv")
    print("State map:", args.root.resolve() / METROID_SAMUS_RUNTIME / "animation_map.tsv")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
