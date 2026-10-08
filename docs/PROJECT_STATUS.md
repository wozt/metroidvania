# Project status

Last updated: **2026-10-08**.

## Fixed vision and decisions

The project targets a real crossover with two distinct engines and one active
backend at a time. Personal USA ROMs are mandatory and remain local. Upstream
sources are isolated as submodules. Linux/SDL3 is the initial target; physical
GBA support is a later, unproven feasibility study. All project commits,
documentation, comments, UI text, and diagnostics are written in English.
Original project-authored code and documentation use GPL-3.0-only. PolyForm-licensed `gbarecomp` is research-only and blocked from integration.

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
| Authentic Aria engine | Feasibility reproduced, not integrated | Standalone AOT Linux runtime: 36/36 tests, nonempty framebuffer, zero fallback after clean-room BIOS recompilation; PolyForm runtime incompatible with current GPLv3-only policy; unlicensed root host code excluded |
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
- 2026-10-08 - Reproduced the isolated Aria AOT Linux build, its clean-room
  BIOS path, all 36 configured tests, and a zero-fallback authentic framebuffer
  capture without adding generated or proprietary files to the repository.
- 2026-10-08 - Adopted PolyForm Noncommercial 1.0.0 for project-authored code
  and documentation, matching `gbarecomp` and the confirmed noncommercial
  project intent.

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

The separate `ariaOfSorrow-recomp` evaluation also ran under `/tmp`. Its
cartridge pass generated 12,736 AOT functions. A placeholder-BIOS run exposed
three interpreted BIOS addresses; generating and recompiling the clean-room
BIOS removed them. With runtime self-healing disabled, 300 frames completed
with nonempty palette, VRAM, and OAM, zero dispatch misses, and zero interpreted
instructions. A TCP capture returned a 240 x 160 nonempty framebuffer. All 36
configured upstream tests passed. See `docs/ARIA_RECOMP_EVALUATION.md`.

## Gap between the prototype and authentic games

The prototype currently reads ROMs only to authenticate them. SDL3 draws the
rooms, `room_sim.c` supplies collisions, and all movement profiles are temporary.
No original ARM code, asset, room, enemy, boss, audio, or save format is executed.
The current backends are integration boundaries and test doubles.

## Risks and next actions

1. **P0 - Product blocker:** identify a GPLv3-compatible Aria execution
   strategy before implementing a project-owned adapter. Do not incorporate
   PolyForm-licensed `gbarecomp` or unlicensed root frontend code.
2. **P0 - Technical:** select source HAL or recompilation for MZM after a proof
   of concept.
3. **P1 - State:** inventory EWRAM/IWRAM, room globals, and snapshot boundaries
   in Ghidra for both ROMs.
4. **P1 - Tests:** add frame traces, render captures, and real lifecycle tests.
5. **P2 - Content:** implement guest-character adaptations, progression, and
   synergy hooks.

## Open, non-blocking questions

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

## Runtime adapter milestone (contract only)

Added an SDL-independent Aria runtime API with open/step/frame/close operations,
240x160 RGBA8888 framebuffer validation and deterministic fake-driver tests.
No authentic Aria runtime is integrated, no game code is executed by this API,
and no ROM or proprietary assets are redistributed. A real implementation must
pass license review (GPL-3.0-only compatibility), local ROM verification,
frame tests and backend lifecycle tests before being enabled.

## Patch 005 - SDL3 video bridge

- Added SDL3 streaming-texture presentation for a 240x160 RGBA frame supplied
  by the existing Aria runtime interface. Nearest scaling, pitch validation,
  resource cleanup and a software-renderer test are included.
- This is *only* host-side rendering of synthetic or future runtime pixels.
  It is not a working Aria runtime and does not integrate `gbarecomp`.
- GPLv3 licensing remains a separate required audit: do not incorporate
  PolyForm Noncommercial code or unlicensed frontend files.
- Next: a GPL-compatible driver producing authentic user-ROM frames.

## GPLv3 license migration

- Original project-authored material moved to GPL-3.0-only; prior PolyForm
  grants for previous recipients are not revoked.
- MIT submodules remain unchanged, with their original notices.
- The earlier PolyForm-based Aria integration plan is blocked pending a
  separately licensed runtime, sufficient permission, or an independent
  GPL-compatible solution. Earlier AOT measurements remain research evidence.
- No gameplay source, extracted game content, or build implementation changed.
- Run the existing project build/tests after applying this documentation patch.
