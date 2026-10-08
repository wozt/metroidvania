# SPDX-License-Identifier: GPL-3.0-only
"""Inspect candidate 3-row GBA BGR555 palettes in a verified local MZM ROM.

Candidates are heuristic, NOT proof of Samus palette identity.
"""
import argparse
import hashlib
from pathlib import Path
import struct

try:
    from scripts.mzm_samus_frame import EXPECTED_SHA1
except ModuleNotFoundError:
    from mzm_samus_frame import EXPECTED_SHA1


def palette_rows(data, offset, rows=3):
    if rows < 1 or rows > 16 or offset < 0 or offset % 2 or offset + rows * 32 > len(data):
        raise ValueError("palette region is misaligned or out of bounds")
    return [struct.unpack_from("<16H", data, offset + i * 32) for i in range(rows)]


def score_palette(rows):
    """Rank interesting palettes without claiming any game-specific identity."""
    if not rows:
        return 0
    nonzero = [v for row in rows for v in row[1:]]
    unique = len(set(nonzero))
    if unique < 6:
        return 0
    # Most visible palettes reuse some colors across adjacent rows.
    shared = sum(len(set(rows[0][1:]) & set(r[1:])) for r in rows[1:])
    used = sum(v != 0 for v in nonzero)
    return unique + shared * 2 + used // 8


def find_candidates(data, *, start=0, end=None, max_results=25, threshold=24):
    if end is None:
        end = len(data)
    if start < 0 or end > len(data) or start >= end or max_results < 1:
        raise ValueError("invalid scan range")
    ranked = []
    for off in range((start + 31) // 32 * 32, end - 95, 32):
        rows = palette_rows(data, off)
        rating = score_palette(rows)
        if rating >= threshold:
            ranked.append((rating, off))
    ranked.sort(key=lambda item: (-item[0], item[1]))
    return ranked[:max_results]


def bmp_swatch(rows):
    """24-bit BMP: a 16-color-wide swatch, 3 rows, each color 12x12."""
    width, height, size = 16 * 12, len(rows) * 12, 12
    stride = (width * 3 + 3) & ~3
    image = bytearray(stride * height)
    for ry, row in enumerate(rows):
        for ci, raw in enumerate(row):
            r = ((raw >> 0) & 31) * 255 // 31
            g = ((raw >> 5) & 31) * 255 // 31
            b = ((raw >> 10) & 31) * 255 // 31
            for yy in range(size):
                for xx in range(size):
                    y = ry * size + yy
                    x = ci * size + xx
                    pos = (height - 1 - y) * stride + x * 3
                    image[pos:pos + 3] = bytes((b, g, r))
    return (struct.pack("<2sIHHI", b"BM", 54 + len(image), 0, 0, 54) +
            struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0,
                        len(image), 0, 0, 0, 0) + image)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--start", type=lambda x: int(x, 0), default=0)
    p.add_argument("--end", type=lambda x: int(x, 0))
    p.add_argument("--max-results", type=int, default=25)
    p.add_argument("--offset", type=lambda x: int(x, 0),
                   help="Inspect a verified candidate offset and optionally export its swatch")
    p.add_argument("--output", type=Path, help="BMP output under ignored assets/extracted/")
    a = p.parse_args()
    try:
        data = a.rom.read_bytes()
        if hashlib.sha1(data).hexdigest() != EXPECTED_SHA1:
            p.error("Zero Mission USA SHA-1 mismatch")
        if a.offset is None:
            for rating, offset in find_candidates(data, start=a.start, end=a.end,
                                                  max_results=a.max_results):
                print(f"candidate=0x{offset:08X} score={rating}")
            print("UNVERIFIED heuristic candidates; do not label as Samus palettes.")
            return
        rows = palette_rows(data, a.offset)
        print(f"palette_offset=0x{a.offset:08X} score={score_palette(rows)}")
        if a.output is not None:
            root = Path(__file__).resolve().parent.parent
            allowed = (root / "assets/extracted").resolve()
            dst = a.output.absolute()
            if dst.suffix.lower() != ".bmp" or allowed not in dst.parents:
                p.error("output must be a .bmp inside assets/extracted/")
            if dst.is_symlink() or any(parent.is_symlink() for parent in dst.parents):
                p.error("symlink output refused")
            dst.parent.mkdir(parents=True, exist_ok=True)
            bmp = bmp_swatch(rows)
            if not dst.exists() or dst.read_bytes() != bmp:
                dst.write_bytes(bmp)
            print("Wrote local-only palette swatch:", dst)
    except (OSError, ValueError) as exc:
        p.error(str(exc))


if __name__ == "__main__":
    main()
