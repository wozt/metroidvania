# Targeted tests during development

The CTest suite is already split into independently named tests. Use these
scopes instead of repeating the full suite after every enemy edit:

```sh
cmake --build build --target fusion_aos_enemy_tests -j8
sh scripts/test_scopes.sh enemy
sh scripts/test_scopes.sh aria
sh scripts/test_scopes.sh dashboard
sh scripts/test_scopes.sh mzm
```

- `enemy`: enemy runtime, audio, stats and pickup regressions.
- `aria`: all Aria CTest tests (without the slower ROM-wide golden suite).
- `dashboard`: generator and proprietary-file leak checks.
- `mzm`: Zero Mission tests if its code was changed.
- `full`: the exact authoritative dashboard/CTest full run.

For final validation, compile once and run exactly:

```sh
cmake --build build -j8
./build/fusion_checklist
./build/fusion_dashboard --run-tests
./build/fusion_check_generated
```

The dashboard's full CTest pass uses four parallel workers by default. For
smaller systems or memory-heavy tests, set `FUSION_CTEST_JOBS=2` or `=1`;
values 1..8 are supported. This changes *only* scheduling, never the number
of tests or their verdicts.

Scoped tests use `/usr/bin/time -p` when available to report real/user/sys
seconds. Native golden cache misses and explicit uncached golden checks are
also timed. The dashboard prints the wall time of its parallel full CTest
pass and any individual tests lasting at least three seconds. Cache hits
are still reported as cache hits, not as new ROM golden test executions. Do not run individual scopes and the full suite
repeatedly in the same patch iteration unless debugging a failure.

Auto-AI-PyPatch owns the commit/push after successful final validation.

## Cache for expensive native golden tests

The two ROM-wide golden tests still appear in every full CTest run. Their
first successful execution creates a private stamp in `build/`. On subsequent
runs the wrapper hashes each test/importer binary, relevant ROM/ELF, fixture
and decompilation database input. If all bytes match the previous successful
run, that previous golden verification is reused rather than re-importing
thousands of native asset files. Missing data, updated fixtures, changed
binaries or failed tests never produce a cached success.

This is *incremental validation*, not a claim that the full ROM importer was
executed anew: a CTest entry marked passed may be backed by an unchanged
verified input fingerprint. To force real re-execution for auditing:

```sh
FUSION_GOLDEN_CACHE=0 ./build/fusion_dashboard --run-tests
```

Only the two golden CTest entries are cached; all other tests rerun. The
standalone `native_golden_cache_contract` test checks cache hits, changes,
explicit bypass and failure invalidation without using ROMs. Generated
output and status remain derived from the authoritative C tools.
