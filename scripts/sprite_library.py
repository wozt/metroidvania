# SPDX-License-Identifier: GPL-3.0-only
"""Content-addressed private sprite libraries shared by both games.

A library is a directory below ``assets/extracted`` holding ``objects/`` (one
BMP per unique SHA-256) and ``runtime_index.tsv``:

    schema<TAB>metroidvania-sprite-index-v1
    key<TAB>frame<TAB>ticks<TAB>offset_x<TAB>offset_y<TAB>object path

``offset_x``/``offset_y`` place the BMP's top-left pixel relative to the
character's native draw origin, so a runtime never guesses anchors. Frames of a
key are consecutive and numbered from zero. Durations are 60 Hz ticks.
"""
from __future__ import annotations

import hashlib
import os
import struct
from pathlib import Path

from scripts.asset_layout import private_path

INDEX_SCHEMA = "metroidvania-sprite-index-v1"
MAX_KEY = 159
MAX_PATH = 255


def write_atomic(path: Path, content: str | bytes) -> None:
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


def bmp_from_pixels(pixels: dict[tuple[int, int], tuple[int, int, int]]
                    ) -> tuple[bytes, int, int]:
    """Crop opaque pixels to a 32-bit BGRA BMP; return it with its origin."""
    if not pixels:
        raise ValueError("frame has no visible pixels")
    left = min(x for x, _ in pixels)
    top = min(y for _, y in pixels)
    width = max(x for x, _ in pixels) - left + 1
    height = max(y for _, y in pixels) - top + 1
    if width > 512 or height > 512:
        raise ValueError("frame exceeds 512 pixels")
    data = bytearray(width * height * 4)
    for (x, y), (red, green, blue) in pixels.items():
        row = height - 1 - (y - top)
        offset = (row * width + (x - left)) * 4
        data[offset:offset + 4] = bytes((blue, green, red, 255))
    header = bytearray(108)
    struct.pack_into("<IiiHHII", header, 0, 108, width, height, 1, 32, 3, len(data))
    struct.pack_into("<IIII", header, 40, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    struct.pack_into("<I", header, 56, 0x73524742)
    bmp = struct.pack("<2sIHHI", b"BM", 14 + 108 + len(data), 0, 0, 122) + header + data
    return bmp, left, top


def bmp8_from_indices(pixels: dict[tuple[int, int], int],
                      colors: list[tuple[int, int, int]]) -> tuple[bytes, int, int]:
    """Crop palette-indexed pixels to an 8-bit BMP; return it with its origin.

    Index 0 is transparent and never stored by the callers; ``colors`` is the
    preview palette written into the file (up to 256 entries)."""
    if not pixels:
        raise ValueError("frame has no visible pixels")
    if not 1 <= len(colors) <= 256 or any(not 0 < index < len(colors)
                                         for index in pixels.values()):
        raise ValueError("palette index outside the preview palette")
    left = min(x for x, _ in pixels)
    top = min(y for _, y in pixels)
    width = max(x for x, _ in pixels) - left + 1
    height = max(y for _, y in pixels) - top + 1
    if width > 512 or height > 512:
        raise ValueError("frame exceeds 512 pixels")
    pitch = (width + 3) & ~3
    data = bytearray(pitch * height)
    for (x, y), index in pixels.items():
        data[(height - 1 - (y - top)) * pitch + (x - left)] = index
    palette = bytearray(1024)
    for index, (red, green, blue) in enumerate(colors):
        palette[index * 4:index * 4 + 4] = bytes((blue, green, red, 0))
    header = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 8, 0, len(data),
                         2835, 2835, 256, 0)
    offset = 14 + 40 + 1024
    bmp = struct.pack("<2sIHHI", b"BM", offset + len(data), 0, 0, offset) + header + palette + data
    return bmp, left, top


class LibraryWriter:
    """Accumulate keyed frames, store unique objects and write the index."""

    def __init__(self, root: Path, relative: Path):
        self.root = Path(root).resolve()
        self.destination = private_path(self.root, Path(relative), create=True)
        if self.destination.is_symlink():
            raise ValueError("private sprite library cannot be a symlink")
        self.objects = self.destination / "objects"
        if self.objects.is_symlink():
            raise ValueError("private object store cannot be a symlink")
        self.objects.mkdir(parents=True, exist_ok=True)
        self.rows: list[str] = []
        self.keys: list[str] = []
        self.packed: dict[str, int] = {}

    def add(self, key: str, frames) -> None:
        """Add ``frames`` as ``(bmp, ticks, offset_x, offset_y)`` tuples."""
        if (not key or len(key) > MAX_KEY or key in self.keys or
                any(character in key for character in "\t\r\n")):
            raise ValueError("invalid or duplicate sprite key: " + repr(key))
        if not frames:
            raise ValueError("sprite key has no frames: " + key)
        for index, (bmp, ticks, offset_x, offset_y) in enumerate(frames):
            if not 1 <= ticks <= 255:
                raise ValueError(f"invalid duration in {key}")
            if not -256 <= offset_x <= 256 or not -256 <= offset_y <= 256:
                raise ValueError(f"offset outside the supported range in {key}")
            if bmp[:2] != b"BM":
                raise ValueError(f"invalid BMP in {key}")
            digest = hashlib.sha256(bmp).hexdigest()
            output = self.objects / f"{digest}.bmp"
            if output.is_symlink():
                raise ValueError("symlink asset destination")
            if output.exists():
                if output.read_bytes() != bmp:
                    raise ValueError("object hash collision")
            else:
                write_atomic(output, bmp)
            self.packed[digest] = len(bmp)
            relative = output.relative_to(self.root).as_posix()
            if len(relative) > MAX_PATH:
                raise ValueError("object path too long")
            self.rows.append(
                f"{key}\t{index}\t{ticks}\t{offset_x}\t{offset_y}\t{relative}")
        self.keys.append(key)

    def finish(self) -> dict:
        """Write the index, prune unreferenced objects and report totals."""
        removed = 0
        for stale in self.objects.glob("*.bmp"):
            if stale.stem not in self.packed and not stale.is_symlink():
                stale.unlink()
                removed += 1
        index = f"schema\t{INDEX_SCHEMA}\n" + "\n".join(self.rows) + "\n"
        path = self.destination / "runtime_index.tsv"
        if not path.exists() or path.read_text(encoding="utf-8") != index:
            write_atomic(path, index)
        return {"index_schema": INDEX_SCHEMA, "sequences": len(self.keys),
                "frames": len(self.rows), "unique_bmps": len(self.packed),
                "removed_stale_objects": removed}

    def write_text(self, name: str, content: str) -> None:
        path = self.destination / name
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            write_atomic(path, content)
