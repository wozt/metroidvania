# SPDX-License-Identifier: GPL-3.0-only
"""Batch diagnostic extraction of special native Power Suit motions.

Only proposed frame compositions. No gameplay behavior or original GBA
graphics selection is asserted by matching a symbol name.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from scripts import mzm_samus_compositions_0160 as core
from scripts.mzm_samus_frame import EXPECTED_SHA1

# Distinct native pose suffixes; unknown matching cannon/OAM layouts are
# reported as failures rather than substituted with unrelated animations.
SPECIAL = (
    "Spinning", "StartingSpinJump", "SpaceJumping", "ScrewAttacking",
    "StartingWallJump", "Morphball", "Morphing", "Unmorphing",
    "MorphballMotionless", "Shinesparking", "Sidewards_Shinesparking",
    "DelayBeforeShinesparking", "DelayAfterShinesparking",
    "Ballsparking", "GettingKnockedBack", "Dying",
    "HangingOnLedge", "PullingYourselfUpFromHanging",
    "PullingYourselfForwardFromHanging", "Skidding",
)
SIDES = ("Right", "Left")


def candidate_table():
    rows = {}
    for side in SIDES:
        for motion in SPECIAL:
            tail = f"{side}_{motion}"
            # These are discovery candidates, not an assertion that the
            # cannon is visible, armed or even present in the animation.
            cannon_motion = "GettingHurt" if motion == "GettingKnockedBack" else motion
            cannon_tail = f"{side}_{cannon_motion}"
            if motion == "Dying":
                cannon_tail = "Dying"
            # Selected generic graphics provide diagnostics only.
            gfx = (
                f"sArmCannonGfx_Upper_Forward_{side}_Default",
                f"sArmCannonGfx_Lower_Forward_{side}_Default",
            )
            key = f"{motion.lower()}_{side.lower()}"
            rows[key] = (
                f"sSamusAnim_PowerSuit_{tail}",
                f"sArmCannonAnim_Suit_{cannon_tail}", *gfx,
            )
    return rows


CANDIDATES = candidate_table()


def make_report(rom, symbols, out, selected):
    old = core.SEQUENCES
    try:
        core.SEQUENCES = {**old, **{key: CANDIDATES[key] for key in selected}}
        report = core.run(rom, symbols, out, selected)
    finally:
        core.SEQUENCES = old
    report["schema"] = "metroidvania-mzm-special-motions-diagnostic-v1"
    report["warning"] = (
        "Symbol candidates and fixed default cannon graphics are NOT "
        "validated GBA visual/gameplay compositions. Morph Ball, spinning "
        "and death may require dedicated rendering paths."
    )
    return report


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", required=True, type=Path)
    p.add_argument("--elf", type=Path, default=Path("third_party/mzm/mzm_us.elf"))
    p.add_argument("--output-dir", type=Path,
                   default=Path("assets/extracted/samus_special_0171"))
    p.add_argument("--sequence", choices=sorted(CANDIDATES), action="append")
    args = p.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    try:
        out = core.safe_dir(root, args.output_dir)
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("unexpected MZM USA ROM checksum")
        process = subprocess.run(
            ["arm-none-eabi-nm", "-S", "--defined-only", str(args.elf)],
            text=True, capture_output=True, check=True)
        symbols = core.parse_sized_symbols(process.stdout)
        report = make_report(rom, symbols, out, args.sequence or list(CANDIDATES))
        out.mkdir(parents=True, exist_ok=True)
        target = out / "manifest.json"
        if target.is_symlink():
            raise ValueError("manifest symlink refused")
        contents = json.dumps(report, sort_keys=True, indent=2) + "\n"
        if not target.exists() or target.read_text(encoding="utf-8") != contents:
            target.write_text(contents, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        p.error(str(exc))
    totals = {}
    for name, item in report["sequences"].items():
        status = item["status"]
        totals[status] = totals.get(status, 0) + 1
        print(f"{name}: {status} ({len(item.get('frames', []))} frames)")
        if item.get("reason"):
            print(" ", item["reason"])
    print("Summary:", totals)
    print("Manifest:", target)


if __name__ == "__main__":
    main()
