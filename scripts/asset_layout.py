#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Canonical paths for private generated assets.

This module contains paths only. Importers remain responsible for validating
their native input and never write outside the ignored extraction root.
"""
from __future__ import annotations

from pathlib import Path

EXTRACTED = Path("assets/extracted")
METROID = EXTRACTED / "metroid"
ARIA = EXTRACTED / "aria"
SHARED = EXTRACTED / "shared"

METROID_RAW = METROID / "raw"
# Raw ROM payload tree written by import_game_assets, relative to EXTRACTED.
METROID_RAW_DATA = METROID_RAW.relative_to(EXTRACTED) / "data"
METROID_ROOMS = METROID / "rooms"
METROID_MAPS = METROID / "maps"
METROID_TILESETS = METROID / "tilesets"
METROID_BACKGROUNDS = METROID / "backgrounds"
METROID_METADATA = METROID / "metadata"
METROID_SPRITES = METROID / "sprites"
METROID_SAMUS = METROID_SPRITES / "samus"
METROID_SAMUS_ANIMATIONS = METROID_SAMUS / "animations"
METROID_SAMUS_RUNTIME = METROID_SAMUS / "runtime"
METROID_PROJECTILES_RUNTIME = METROID_SPRITES / "projectiles" / "runtime"

ARIA_ROOMS = ARIA / "rooms"
ARIA_MAPS = ARIA / "maps"
ARIA_TILESETS = ARIA / "tilesets"
ARIA_BACKGROUNDS = ARIA / "backgrounds"
ARIA_METADATA = ARIA / "metadata"
ARIA_SPRITES = ARIA / "sprites"
ARIA_RAW = ARIA / "raw"
ARIA_SOMA = ARIA_SPRITES / "soma"
ARIA_SOMA_RUNTIME = ARIA_SOMA / "runtime"

SHARED_DIAGNOSTICS = SHARED / "diagnostics"
SHARED_MANIFESTS = SHARED / "manifests"
IMPORT_CATALOG = SHARED_MANIFESTS / "import_catalog.json"


def private_path(root: Path, relative: Path, *, create: bool = False) -> Path:
    """Resolve one canonical private path and reject symlink components."""
    root = Path(root).resolve()
    base = root / EXTRACTED
    target = root / relative
    if not target.is_relative_to(base):
        raise ValueError("asset path escapes the private extraction root")
    current = root
    for part in relative.parts:
        current /= part
        if current.is_symlink():
            raise ValueError(f"symlink private asset component refused: {current}")
        if current.exists() and current != target and not current.is_dir():
            raise ValueError(f"private asset parent is not a directory: {current}")
        if create and current != target:
            current.mkdir(exist_ok=True)
    return target
