#!/usr/bin/env python3
"""Fail if tracked or unignored files look like ROMs or extracted assets."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

FORBIDDEN = {".gba", ".gb", ".gbc", ".nds", ".3ds", ".cia", ".sav", ".state"}


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        check=True, capture_output=True,
    )
    failures: list[str] = []
    for raw in result.stdout.split(b"\0"):
        if not raw:
            continue
        relative = raw.decode("utf-8", errors="replace")
        path = root / relative
        if path.suffix.lower() in FORBIDDEN:
            failures.append(f"forbidden extension: {relative}")
        if path.is_file() and path.stat().st_size > 4 * 1024 * 1024 and not relative.startswith("third_party/"):
            failures.append(f"file larger than 4 MiB outside submodules: {relative}")
    if failures:
        print("Potential proprietary data leak:", file=sys.stderr)
        print("\n".join(f"- {item}" for item in failures), file=sys.stderr)
        return 1
    print("Leak check passed: no tracked proprietary file detected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
