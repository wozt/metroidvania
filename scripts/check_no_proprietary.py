#!/usr/bin/env python3
"""Échoue si le dépôt suivi semble contenir une ROM ou un gros asset extrait."""

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
            failures.append(f"extension interdite: {relative}")
        if path.is_file() and path.stat().st_size > 4 * 1024 * 1024 and not relative.startswith("third_party/"):
            failures.append(f"fichier > 4 Mio hors sous-module: {relative}")
    if failures:
        print("Fuite potentielle de données propriétaires:", file=sys.stderr)
        print("\n".join(f"- {item}" for item in failures), file=sys.stderr)
        return 1
    print("Contrôle anti-fuite: aucun fichier propriétaire suivi détecté.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
