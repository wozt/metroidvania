# Project status

Last updated: **2026-10-08**.

## Fixed vision and decisions

The project targets a real crossover with two distinct engines and one active
backend at a time. Personal USA ROMs are mandatory and remain local. Upstream
sources are isolated as submodules. Linux/SDL3 is the initial target; physical
GBA support is a later, unproven feasibility study. All project commits,
documentation, comments, UI text, and diagnostics are written in English.

## Actual state

| Item | Status | Evidence or limitation |
|---|---|---|
| Validation of both ROMs | Done | Python and C SHA-1 checks; rejection before SDL |
| `mzm` and `cvaos` sources | Done | Pinned submodules and targeted audit |
| Matching upstream GBA builds | Done | Both generated ROMs match references byte for byte |
| Two-backend interface | Done | Exclusive init/enter/tick/render/leave/shutdown lifecycle |
| Two diagnostic rooms | Done | SDL3, distinct geometry, ROMs required |
| Four character/world profiles | Done, simulated | Separate parameters, not original physics |
| HP, KO, and persistence | Done | Core tests and version-1 save data |
| Collision-safe character swap | Done | Deterministic offsets and rejection test |
| HUD, debug, and synergy | Partial | Dual HP and overlay; synergy explicitly a placeholder |
| Authentic Aria engine | Not started | AOT route audited but not reproduced |
| Authentic MZM engine | Not started | No runtime strategy selected |
| Original assets, maps, and audio | Not started | No extracted asset is tracked |
| Physical GBA port | Not started | Feasibility unknown |

## History

- 2026-10-08 - Initialized the repository, pinned `mzm` and `cvaos`, audited
  references, and installed verified Ghidra 12.1.4.
- 2026-10-08 - Added the C11/SDL3 skeleton, ROM validation, two diagnostic
  backends, session state, saves, tests, and documentation (`5f5aa04`).
- 2026-10-08 - Made SSH commit/push at the end of each work cycle a repository
  rule (`507764d`).
- 2026-10-08 - Installed the ARM binutils toolchain and reproduced matching GBA
  builds for both pinned decompilations in temporary workspaces.
- 2026-10-08 - Standardized all project-authored text on English.

## Commands and observed results

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/fusion_dev --validate-only
SDL_VIDEODRIVER=dummy timeout 2s ./build/fusion_dev
```

Normal and AddressSanitizer/UndefinedBehaviorSanitizer builds passed all 4 tests.
Both ROMs were accepted. The SDL smoke test remained active for two seconds and
was stopped by `timeout` with the expected status 124 and no runtime error.

Upstream build verification used local clones under `/tmp`, a locally built
`agbcc`, and symlinks to the ignored ROM files. Both `make` invocations exited
successfully and both generated ROMs matched the reference files with `cmp`.

## Gap between the prototype and authentic games

The prototype currently reads ROMs only to authenticate them. SDL3 draws the
rooms, `room_sim.c` supplies collisions, and all movement profiles are temporary.
No original ARM code, asset, room, enemy, boss, audio, or save format is executed.
The current backends are integration boundaries and test doubles.

## Risks and next actions

1. **P0 - Product blocker:** integrate one authentic Aria frame, then MZM.
2. **P0 - License:** decide whether PolyForm Noncommercial is acceptable before
   adopting `gbarecomp`.
3. **P0 - Technical:** select source HAL or recompilation for MZM after a proof
   of concept.
4. **P1 - State:** inventory EWRAM/IWRAM, room globals, and snapshot boundaries
   in Ghidra for both ROMs.
5. **P1 - Tests:** add frame traces, render captures, and real lifecycle tests.
6. **P2 - Content:** implement guest-character adaptations, progression, and
   synergy hooks.

## Open, non-blocking questions

- Is a strictly noncommercial dependency acceptable for this project?
- Should the first authentic runtime milestone prioritize Aria's more advanced
  AOT route or MZM's more readable C decompilation?
- What resurrection policy and progression flags should be shared?

## Next milestone acceptance criteria

- startup remains gated by both SHA-1 checks;
- one authentic frame from the Aria or MZM ROM, with no committed asset;
- the opposite backend remains fully suspended;
- reproducible capture and trace;
- explicit documentation of native, recompiled, interpreted, or emulated paths
  and every fallback;
- existing automated tests remain green and the license audit is updated.
