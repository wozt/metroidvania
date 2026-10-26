# Repository working rules

- Write all commits, documentation, code comments, UI text, and diagnostics in English.
- Never stage, commit, push, or transfer ROMs, BIOS images, save files, or
  extracted proprietary data.
- At the end of every meaningful work cycle, run the relevant checks, update
  `docs/PROJECT_STATUS.md`, create a descriptive commit, and push the current
  branch to `origin` over SSH.
- Do not publish releases or rewrite remote history without an explicit request.
- After every feature change, synchronize the registries that describe it in
  the same commit: `data/native_parity/annotations.tsv` for the two original
  reconstructions, `data/dashboard/features.tsv` for assets, tools, story,
  platforms and the Metroidvania layer, and any hand-written document that
  states the feature's scope (`README.md`, `docs/*.md`). Update the notes,
  divergences, limitations, sources, tests, dependencies and next actions to
  match the code and its tests; change a status only when the evidence
  changed, never to raise a percentage, and never declare a feature faithful
  or validated without proof.
- Then regenerate the derived documents (`./build/fusion_checklist`
  for `docs/NATIVE_PARITY_CHECKLIST.md`, `./build/fusion_dashboard
  --run-tests` for `dashboard/data.json` and `dashboard/data.js`) and verify
  their consistency with `./build/fusion_check_generated`, which runs no
  tests and is enough on its own for documentation-only changes.

## Running tests

Run each test once per cycle. The full suite is slow (the real-ROM
equivalence tests recompute the Python references), so target it while
working and run it in full only once, before the commit.

- While working, run only the tests that cover what changed: the C test
  binary of the touched module (`ctest --test-dir build -R <name>` or the
  executable in `build/`), and the matching Python module, narrowed with
  `-k` when one test is enough (`python3 -m unittest
  tests.test_fusion_assets_mzm_equivalence -k sprite`). Rerun a slow
  equivalence test only when the code it compares changed.
- At the end of the cycle, in this order:
  1. `cmake --build build -j8`
  2. `./build/fusion_checklist`
  3. `./build/fusion_dashboard --run-tests`: the single full test run.
     It runs every CTest test except `python_tooling_and_data`, then the
     Python suite once (which decides that entry), records the outcomes,
     prints the failed tests and exits non-zero when one failed. The leak
     check runs there as the `no_proprietary_files` CTest test.
  4. `./build/fusion_check_generated`
- Do not add a separate `python3 -m unittest discover`, `ctest` or
  `fusion_check_no_proprietary` run to that cycle: step 3 already runs
  them. Run plain `ctest` only when the dashboard is not being regenerated.
- A failing test is fixed and its targeted test rerun; repeat the full run
  only if the fix touched code that other tests cover.
