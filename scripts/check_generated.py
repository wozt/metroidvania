#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that the generated tracking documents match their registries.

Runs, without writing anything or running any test:

* ``build/fusion_checklist --check``: ``docs/NATIVE_PARITY_CHECKLIST.md``
  against ``data/native_parity/annotations.tsv`` and the native inventory;
* ``scripts/dashboard.py --check``: the registry-derived part of
  ``dashboard/data.json`` and ``dashboard/data.js`` against both registries
  and the tracked files;
* ``build/fusion_sound_ids --check``: ``src/runtime/mzm_sound_ids.h``
  against the pinned decompilation's ``Sound`` enum.

Use it after editing a registry or its documentation; regenerate with
``build/fusion_rebuild --checklist`` and
``python3 -m scripts.dashboard --run-tests``.
"""
from __future__ import annotations

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import dashboard  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    if argv:
        print("usage: check_generated.py (no arguments)", file=sys.stderr)
        return 2
    failures = 0
    try:
        checklist = subprocess.run(
            [ROOT / "build/fusion_checklist", "--check"], cwd=ROOT,
            check=False, capture_output=True, text=True)
    except OSError as exc:
        failures += 1
        print(f"cannot run native checklist check: {exc}", file=sys.stderr)
    else:
        if checklist.returncode:
            failures += 1
            message = checklist.stderr.strip() or checklist.stdout.strip()
            print(message or "native checklist check failed", file=sys.stderr)
        else:
            print(checklist.stdout, end="")
    failures += 1 if dashboard.main(["--check"]) else 0
    sound_ids = ROOT / "build" / "fusion_sound_ids"
    if (ROOT / "third_party/mzm/include/constants/audio.h").is_file() and sound_ids.is_file():
        result = subprocess.run([str(sound_ids), "--check"], cwd=ROOT, text=True,
                                capture_output=True, check=False)
        if result.returncode:
            failures += 1
            print(result.stdout.strip() or result.stderr.strip(), file=sys.stderr)
    if failures:
        print(f"{failures} generated document(s) stale", file=sys.stderr)
        return 1
    print("Generated documents are fresh")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
