# SPDX-License-Identifier: GPL-3.0-only
"""Bulk diagnostic native Samus composition with per-sequence reporting.

Graphic selection is a hypothesis until validated against original game logic.
Existing composition archives and runtime files are never modified.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from scripts import mzm_samus_compositions_0160 as core
from scripts.mzm_samus_frame import EXPECTED_SHA1
from scripts.asset_layout import METROID_SAMUS_COMPOSED_SOURCE

AIMS = ("Forward", "DiagonalUp", "DiagonalDown")
SIDES = ("Right", "Left")
ACTIONS = ("Standing", "Shooting", "Crouching", "ShootingAndCrouching",
           "Running", "MidAir", "Landing", "TurningAround")
# Explicitly record candidate graphic conventions. No claim that default
# or armed graphics is correct for every native animation frame.
ARMED = {"Shooting", "ShootingAndCrouching"}
CROUCH = {"Crouching", "ShootingAndCrouching"}


def build_candidates():
    result = {}
    for side in SIDES:
        for action in ACTIONS:
            aims = AIMS
            for aim in aims:
                tail = ("" if aim == "Forward" and action not in ("Running",) else aim + "_") + action
                body = f"sSamusAnim_PowerSuit_{side}_{tail}"
                cannon = f"sArmCannonAnim_Suit_{side}_{tail}"
                gfx_suffix = "Armed_Default" if action in ARMED else "Default"
                gfx = [f"sArmCannonGfx_{part}_{aim}_{side}_{gfx_suffix}"
                       for part in ("Upper", "Lower")]
                name = f"{action.lower()}_{aim.lower()}_{side.lower()}"
                result[name] = (body, cannon, *gfx)
    return result


CANDIDATES = build_candidates()


def collect(rom, symbols, output, selected):
    old = core.SEQUENCES
    try:
        core.SEQUENCES = {**old, **{key: CANDIDATES[key] for key in selected}}
        result = core.run(rom, symbols, output, selected)
    finally:
        core.SEQUENCES = old
    result["schema"] = "metroidvania-mzm-samus-bulk-diagnostics-v1"
    result["candidate_count"] = len(selected)
    result["graphic_selection"] = "heuristic candidate; not validated by native gameplay state"
    result["limitations"].append(
        "missing-symbols and unsupported-native-frame are expected and reported")
    result["limitations"].append(
        "full game fidelity requires frame-specific cannon graphics/OAM selection")
    return result


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--elf", type=Path, default=Path("third_party/mzm/mzm_us.elf"))
    p.add_argument("--output-dir", type=Path,
                   default=METROID_SAMUS_COMPOSED_SOURCE)
    p.add_argument("--sequence", action="append", choices=sorted(CANDIDATES))
    args = p.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    try:
        output = core.safe_dir(root, args.output_dir)
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("wrong original MZM USA ROM SHA-1")
        proc = subprocess.run(["arm-none-eabi-nm", "-S", "--defined-only", str(args.elf)],
                              capture_output=True, check=True, text=True)
        symbols = core.parse_sized_symbols(proc.stdout)
        selected = args.sequence or list(CANDIDATES)
        result = collect(rom, symbols, output, selected)
        output.mkdir(parents=True, exist_ok=True)
        manifest = output / "manifest.json"
        if manifest.is_symlink():
            raise ValueError("symlink manifest refused")
        data = json.dumps(result, sort_keys=True, indent=2) + "\n"
        if not manifest.exists() or manifest.read_text(encoding="utf-8") != data:
            manifest.write_text(data, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        p.error(str(exc))
    counts = {}
    for name, row in result["sequences"].items():
        state = row["status"]
        counts[state] = counts.get(state, 0) + 1
        print(f"{name}: {state} ({len(row.get('frames', []))} frames)")
    print("Summary:", counts)
    print("Manifest:", manifest)


if __name__ == "__main__":
    main()
