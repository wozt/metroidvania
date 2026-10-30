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
of tests or their verdicts. Do not run individual scopes and the full suite
repeatedly in the same patch iteration unless debugging a failure.

Auto-AI-PyPatch owns the commit/push after successful final validation.
