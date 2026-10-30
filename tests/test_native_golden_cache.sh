#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# No ROM required: exercises cache hit, input invalidation and failure path.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/scripts" "$tmp/build" "$tmp/tests/fixtures" "$tmp/roms" \
    "$tmp/third_party/mzm" "$tmp/third_party/cvaos"

cp "$root/scripts/native_golden_cached.sh" "$tmp/scripts/"
cat > "$tmp/build/fusion_native_golden_tests" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >> "$(dirname -- "$0")/calls"
if [ "${MOCK_GOLDEN_FAIL:-0}" = 1 ]; then exit 1; fi
exit 0
EOF
chmod +x "$tmp/build/fusion_native_golden_tests"
printf 'importer-v1\n' > "$tmp/build/fusion_importer"
printf 'aos reference\n' > "$tmp/tests/fixtures/aos_native_golden.tsv"
printf 'mzm reference\n' > "$tmp/tests/fixtures/mzm_native_golden.tsv"
printf 'aos rom fixture\n' > "$tmp/roms/Castlevania - Aria of Sorrow (USA).gba"
printf 'mzm rom fixture\n' > "$tmp/roms/Metroid - Zero Mission (USA).gba"
printf 'elf fixture\n' > "$tmp/third_party/mzm/mzm_us.elf"
printf 'mzm database fixture\n' > "$tmp/third_party/mzm/database.json"
printf 'aos database fixture\n' > "$tmp/third_party/cvaos/database.json"
runner="$tmp/scripts/native_golden_cached.sh"
count() { if [ -f "$tmp/build/calls" ]; then wc -l < "$tmp/build/calls"; else echo 0; fi; }
sh "$runner" aos
sh "$runner" aos
[ "$(count)" -eq 1 ] || { echo 'aos cache miss on same inputs' >&2; exit 1; }
printf 'new fixture\n' >> "$tmp/tests/fixtures/aos_native_golden.tsv"
sh "$runner" aos
[ "$(count)" -eq 2 ] || { echo 'aos fixture change did not invalidate cache' >&2; exit 1; }
FUSION_GOLDEN_CACHE=0 sh "$runner" aos
[ "$(count)" -eq 3 ] || { echo 'aos cache bypass was ignored' >&2; exit 1; }
sh "$runner" mzm
sh "$runner" mzm
[ "$(count)" -eq 4 ] || { echo 'mzm cache miss on same inputs' >&2; exit 1; }
printf 'updated database\n' >> "$tmp/third_party/cvaos/database.json"
sh "$runner" mzm
[ "$(count)" -eq 5 ] || { echo 'mzm database change did not invalidate cache' >&2; exit 1; }
printf 'another importer\n' >> "$tmp/build/fusion_importer"
if MOCK_GOLDEN_FAIL=1 sh "$runner" aos; then
    echo 'failing golden test was accepted' >&2; exit 1
fi
[ "$(count)" -eq 6 ] || { echo 'failing test did not execute' >&2; exit 1; }
sh "$runner" aos
[ "$(count)" -eq 7 ] || { echo 'failed result was cached' >&2; exit 1; }
sh "$runner" aos
[ "$(count)" -eq 7 ] || { echo 'success after failure was not cached' >&2; exit 1; }
printf 'native golden cache contract: ok\n'
