#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compatibility entry point for the canonical unnumbered Samus pipeline."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.mzm_samus_pipeline import SUITS, main, produce, safe_asset_path

__all__ = ("SUITS", "main", "produce", "safe_asset_path")


if __name__ == "__main__":
    raise SystemExit(main())
