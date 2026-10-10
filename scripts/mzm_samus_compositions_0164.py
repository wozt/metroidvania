# SPDX-License-Identifier: GPL-3.0-only
"""Expand diagnostic native Samus compositions without modifying the 0160 set."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from scripts.mzm_samus_frame import EXPECTED_SHA1
from scripts import mzm_samus_compositions_0160 as base
from scripts.asset_layout import METROID_SAMUS_DIAGNOSTICS

ADDITIONAL = {
    "run_diagonal_down_right": (
        "sSamusAnim_PowerSuit_Right_DiagonalDown_Running",
        "sArmCannonAnim_Suit_Right_DiagonalDown_Running",
        "sArmCannonGfx_Upper_DiagonalDown_Right_Default",
        "sArmCannonGfx_Lower_DiagonalDown_Right_Default",
    ),
    "run_diagonal_up_left": (
        "sSamusAnim_PowerSuit_Left_DiagonalUp_Running",
        "sArmCannonAnim_Suit_Left_DiagonalUp_Running",
        "sArmCannonGfx_Upper_DiagonalUp_Left_Default",
        "sArmCannonGfx_Lower_DiagonalUp_Left_Default",
    ),
    "shoot_standing_right": (
        "sSamusAnim_PowerSuit_Right_Shooting",
        "sArmCannonAnim_Suit_Right_Shooting",
        "sArmCannonGfx_Upper_Forward_Right_Armed_Default",
        "sArmCannonGfx_Lower_Forward_Right_Armed_Default",
    ),
    "midair_diagonal_up_right": (
        "sSamusAnim_PowerSuit_Right_DiagonalUp_MidAir",
        "sArmCannonAnim_Suit_Right_DiagonalUp_MidAir",
        "sArmCannonGfx_Upper_DiagonalUp_Right_Default",
        "sArmCannonGfx_Lower_DiagonalUp_Right_Default",
    ),
    "shoot_crouch_diagonal_up_right": (
        "sSamusAnim_PowerSuit_Right_DiagonalUp_ShootingAndCrouching",
        "sArmCannonAnim_Suit_Right_DiagonalUp_ShootingAndCrouching",
        "sArmCannonGfx_Upper_DiagonalUp_Right_Armed_Default",
        "sArmCannonGfx_Lower_DiagonalUp_Right_Armed_Default",
    ),
}


def extend_sequences(original, additions):
    overlap = set(original).intersection(additions)
    if overlap:
        raise ValueError("duplicate animation names: " + repr(sorted(overlap)))
    if len(set(additions)) != len(additions):
        raise ValueError("duplicate animation keys")
    return {**original, **additions}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--elf", type=Path, default=Path("third_party/mzm/mzm_us.elf"))
    parser.add_argument("--output-dir", type=Path,
                        default=METROID_SAMUS_DIAGNOSTICS / "compositions/extended")
    parser.add_argument("--sequence", action="append", choices=sorted(ADDITIONAL))
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    try:
        output = base.safe_dir(root, args.output_dir)
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("expected the pinned MZM USA ROM")
        nm = subprocess.run(["arm-none-eabi-nm", "-S", "--defined-only", str(args.elf)],
                            capture_output=True, text=True, check=True)
        symbols = base.parse_sized_symbols(nm.stdout)
        # Keep the global table unchanged after extraction, including on errors.
        old = base.SEQUENCES
        try:
            base.SEQUENCES = extend_sequences(old, ADDITIONAL)
            report = base.run(rom, symbols, output, args.sequence or list(ADDITIONAL))
        finally:
            base.SEQUENCES = old
        report["schema"] = "metroidvania-mzm-samus-compositions-extension-v1"
        report["scope"] = "additional diagnostic Power Suit sequences only"
        report["limitations"].append(
            "directional graphics and arm cannon frame fidelity require visual verification")
        output.mkdir(parents=True, exist_ok=True)
        manifest = output / "manifest.json"
        data = json.dumps(report, indent=2, sort_keys=True) + "\n"
        if not manifest.exists() or manifest.read_text(encoding="utf-8") != data:
            manifest.write_text(data, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    for key, record in report["sequences"].items():
        print(key, record["status"], len(record.get("frames", [])), "frames")
        if "reason" in record:
            print(" ", record["reason"])
        if "symbols" in record:
            print("  missing:", ", ".join(record["symbols"]))
    print("Manifest:", manifest)


if __name__ == "__main__":
    main()
