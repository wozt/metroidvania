# SPDX-License-Identifier: GPL-3.0-only
"""Safely cache actual native ENEMY sprites exported from the owner's source ROM.

This is an OPTIONAL path for PNGs already authentically decoded; it does not
pretend to decode enemy OAM from scratch. Only private ignored assets are written.

Example: python3 -m scripts.native_enemy_sprite_cache --world aria \
           --native-type enemy:07 --source ~/Pictures/real-aria-enemy.png
"""
from __future__ import annotations
import argparse
import os
import re
import struct
import tempfile
from pathlib import Path
from scripts.import_game_assets import OUTPUT

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
VALID_ARIA = re.compile(r"enemy:[0-9A-Fa-f]{2}\Z")
VALID_MZM = re.compile(r"PSPRITE_[A-Z0-9_]{1,60}\Z")


def validate_png(source: Path) -> bytes:
    if source.is_symlink() or not source.is_file():
        raise ValueError("sprite source must be a regular non-symlink PNG file")
    if source.stat().st_size < 45 or source.stat().st_size > 4 * 1024 * 1024:
        raise ValueError("sprite PNG exceeds size limits")
    data = source.read_bytes()
    if data[:8] != PNG_SIGNATURE or data[12:16] != b"IHDR":
        raise ValueError("invalid PNG header")
    width, height = struct.unpack_from(">II", data, 16)
    if not 1 <= width <= 512 or not 1 <= height <= 512:
        raise ValueError("native sprite is too large or empty")
    # GTK/Cairo remains the authoritative complete PNG decoder at display time.
    return data


def destination(world: str, native_type: str, root: Path | None = None) -> Path:
    if world == "aria":
        if not VALID_ARIA.fullmatch(native_type) or int(native_type[6:], 16) > 0x70:
            raise ValueError("expected a documented Aria enemy constructor ID 00..70")
        native_type = "enemy:" + native_type[6:].upper()
    elif world == "mzm":
        if not VALID_MZM.fullmatch(native_type):
            raise ValueError("expected a MZM PSPRITE symbol")
        from scripts.object_catalog import mzm_sprite_kind
        if mzm_sprite_kind(native_type) != "ENEMY":
            raise ValueError("MZM source sprite must have the ENEMY role")
    else:
        raise ValueError("unknown game")
    folder = Path(root) if root is not None else OUTPUT / "sprite_previews"
    return folder / world / (native_type + ".png")


def install(world: str, native_type: str, source: Path,
            root: Path | None = None) -> Path:
    data = validate_png(source)
    dest = destination(world, native_type, root)
    parent = dest.parent
    if parent.is_symlink() or dest.is_symlink():
        raise ValueError("private sprite cache symlinks not allowed")
    parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.read_bytes() == data:
        return dest
    tmp = None
    try:
        fd, tmp = tempfile.mkstemp(prefix=".native-sprite-", dir=parent)
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        if dest.is_symlink():
            raise ValueError("private sprite destination changed to symlink")
        os.replace(tmp, dest)
    finally:
        if tmp and os.path.exists(tmp):
            os.unlink(tmp)
    return dest


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--world", choices=("aria", "mzm"), required=True)
    ap.add_argument("--native-type", required=True)
    ap.add_argument("--source", type=Path, required=True)
    args = ap.parse_args()
    print("Private sprite installed:", install(args.world, args.native_type, args.source))


if __name__ == "__main__":
    main()
