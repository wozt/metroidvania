#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Shared private-asset helpers of the Python tools.

The raw ROM blocks, the import manifest and the Zero Mission room
descriptors are written by the native importer (``fusion_importer
--raw-import``, rebuild task ``raw_import``). This module keeps the verified
ROM reader and the safe, atomic writer the remaining Python tools use. All
generated files stay under ignored assets/extracted/. Never publish them.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

OUTPUT = ROOT / "assets" / "extracted"
ROMS = {
    "metroid": ("Metroid - Zero Mission (USA).gba",
                "5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8", "mzm"),
    "aria": ("Castlevania - Aria of Sorrow (USA).gba",
             "abd71fe01ebb201bcc133074db1dd8c5253776c7", "cvaos"),
}
_ALLOWED_NAME = re.compile(r"^[A-Za-z0-9_.-]+$")


def verified_rom(path: Path, expected: str) -> bytes:
    data = path.read_bytes()
    if hashlib.sha1(data).hexdigest() != expected:
        raise ValueError(f"ROM SHA-1 mismatch: {path} (expected verified USA ROM)")
    return data


def safe_parts(value: str) -> tuple[str, ...]:
    path = Path(value)
    if path.is_absolute() or not value or "\\" in value:
        raise ValueError(f"unsafe extracted path: {value!r}")
    parts = path.parts
    if not parts or any(part in (".", "..") or not _ALLOWED_NAME.fullmatch(part)
                        for part in parts):
        raise ValueError(f"unsafe extracted path: {value!r}")
    return parts


def safe_output(relative: str) -> Path:
    parts = safe_parts(relative)
    output = OUTPUT.joinpath(*parts)
    # A symlink at any path component could redirect extracted ROM bytes into
    # another directory. Do not even follow a preexisting output symlink.
    for candidate in (ROOT, ROOT / "assets", OUTPUT, *(
            OUTPUT.joinpath(*parts[:index]) for index in range(1, len(parts) + 1))):
        if candidate.is_symlink():
            raise ValueError(f"symlink extraction destination refused: {candidate}")
    return output


def write_generated(relative: str, data: bytes) -> bool:
    dest = safe_output(relative)
    if dest.is_file() and dest.read_bytes() == data:
        return False
    dest.parent.mkdir(parents=True, exist_ok=True)
    temp = dest.with_name(dest.name + ".tmp")
    if temp.is_symlink():
        raise ValueError(f"symlink extraction temp refused: {temp}")
    try:
        temp.write_bytes(data)
        temp.replace(dest)
    finally:
        if temp.exists():
            temp.unlink()
    return True
