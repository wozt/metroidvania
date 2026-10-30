#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Preserve an earlier successful native golden result only while every
# referenced executable and immutable input has identical SHA-256 bytes.
# This cache is private to build/; no ROM bytes or fixture outputs are stored.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mode=${1:-}
case "$mode" in aos|mzm) ;; *) echo 'usage: native_golden_cached.sh {aos|mzm}' >&2; exit 2;; esac
binary="$root/build/fusion_native_golden_tests"
importer="$root/build/fusion_importer"
# Golden updates and deliberate forced verification must always run the real
# test, including its normal skip code when a ROM or tool is missing.
if [ "${FUSION_GOLDEN_CACHE:-1}" = 0 ] || [ -n "${AOS_GOLDEN_UPDATE:-}" ] || [ -n "${MZM_GOLDEN_UPDATE:-}" ]; then
    exec "$binary" "$mode"
fi
# Inputs used directly by tests/test_native_golden.c's command vectors.
if [ "$mode" = aos ]; then
    set -- "$binary" "$importer" "$root/scripts/native_golden_cached.sh" \
        "$root/tests/fixtures/aos_native_golden.tsv" \
        "$root/roms/Castlevania - Aria of Sorrow (USA).gba"
else
    set -- "$binary" "$importer" "$root/scripts/native_golden_cached.sh" \
        "$root/tests/fixtures/mzm_native_golden.tsv" \
        "$root/roms/Metroid - Zero Mission (USA).gba" \
        "$root/third_party/mzm/mzm_us.elf" \
        "$root/roms/Castlevania - Aria of Sorrow (USA).gba" \
        "$root/third_party/mzm/database.json" \
        "$root/third_party/cvaos/database.json"
fi
# If an input is absent, do NOT cache even a skipped/partial result. Let the
# original test report the missing data and choose its own exit status.
for input do
    if [ ! -f "$input" ]; then
        exec "$binary" "$mode"
    fi
done
# Include the source directory, locale, script version and each file name as
# well as its content hash. All failures fall back to the real test.
if ! command -v sha256sum >/dev/null 2>&1; then
    exec "$binary" "$mode"
fi
if ! hashes=$(sha256sum "$@"); then
    exec "$binary" "$mode"
fi
if ! key=$(
    {
        printf 'fusion-native-golden-cache-v1\n%s\n%s\n%s\n%s\n' \
            "$root" "$mode" "${LC_ALL:-}" "${LANG:-}"
        printf '%s\n' "$hashes"
    } | sha256sum | cut -d ' ' -f 1
); then
    exec "$binary" "$mode"
fi
case "$key" in
    *[!0-9a-f]*|'') exec "$binary" "$mode" ;;
esac
stamp="$root/build/.fusion-native-golden-${mode}.ok"
if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$key" ]; then
    echo "native $mode golden: unchanged inputs; reusing prior verified result ($key)"
    exit 0
fi
# Run the original test and propagate its failure/skip without writing a stamp.
if "$binary" "$mode"; then
    temporary="$stamp.$$.tmp"
    if printf '%s\n' "$key" > "$temporary"; then
        mv -f "$temporary" "$stamp" || rm -f "$temporary"
    fi
    exit 0
else
    status=$?
    rm -f "$stamp"
    exit "$status"
fi
