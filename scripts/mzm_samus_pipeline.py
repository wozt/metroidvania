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
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from scripts.asset_layout import METROID_PROJECTILES_RUNTIME, METROID_SAMUS_RUNTIME
from scripts import mzm_projectile_compose
from scripts.mzm_samus_compose import (
    VISUAL_SUITS,
    compose_all,
    palette_rows,
    parse_symbols,
    parse_tables,
    verify_symbols_against_rom,
)
from scripts.mzm_samus_frame import EXPECTED_SHA1
from scripts.sprite_library import LibraryWriter, bmp_from_pixels

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


def build_animation_map(keys) -> tuple[str, dict]:
    """Resolve gameplay semantics to exact native composition keys."""
    available = set(keys)
    rows = []
    missing = []
    for suit in SUITS:
        family = VISUAL_SUITS[suit][0]
        for side in ("left", "right"):
            for action, (table, mode) in AIM_ACTIONS.items():
                for aim, selector in AIMS.items():
                    key = f"{family}/{table}/{selector}/{side}"
                    if key in available:
                        rows.append((action, suit, side, aim, mode, key))
                    elif aim in ("forward", "diagonalup", "diagonaldown"):
                        missing.append(f"{action}/{suit}/{side}/{aim}")
            for action, (table, selector, mode) in SIMPLE_ACTIONS.items():
                key = f"{family}/{table}/{selector}/{side}"
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


CANNON_SCHEMA = "metroidvania-samus-cannon-offsets-v1"
PALETTE_SCHEMA = "metroidvania-samus-palettes-v1"


def palette_table(rows) -> str:
    """suit, kind, row and 16 RGB colors for every Samus palette array."""
    lines = [f"schema\t{PALETTE_SCHEMA}"]
    for suit, kind, row, colors in rows:
        lines.append("\t".join([suit, kind, str(row)] +
                                [f"{r:02x}{g:02x}{b:02x}" for r, g, b in colors]))
    return "\n".join(lines) + "\n"
PROJECTILE_SYMBOL_PREFIXES = ("sCommonSprites", "sBeamPal", "sNormalBeam",
                              "sChargedNormalBeam", "sLongBeam", "sChargedLongBeam",
                              "sIceBeam", "sChargedIceBeam", "sWaveBeam",
                              "sChargedWaveBeam", "sPlasmaBeam", "sChargedPlasmaBeam",
                              "sPistol", "sChargedPistol", "sMissile", "sSuperMissile",
                              "sBomb", "sPowerBomb")


def produce_projectiles(root: Path, rom: bytes, decomp: Path, symbols) -> dict:
    """Write the projectile sprite library; return its totals."""
    source = (decomp / "src/data/projectile_data.c").read_text(encoding="utf-8")
    library = LibraryWriter(root, METROID_PROJECTILES_RUNTIME)
    tables = {}

    def sink(key, frames, info):
        converted = []
        for pixels, duration in frames:
            bmp, left, top = bmp_from_pixels(pixels)
            converted.append((bmp, duration, left, top))
        library.add(key, converted)
        tables[key] = info

    report = mzm_projectile_compose.compose_all(rom, source, symbols, sink)
    if report["unresolved"]:
        raise ValueError("unresolved projectile tables: " +
                         ", ".join(sorted(report["unresolved"])[:5]))
    totals = library.finish()
    library.write_text("sequences.json", json.dumps(tables, indent=2, sort_keys=True) + "\n")
    return {**totals, "tables": report["tables"]}


def cannon_offsets(sequences: dict) -> str:
    """Per-frame arm cannon offsets (pixels from Samus's position) by key."""
    rows = [f"schema\t{CANNON_SCHEMA}"]
    for key in sorted(sequences):
        muzzle = sequences[key].get("muzzle")
        if key.endswith("/armed") or not muzzle:
            continue
        rows.extend(f"{key}\t{index}\t{x}\t{y}"
                    for index, (x, y) in enumerate(muzzle))
    return "\n".join(rows) + "\n"


def _run_nm(nm: str, elf: Path) -> str:
    return subprocess.run([nm, "-S", "--defined-only", str(elf)], check=True,
                          capture_output=True, text=True).stdout


def _elf_image(objcopy: str, elf: Path) -> bytes:
    with tempfile.TemporaryDirectory() as directory:
        image = Path(directory) / "reference.gba"
        subprocess.run([objcopy, "-O", "binary", str(elf), str(image)],
                       check=True, capture_output=True)
        return image.read_bytes()


def produce(root: Path, rom_path: Path, elf: Path, decomp: Path,
            nm: str = "arm-none-eabi-nm",
            objcopy: str = "arm-none-eabi-objcopy") -> dict:
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
    nm_output = _run_nm(nm, elf)
    symbols = parse_symbols(nm_output)
    projectile_symbols = parse_symbols(nm_output, PROJECTILE_SYMBOL_PREFIXES)
    image = _elf_image(objcopy, elf)
    verify_symbols_against_rom(rom, image, symbols)
    verify_symbols_against_rom(rom, image, projectile_symbols)

    library = LibraryWriter(root, METROID_SAMUS_RUNTIME)
    sequences: dict[str, dict] = {}

    def sink(key: str, frames, info: dict) -> None:
        library.add(key, frames)
        sequences[key] = info

    report = compose_all(rom, tables, symbols, sink)
    if report["unresolved"]:
        raise ValueError("unresolved native Samus variants: " +
                         ", ".join(sorted(report["unresolved"])[:5]))
    totals = library.finish()
    animation_map, map_report = build_animation_map(sequences)
    metadata = {
        "schema": "metroidvania-mzm-samus-runtime-v3",
        **totals,
        "families": {family: sum(key.startswith(family + "/") for key in sequences)
                     for family in ("PowerSuit", "FullSuit", "Suitless")},
        "composition": {key: report[key] for key in
                        ("variants", "sequences", "frames", "table_problems")},
        "object_store": "sha256",
        "animation_map": map_report,
        "note": "Private native extraction; source ROM data is never redistributed.",
    }
    library.write_text("animation_map.tsv", animation_map)
    library.write_text("cannon_offsets.tsv", cannon_offsets(sequences))
    library.write_text("palettes.tsv", palette_table(palette_rows(rom, symbols)))
    library.write_text("sequences.json",
                       json.dumps(sequences, indent=2, sort_keys=True) + "\n")
    metadata["projectiles"] = produce_projectiles(root, rom, Path(decomp),
                                                  projectile_symbols)
    library.write_text("manifest.json",
                       json.dumps(metadata, indent=2, sort_keys=True) + "\n")
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
    parser.add_argument("--objcopy", default="arm-none-eabi-objcopy")
    args = parser.parse_args(argv)
    try:
        result = produce(args.root, args.rom, args.elf, args.decomp, args.nm,
                         args.objcopy)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    print("Samus runtime library:", result["sequences"], "sequences,",
          result["frames"], "frames,", result["unique_bmps"], "unique BMPs")
    print("Semantic bindings:", result["animation_map"]["rows"])
    print("Projectile library:", result["projectiles"]["sequences"], "sequences,",
          result["projectiles"]["unique_bmps"], "unique BMPs")
    print("Index:", args.root.resolve() / METROID_SAMUS_RUNTIME / "runtime_index.tsv")
    print("State map:", args.root.resolve() / METROID_SAMUS_RUNTIME / "animation_map.tsv")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
