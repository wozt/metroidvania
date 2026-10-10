# SPDX-License-Identifier: GPL-3.0-only
"""Compose three explicitly selected native Power Suit body/cannon sequences.

These are diagnostic compositions, not yet proof of game-equivalent rendering.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from scripts.mzm_samus_frame import EXPECTED_SHA1
from scripts.mzm_samus_export_0150 import frame_metadata
from scripts.mzm_samus_sprite import (
    _make_power_suit_frame, aligned_canvas_bounds,
    SAMUS_ANIMATION_RECORD_BYTES, ARM_CANNON_ANIMATION_RECORD_BYTES,
)

SEQUENCES = {
    "run_diagonal_up_right": (
        "sSamusAnim_PowerSuit_Right_DiagonalUp_Running",
        "sArmCannonAnim_Suit_Right_DiagonalUp_Running",
        "sArmCannonGfx_Upper_DiagonalUp_Right_Default",
        "sArmCannonGfx_Lower_DiagonalUp_Right_Default",
    ),
    "midair_forward_right": (
        "sSamusAnim_PowerSuit_Right_MidAir",
        "sArmCannonAnim_Suit_Right_MidAir",
        "sArmCannonGfx_Upper_Forward_Right_Default",
        "sArmCannonGfx_Lower_Forward_Right_Default",
    ),
    "shoot_crouch_right": (
        "sSamusAnim_PowerSuit_Right_ShootingAndCrouching",
        "sArmCannonAnim_Suit_Right_ShootingAndCrouching",
        "sArmCannonGfx_Upper_Forward_Right_Armed_Default",
        "sArmCannonGfx_Lower_Forward_Right_Armed_Default",
    ),
}
NM_RE = re.compile(r"^([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+[A-Za-z]\s+(\S+)$")


def parse_sized_symbols(output):
    found = {}
    for line in output.splitlines():
        match = NM_RE.fullmatch(line.strip())
        if match:
            addr, size, name = match.groups()
            # The ELF also contains repeated local compiler symbols such as
            # _fpadd_parts. Only our native animation/graphics assets matter.
            if not name.startswith(("sSamusAnim_", "sArmCannonAnim_",
                                    "sArmCannonGfx_")):
                continue
            item = (int(addr, 16), int(size, 16))
            if name in found and found[name] != item:
                raise ValueError("conflicting Samus symbol " + name)
            found[name] = item
    return found


def validate_pair(body, cannon, gfx, sizes, required_frames=None):
    body_addr, body_size = sizes[body]
    cannon_addr, cannon_size = sizes[cannon]
    if body_size < 16 or body_size % 16 or cannon_size < 8 or cannon_size % 8:
        raise ValueError("invalid native animation record alignment")
    body_count = body_size // 16
    cannon_count = cannon_size // 8
    if body_count > 256:
        raise ValueError("body animation exceeds safety limit")
    # ELF sizes include padding and zero terminators. Validate the number
    # of playable body frames separately; never assume the arrays match.
    if required_frames is None:
        if body_count != cannon_count:
            raise ValueError("body/cannon frame count mismatch")
    elif not 1 <= required_frames <= body_count or cannon_count < required_frames:
        raise ValueError("cannon array is shorter than playable body frames")
    for name in gfx:
        addr, size = sizes[name]
        if not 0x08000000 <= addr < 0x0E000000 or size != 64:
            raise ValueError("unexpected cannon graphics symbol " + name)
    return body_addr, cannon_addr, required_frames if required_frames is not None else body_count


def safe_dir(root, relative):
    base = (root / "assets/extracted").resolve()
    if base != root / "assets/extracted":
        raise ValueError("assets/extracted must not be redirected")
    out = (root / relative).absolute()
    if base not in out.parents:
        raise ValueError("output must be under assets/extracted")
    for candidate in (out, *out.parents):
        if candidate == base or base in candidate.parents:
            if candidate.is_symlink():
                raise ValueError("symlink output refused")
    return out


def run(rom, symbols, out, selected):
    report = {"schema": "metroidvania-mzm-samus-compositions-v1",
              "rom_sha1": EXPECTED_SHA1,
              "limitations": [
                  "diagnostic composition; visual equivalence remains unverified",
                  "fixed default/armed cannon graphics may differ per native frame",
                  "effects and non-Power-Suit variants not included",
              ],
              "sequences": {}}
    for key in selected:
        body, cannon, upper, lower = SEQUENCES[key]
        needed = (body, cannon, upper, lower)
        missing = [s for s in needed if s not in symbols]
        if missing:
            report["sequences"][key] = {"status": "missing-symbols", "symbols": missing}
            continue
        try:
            # Determine playable frames from the native zero-terminated
            # body records instead of treating the full ELF size as playback.
            playable = frame_metadata(rom, symbols[body][0], symbols[body][1])
            body_addr, cannon_addr, count = validate_pair(
                body, cannon, (upper, lower), symbols, len(playable))
            images = []
            metas = []
            for index in range(count):
                bmp, meta = _make_power_suit_frame(
                    rom, body_addr + index * SAMUS_ANIMATION_RECORD_BYTES, key,
                    cannon_addr + index * ARM_CANNON_ANIMATION_RECORD_BYTES,
                    (symbols[upper][0], symbols[lower][0]))
                images.append(bmp)
                metas.append(meta)
            bounds = aligned_canvas_bounds(metas)
            rows = []
            # Regenerate on a shared original-coordinate canvas.
            for index in range(count):
                bmp, meta = _make_power_suit_frame(
                    rom, body_addr + index * SAMUS_ANIMATION_RECORD_BYTES, key,
                    cannon_addr + index * ARM_CANNON_ANIMATION_RECORD_BYTES,
                    (symbols[upper][0], symbols[lower][0]), bounds)
                path = out / "composed" / key / f"{index:03d}.bmp"
                path.parent.mkdir(parents=True, exist_ok=True)
                if not path.exists() or path.read_bytes() != bmp:
                    path.write_bytes(bmp)
                rows.append({
                    "index": index, "bmp": path.relative_to(out).as_posix(),
                    "duration_ticks": meta["frame"]["duration"],
                    "muzzle_offset": list(meta["arm_cannon_animation"]["muzzle_offset"]),
                    "arm_cannon_oam_parts": meta["arm_cannon"]["count"] if meta["arm_cannon"] else 0,
                })
            report["sequences"][key] = {
                "status": "diagnostic-composed",
                "body_symbol": body, "cannon_symbol": cannon,
                "cannon_graphics_symbols": [upper, lower],
                "canvas_bounds": list(bounds), "frames": rows,
            }
        except (ValueError, KeyError) as exc:
            report["sequences"][key] = {"status": "unsupported-native-frame", "reason": str(exc)}
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--elf", type=Path, default=Path("third_party/mzm/mzm_us.elf"))
    parser.add_argument("--output-dir", type=Path,
                        default=Path("assets/extracted/samus_compositions_0160"))
    parser.add_argument("--sequence", choices=tuple(SEQUENCES), action="append",
                        help="repeat to extract chosen sequences; default: all three")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    try:
        out = safe_dir(root, args.output_dir)
        rom = args.rom.read_bytes()
        if hashlib.sha1(rom).hexdigest() != EXPECTED_SHA1:
            raise ValueError("expected unmodified MZM USA ROM")
        nm = subprocess.run(["arm-none-eabi-nm", "-S", "--defined-only", str(args.elf)],
                            check=True, text=True, capture_output=True)
        sizes = parse_sized_symbols(nm.stdout)
        report = run(rom, sizes, out, args.sequence or list(SEQUENCES))
        out.mkdir(parents=True, exist_ok=True)
        manifest = out / "manifest.json"
        data = json.dumps(report, indent=2, sort_keys=True) + "\n"
        if not manifest.exists() or manifest.read_text(encoding="utf-8") != data:
            manifest.write_text(data, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    for name, item in report["sequences"].items():
        print(name, item["status"], len(item.get("frames", [])), "frames")
        if "reason" in item:
            print(" ", item["reason"])
    print("Manifest:", manifest)


if __name__ == "__main__":
    main()
