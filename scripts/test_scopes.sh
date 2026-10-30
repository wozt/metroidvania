#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Scoped, ROM-free development checks. Run a single full suite before commit.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
if [ ! -f build/CTestTestfile.cmake ]; then
    echo "Configure the project first: cmake -S . -B build" >&2
    exit 2
fi
jobs=${FUSION_CTEST_JOBS:-4}
case "$jobs" in ''|*[!0-9]*) echo "FUSION_CTEST_JOBS must be 1..8" >&2; exit 2;; esac
if [ "$jobs" -lt 1 ] || [ "$jobs" -gt 8 ]; then
    echo "FUSION_CTEST_JOBS must be 1..8" >&2
    exit 2
fi
scope=${1:-enemy}
case "$scope" in
    enemy) set -- ctest --test-dir build --parallel "$jobs" --output-on-failure \
        -R '^(aos_enemy|aos_audio|aos_stats|aos_pickup)$' ;;
    aria) set -- ctest --test-dir build --parallel "$jobs" --output-on-failure \
        -R '^aos_' ;;
    mzm) set -- ctest --test-dir build --parallel "$jobs" --output-on-failure \
        -R '^mzm_' ;;
    dashboard) set -- ctest --test-dir build --parallel "$jobs" --output-on-failure \
        -R '^(dashboard_generator|no_proprietary_files)$' ;;
    full) set -- ./build/fusion_dashboard --run-tests ;;
    *) echo "usage: scripts/test_scopes.sh {enemy|aria|mzm|dashboard|full}" >&2; exit 2;;
esac
# POSIX time: real (wall time), user and sys. The Debian time package is
# optional; retain the original test command when it is not installed.
if [ -x /usr/bin/time ]; then
    exec /usr/bin/time -p "$@"
fi
exec "$@"
