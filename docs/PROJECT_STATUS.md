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
| Original assets, maps, and audio | Partial, local-only | One verified complete Samus idle-frame recipe; no extracted asset is tracked |
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

## Patch 011 - local GBA 4bpp inspector

- Added a SHA-1-gated raw 4bpp/BGR555 tile viewer; output is local and ignored.
- Added synthetic decoding and bounds tests. No ROM assets are committed.
- Tile grids are **not** assembled character sprites: OAM, animation and graphics
  pointers still need ROM-specific interpretation.

## Patch 012 - local GBA OBJ/OAM composition primitives

- Added 4bpp OBJ tile/OAM/BGR555 composition with 1D tile mapping and alpha BMP output.
- Unit tests cover signed OBJ coordinates, palette banks, tile flips and transparency.
- Inputs require verified ROM SHA-1 and explicit offsets; no game assets are shipped.
- Samus top/bottom VRAM staging, arm cannon, and Aria-specific animation chains are not yet reconstructed.

## Patch 013 - Zero Mission Samus graphics frame staging

- Added a ROM-pointer-aware SamusAnimationData reader and four-part OBJ VRAM
  staging for shoulders, torso, legs and lower body, using the pinned mzm docs.
- Bounds checking, slot-capacity validation and deterministic synthetic tests.
- Requires a verified game frame pointer; OAM, palette and cannon rendering
  remain unimplemented. No proprietary output is tracked.

## Patch 014 - MZM Samus animation table discovery

- Added a ROM-hash-gated candidate scanner for arrays pointing to plausible
  Samus animation frames, plus synthetic regression tests.
- The output is address-only research metadata: no graphics or ROM dumps.
- Candidate tables must be matched to upstream symbols before assigning poses.
- Next: verify the chosen pointer chains and decode game-specific OAM.

## Patch 016 - inspect Samus candidate frame tables

- Added read-only inspection of verified-ROM candidate animation tables, frame
  durations, graphics subset counts and raw OAM prefixes.
- Does not yet assign poses or assemble copyrighted sprites.
- Added synthetic tests for bounds, alignment and frame metadata.

## Patch 017 - Samus OAM header diagnostics

- Added a ROM-hash-checked OAM layout probe for candidate Samus frames.
- Probe compares plausible header sizes and object strides without asserting identity.
- No ROM data is exported, committed, or embedded.

## Patch 018 - bounded Samus OAM candidates

- Added count-bounded candidate decoding for Samus OAM layout hypotheses.
- Four small synthetic tests cover count bounds, decoding, and invalid data.
- No ROM-derived assets are exported and no layout is asserted verified.
- Next: confirm the raw OAM format against pinned mzm source.

## Patch 0019 - verified raw Samus OAM format

- Confirmed the body OAM layout from pinned `mzm/tools/oam.py`: one
  16-bit count/flags header followed by six bytes per OBJ entry.
- Added bounded header/entry decoding and metadata-only ROM inspection.
- The next milestone is validating Samus palette staging and composing
  body parts from the per-frame OBJ VRAM banks; no proprietary assets added.

## Patch 0020 - Samus local body composition

- Added a diagnostic ROM-local Samus body composer using four staged VRAM
  banks and the confirmed raw OAM layout.
- Requires an explicit, verified 16-color palette offset. Does not infer
  animations, draw the arm cannon or bundle copyrighted data.
- Extracted BMP output remains inside ignored `assets/extracted/`.

## Patch 0021 - inspect MZM suit palette candidates

- Added an exact-ROM-hash-checked BGR555 three-row palette scanner and
  optional local BMP swatch export, plus independent decoder tests.
- Results are explicitly heuristic. No palette is automatically asserted
  to be Samus's Power Suit palette; cross-check upstream symbols first.
- Extracted graphics remain local and ignored by Git.

## Patch 0022 - multi-bank Samus palettes

- The local Samus body compositor now maps up to 16 caller-verified BGR555
  palette rows into explicit OBJ palette banks and rejects unmatched OAM banks.
- Defaults preserve the former single-row behavior. Palette detection and
  arm cannon/effects are not yet implemented.

## Patch 0023 - first complete verified Samus frame

- Matched the Power Suit right-standing frame, default palette, standing arm
  cannon animation, and forward cannon graphics to exact symbols in a pinned
  build whose ROM is byte-identical to the required USA ROM.
- Corrected Samus composition to the game's 2D OBJ mapping. The prior 1D body
  preview could not reproduce the separated VRAM tile rows correctly.
- Added full body/cannon staging, raw OAM layer ordering, transparent trimming,
  and deterministic local BMP generation at the SDL3 loader's `samus/idle_0.bmp`
  path.
- Generated pixels remain ignored and local. Synthetic tests cover mapping,
  cannon pointers, signed muzzle offsets, VRAM slots, layer order, and cropping.
- Next: enumerate verified standing frames, then run, jump, and attack with
  their per-frame cannon records.

## Patch 0024 - complete verified Samus idle cycle

- Extended the exact standing recipe across the four live animation records;
  the source sequence is `0, 1, 2, 1`, with a 16-update duration per record.
- Added one-command generation of `idle_0.bmp` through `idle_3.bmp` under the
  ignored SDL asset directory. The duplicate fourth image is intentional and
  preserves the original animation order.
- SDL now plays Samus's idle assets at `60 / 16 = 3.75` frames per second.
  Soma and unverified action previews retain their existing temporary cadence.
- Next: implement the verified running sequence and its per-frame arm-cannon
  OAM, then select a canonical midair and shooting sequence.
