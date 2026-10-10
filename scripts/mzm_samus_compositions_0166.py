# SPDX-License-Identifier: GPL-3.0-only
"""Extract diagnostic left-facing and down-aim Samus body/cannon compositions."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from scripts.mzm_samus_frame import EXPECTED_SHA1
from scripts import mzm_samus_compositions_0160 as core
from scripts.mzm_samus_compositions_0164 import extend_sequences
from scripts.asset_layout import METROID_SAMUS_DIAGNOSTICS

SEQUENCES = {
    "midair_forward_left": (
        "sSamusAnim_PowerSuit_Left_MidAir",
        "sArmCannonAnim_Suit_Left_MidAir",
        "sArmCannonGfx_Upper_Forward_Left_Default",
        "sArmCannonGfx_Lower_Forward_Left_Default",
    ),
    "midair_diagonal_up_left": (
        "sSamusAnim_PowerSuit_Left_DiagonalUp_MidAir",
        "sArmCannonAnim_Suit_Left_DiagonalUp_MidAir",
        "sArmCannonGfx_Upper_DiagonalUp_Left_Default",
        "sArmCannonGfx_Lower_DiagonalUp_Left_Default",
    ),
    "shoot_standing_left": (
        "sSamusAnim_PowerSuit_Left_Shooting",
        "sArmCannonAnim_Suit_Left_Shooting",
        "sArmCannonGfx_Upper_Forward_Left_Armed_Default",
        "sArmCannonGfx_Lower_Forward_Left_Armed_Default",
    ),
    "shoot_crouch_left": (
        "sSamusAnim_PowerSuit_Left_ShootingAndCrouching",
        "sArmCannonAnim_Suit_Left_ShootingAndCrouching",
        "sArmCannonGfx_Upper_Forward_Left_Armed_Default",
        "sArmCannonGfx_Lower_Forward_Left_Armed_Default",
    ),
    "shoot_crouch_diagonal_up_left": (
        "sSamusAnim_PowerSuit_Left_DiagonalUp_ShootingAndCrouching",
        "sArmCannonAnim_Suit_Left_DiagonalUp_ShootingAndCrouching",
        "sArmCannonGfx_Upper_DiagonalUp_Left_Armed_Default",
        "sArmCannonGfx_Lower_DiagonalUp_Left_Armed_Default",
    ),
    "run_diagonal_down_left": (
        "sSamusAnim_PowerSuit_Left_DiagonalDown_Running",
        "sArmCannonAnim_Suit_Left_DiagonalDown_Running",
        "sArmCannonGfx_Upper_DiagonalDown_Left_Default",
        "sArmCannonGfx_Lower_DiagonalDown_Left_Default",
    ),
}


def build(rom, sized, directory, selected):
    original = core.SEQUENCES
    try:
        core.SEQUENCES = extend_sequences(original, SEQUENCES)
        result = core.run(rom, sized, directory, selected)
    finally:
        core.SEQUENCES = original
    result["schema"] = "metroidvania-mzm-samus-compositions-left-v1"
    result["scope"] = "additional left-facing original-body diagnostic composites"
    result["limitations"].append(
        "graphics variants and native OAM layering require visual comparison")
    return result


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--elf", type=Path, default=Path("third_party/mzm/mzm_us.elf"))
    p.add_argument("--output-dir", type=Path,
                   default=METROID_SAMUS_DIAGNOSTICS / "compositions/left")
    p.add_argument("--sequence", action="append", choices=sorted(SEQUENCES))
    args = p.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    try:
        output = core.safe_dir(root, args.output_dir)
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("expected original MZM USA ROM")
        nm = subprocess.run(["arm-none-eabi-nm", "-S", "--defined-only", str(args.elf)],
                            check=True, capture_output=True, text=True)
        symbols = core.parse_sized_symbols(nm.stdout)
        report = build(rom, symbols, output, args.sequence or list(SEQUENCES))
        output.mkdir(parents=True, exist_ok=True)
        manifest = output / "manifest.json"
        content = json.dumps(report, sort_keys=True, indent=2) + "\n"
        if manifest.is_symlink():
            raise ValueError("symlink manifest refused")
        if not manifest.exists() or manifest.read_text(encoding="utf-8") != content:
            manifest.write_text(content, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        p.error(str(exc))
    for name, row in report["sequences"].items():
        print(name, row["status"], len(row.get("frames", [])), "frames")
        if "symbols" in row:
            print("  missing symbols:", ", ".join(row["symbols"]))
        if "reason" in row:
            print(" ", row["reason"])
    print("Manifest:", manifest)


if __name__ == "__main__":
    main()
