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
| Original assets, maps, and audio | Partial, local-only | Verified Samus idle/run/jump/attack recipes and Soma idle cycle; no extracted asset is tracked |
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

The prototype reads ROMs to authenticate them and optional local tools extract
verified character graphics. SDL3 draws the rooms, `room_sim.c` supplies
collisions, and all movement profiles are temporary.
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

## Patch 007 - Aria runtime / SDL3 integrated pipeline test

- Added a ROM-free integration test that opens a synthetic runtime driver,
  steps ten frames, validates frame views, uploads them to SDL3 and renders
  them using the software renderer under the dummy video driver.
- This tests the full host-side video path, not authentic game execution.
- Neither `gbarecomp` nor unlicensed Aria frontend code is included.
- Next milestone: a legally compatible authentic execution driver.

## Patch 008 - transactional save loading

- F9 now validates into a candidate session before leaving the active backend.
- Invalid, truncated, or trailing save data leaves the live session unchanged.
- V1 payload validation covers version, character health, and target HP invariants.
- Core regression tests cover corrupted files and unmodified caller state.
- Existing binary save v1 format is retained. No ROM or proprietary data added.
- Note: backend entry failure currently aborts the session; rollback on entry
  failure remains a future lifecycle enhancement.

## Patch 009 - opt-in Aria video preview

- `fusion_dev --aria-video-test` uses the real AriaRuntime contract and SDL3
  video bridge while visiting the Castlevania simulated world (M switches worlds).
- Pixels are synthetic, generated from an independent GPLv3-owned driver;
  **NO original Aria gameplay, ROM data, or copyrighted assets are executed**.
- Both user ROMs must still pass local SHA-1 checks before the application starts.
- Default behavior and existing simulated rooms remain unchanged.
- Next: evaluate an authentic, GPL-compatible execution implementation.

## Patch 010 - optional local sprite animations

- Added an SDL3 BMP sprite renderer driven by idle/run/jump/attack simulation states.
- Both characters work in both test rooms without changing collisions or KO behavior.
- Frame files are loaded only from ignored `assets/extracted/sprites/` paths.
- No ROM extraction/decoding is implemented by this patch; actual sprite BMPs
  must be created locally from personally supplied game data.
- A colored rectangle remains the explicit fallback when assets are absent.
- Next: implement verified ROM-specific palette/tile/OAM extraction for both games.

## Local GBA graphics toolchain

- SHA-1-gated tools preview raw 4bpp/BGR555 tiles and compose standard GBA OBJ
  data with 1D or 2D mapping, palette banks, flips and alpha BMP output.
- Synthetic tests cover decoding, bounds, coordinates, mapping and transparency.
- These are hardware primitives, not character recipes; outputs remain local and
  ignored. See `docs/GBA_GRAPHICS_TOOLS.md`.

## Verified MZM Samus graphics milestone

- Built a hash-gated diagnostic chain for animation discovery, frame inspection,
  four-part OBJ VRAM staging, compact raw OAM decoding, palette inspection and
  body/cannon composition.
- Confirmed the two-byte header plus six-byte Samus OAM format, 2D OBJ mapping,
  Power Suit palette rows, separate cannon slots and front/behind layer flags
  against pinned source and a byte-identical matching build.
- Exact ROM-local recipes now generate idle (4), run (10), spin-jump (8) and
  forward-shooting (3) frames on stable OAM-axis canvases. SDL honors the
  source-defined 60 Hz durations and falls back when assets are absent.
- Scanner/probe results remain explicitly provisional until matched to symbols;
  extracted pixels stay ignored. See `docs/MZM_SAMUS.md` for formats, addresses,
  commands, limitations and remaining work.

## Verified Aria Soma graphics milestone

- Traced Soma initialization in the matching `cvaos` build to the exact
  graphics, palette, and animation descriptors used by the USA ROM.
- Decoded the native 128x128 sheet / four 64x64 cell format and idle sequence
  (`12, 13, 14, 13` with durations `30, 11, 11, 11`).
- Added a SHA-1-gated extractor for the complete four-frame idle cycle on a
  stable 18x34 canvas. Live research captures matched all 2,048 staged tile
  bytes for frames 12 and 13 and all 32 palette bytes against OBJ VRAM and
  palette RAM. The runtime used for comparison remains research-only and is not
  integrated.
- SDL asset discovery now uses the exact Soma idle durations and falls back to
  the animated idle cycle while other authentic Soma states are absent.
- The next graphics cycle is Soma's run sequence; extracted pixels remain
  ignored.
