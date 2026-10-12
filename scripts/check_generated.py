#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that the generated tracking documents match their registries.

Runs, without writing anything or running any test:

* ``scripts/native_parity.py --check``: ``docs/NATIVE_PARITY_CHECKLIST.md``
  against ``data/native_parity/annotations.tsv`` and the native inventory;
* ``scripts/dashboard.py --check``: the registry-derived part of
  ``dashboard/data.json`` and ``dashboard/data.js`` against both registries
  and the tracked files;
* ``scripts/mzm_sound_ids.py --check``: ``src/runtime/mzm_sound_ids.h``
  against the pinned decompilation's ``Sound`` enum.

Use it after editing a registry or its documentation; regenerate with
``python3 -m scripts.native_parity`` and
``python3 -m scripts.dashboard --run-tests``.
"""
from __future__ import annotations

import contextlib
import io
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import dashboard, mzm_sound_ids, native_parity  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    if argv:
        print("usage: check_generated.py (no arguments)", file=sys.stderr)
        return 2
    failures = 0
    errors = io.StringIO()
    try:
        with contextlib.redirect_stderr(errors):
            native_parity.main(["--check"])
    except SystemExit as exit_:
        if exit_.code:
            failures += 1
            print(errors.getvalue().strip().splitlines()[-1], file=sys.stderr)
    failures += 1 if dashboard.main(["--check"]) else 0
    if (mzm_sound_ids.DECOMP / "include/constants/audio.h").is_file():
        with contextlib.redirect_stdout(io.StringIO()) as output:
            stale = mzm_sound_ids.main(["--check"])
        if stale:
            failures += 1
            print(output.getvalue().strip(), file=sys.stderr)
    if failures:
        print(f"{failures} generated document(s) stale", file=sys.stderr)
        return 1
    print("Generated documents are fresh")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
