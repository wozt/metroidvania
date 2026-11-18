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
  reconstructions, `data/native_parity/aos_monster_aspects.tsv` for each
  ported Aria monster's sprite, animations, sounds, loot and dropped soul
  (a monster is implemented with all five, not with its AI alone),
  `data/dashboard/features.tsv` for assets, tools, story,
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

Run each test once per cycle. The full suite is slow (the real-ROM tests
decode both games' data), so target it while working and run it in full only once, before the commit.

- While working, run only the tests that cover what changed: the C test
  binary of the touched module (`ctest --test-dir build -R <name>` or the
  executable in `build/`). Rerun a slow real-data test only when the code it
  covers changed.
- At the end of the cycle, in this order:
  1. `cmake --build build -j8`
  2. `./build/fusion_checklist`
  3. `./build/fusion_dashboard --run-tests`: the single full CTest pass.
     Every entry is checked; native ROM-wide golden entries may reuse a prior
     successful SHA-256 verification of identical binaries and private inputs.
     Use `FUSION_GOLDEN_CACHE=0` to force fresh imports when investigating a
     golden regression or for a periodically uncached full audit. The dashboard
     records outcomes, prints failed tests and exits non-zero when one failed. The leak
     check runs there as the `no_proprietary_files` CTest test.
  4. `./build/fusion_check_generated`
- Do not add a separate `ctest` or `fusion_check_no_proprietary` run to that cycle: step 3 already runs
  them. Run plain `ctest` only when the dashboard is not being regenerated.
- A failing test is fixed and its targeted test rerun; repeat the full run
  only if the fix touched code that other tests cover.

## Gameplay Fidelity, Vertical-Slice Development and Regression Gates

These rules are permanent and apply to every gameplay development cycle.

1. Measure progress by validated gameplay scenarios, not by translated-routine,
   file, object or feature counts.
2. Give every gameplay change a reproducible behavioral acceptance scenario
   with a defined start state, input schedule and observable result.
3. Compare against the native game before claiming fidelity; reconstruction
   sources and ROM-derived tests alone do not prove runtime behavior.
4. Treat unit tests as component evidence only. Integration requires the real
   runtime path, authoritative state and dependencies.
5. Keep simulation timing deterministic and independent from rendering,
   capture rate and host performance.
6. Add regression protection for every validated scenario before moving to the
   next gameplay slice.
7. Prioritize blockers in saves, movement, collision, progression and other
   core loops over breadth or cosmetic feature count.
8. Respect existing state ownership, update order and module boundaries when
   fixing a slice; do not bypass the architecture to make a scenario pass.
9. Do not use silent approximations, duplicate systems, fake state or arbitrary
   constants. Document any evidenced limitation and keep its status partial.
10. Preserve previously validated behavior and rerun the relevant regression
    gates whenever shared code or state changes.
11. Match completion and fidelity claims to the strongest available evidence;
    distinguish translated, unit-tested, integrated and emulator-compared work.
12. Keep each cycle focused, measurable and reviewable: one bounded objective,
    its root cause, its evidence, its regression test and its honest remaining
    gaps.

## Native Emulator Comparison Workflow (Aria of Sorrow)

Use the headless emulator laboratory whenever Aria behavior or rendering is
compared with the original game. Source reconstruction and verified-ROM unit
tests remain useful evidence, but they do not replace a live emulator
comparison.

1. **Start from a reproducible private checkpoint.** Use the user's local ROM
   and an existing save state with `./build/aos_mgba_lab --state-in`, or create
   a reusable checkpoint with `--state-out`. Keep ROMs, save states, SRAM,
   screenshots and memory dumps outside the repository (prefer `/tmp` or an
   ignored local directory); never stage, commit, push or transfer them.
2. **Record the exact native run.** Specify `--frames`, a frame-indexed
   `--keys` schedule, the `--capture` frame(s), and a private `--shot` prefix.
   Use `--peek` when the claim concerns WRAM, registers or controller state.
   A typical invocation is:

   ```sh
   ./build/aos_mgba_lab --rom /private/path/aria.gba \
     --state-in /tmp/aria_checkpoint.state --frames 120 \
     --keys '5:8,6:0' --capture '119' --shot /tmp/aria_native
   ```

3. **Match the portable run.** Exercise the same gameplay path, selection,
   input timing and observation frame in the runtime. Do not compare screens
   reached from materially different inventory, language, animation, camera
   or menu state without documenting that difference.
4. **Measure the result.** Prefer an exact pixel or mask diff for stable
   regions. For animated or stateful behavior, compare the relevant frame
   sequence and memory values; state the crop, metric, tolerance and numeric
   result. Visual inspection alone may guide debugging but is not sufficient
   evidence for a fidelity claim.
5. **Report only the proven scope.** Record the checkpoint origin, input
   schedule, capture frame and comparison result in `docs/PROJECT_STATUS.md`
   and the relevant parity annotation. A matching line, sprite or transition
   validates only that case; keep the containing feature `partial` until its
   remaining states and known divergences are covered.

The laboratory's broader architecture and controls are documented in
`docs/LAB.md`; the CLI option contract is kept at the top of
`src/tools/aos_mgba_lab.c`.

NO JULIUS MODE and NO SOUL TRADE in aria of sorrow
NO NES METROID in metroid zero mission
## Architecture, Native Fidelity and Integration Rules

These rules are permanent and apply to every development cycle.

1. **Search before implementing; reuse existing systems.** Before
   writing any new implementation, search the whole codebase for
   equivalent behavior and identify the current owner of that
   responsibility. Extend or integrate the existing implementation
   whenever appropriate; create a new component only when a genuine
   distinct responsibility requires it, and document why.
2. **No duplicate functional ownership.** One functional
   responsibility must have one clearly identified owner. Do not
   implement a second menu, equipment, stat-comparison or save
   system when an existing one can be extended correctly. Check
   `src/runtime/aos/menus/`, `src/runtime/aos/game/`,
   `src/runtime/aos/souls/` and the other established directories
   first.
3. **Respect the established directory and module
   responsibilities.** `src/runtime/aos/menus/` owns playable
   menu presentation and UX; `src/runtime/aos/game/` owns object
   and gameplay behavior; `src/runtime/aos/souls/` owns the soul
   systems; `src/runtime/aria_runtime.c` is the Aria runtime
   host. New code goes where its responsibility already lives.
4. **Preserve native routine traceability and human-readable
   object identities.** Native object numbers and source routine
   references stay in code comments, annotations and registries;
   every native object must have a documented, understandable
   functional identity. Use the existing registries as the source
   of truth and generate readable views instead of creating
   redundant registries.
5. **Unit tests are not integration.** Every feature must
   distinguish: native routine translated, unit-tested
   independently, linked into the actual runtime, connected to
   the correct native state and dependencies, exercised through
   a real gameplay path, and compared with native behavior where
   possible. Never mark a module functionally integrated because
   its unit tests pass. Unconnected implementations must remain
   explicitly classified as partial or unintegrated.
6. **No arbitrary state constants or silent gameplay
   approximations.** Never replace missing native state with
   arbitrary constants to make code compile or advance. Never
   silently replace native entities, animations, collision,
   events or particles with generic placeholders. When a
   dependency is missing, identify it, implement or connect it
   when in scope, otherwise document the limitation and keep the
   feature partial.
7. **No placeholder implementations presented as completed
   features.** Honest `partial`/`missing` statuses are required;
   never inflate completion percentages without evidence.
8. **Minimize monolithic runtime growth.** `aria_runtime.c`
   concentrates object dispatch; move coherent handling behind
   interfaces where it improves maintainability, without breaking
   native dispatch ordering or behavior. Refactor incrementally,
   preserving behavior and testing each step. Do not perform
   blind large-scale rewrites.
9. **Keep native source data, engine behavior and
   project-specific extensions separate.**
10. **Update existing registries and documentation** rather than
    duplicating tracking data: `annotations.tsv`,
    `aos_monster_aspects.tsv`, `features.tsv`, and the hand-written
    docs in the same commit as the change.
11. **Preserve the two distinct native gameplay engines** (Aria
    of Sorrow and Metroid: Zero Mission). No Julius mode, no soul
    trade, no NES Metroid.
12. **Require integration evidence before claiming fidelity.**
    Relevant integration tests and native comparisons are needed
    before a feature is called faithful or validated.
13. **Never remove or rewrite working systems** without an
    architectural justification and regression checks.

## Gameplay Fidelity, Vertical-Slice Development and Regression Gates

These rules are permanent and apply to every development cycle.

1. Development progress is measured by validated gameplay, not
   translated routine counts.
2. Every feature requires a reproducible behavioral acceptance
   scenario.
3. Native comparison is mandatory for fidelity claims.
4. Unit-test success does not imply gameplay integration.
5. Simulation timing must not depend on rendering frequency.
6. Validated gameplay scenarios require regression protection.
7. Core gameplay blockers take priority over unrelated new content.
8. Every change must respect native state ownership and existing
   architecture.
9. No silent approximations, duplicate functional owners or fake
   native state.
10. Every completed cycle must preserve previously validated
    gameplay behavior.
11. Feature completion claims must match the evidence.
12. Development cycles must remain focused, measurable and
    reviewable.
