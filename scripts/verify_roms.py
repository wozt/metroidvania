#!/usr/bin/env python3
"""Strict local validation for the two supported ROM revisions."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

EXPECTED = {
    "aria": ("Castlevania: Aria of Sorrow USA", "abd71fe01ebb201bcc133074db1dd8c5253776c7"),
    "metroid": ("Metroid: Zero Mission USA", "5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8"),
}


def sha1_file(path: Path) -> str:
    digest = hashlib.sha1(usedforsecurity=False)
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate(kind: str, path: Path) -> tuple[bool, str]:
    label, expected = EXPECTED[kind]
    if not path.is_file():
        return False, f"{label}: file not found: {path}"
    actual = sha1_file(path)
    if actual != expected:
        return False, f"{label}: unknown SHA-1 {actual} (expected {expected})"
    return True, f"{label}: valid ROM ({actual})"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--aria", type=Path, required=True)
    parser.add_argument("--metroid", type=Path, required=True)
    args = parser.parse_args()
    success = True
    for kind, path in (("aria", args.aria), ("metroid", args.metroid)):
        valid, message = validate(kind, path)
        print(message)
        success &= valid
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
