# Project status

Last updated: **2026-10-08**.

## Fixed vision and decisions

The project targets a real crossover with two distinct engines and one active
backend at a time. Personal USA ROMs are mandatory and remain local. Upstream
sources are isolated as submodules. Linux/SDL3 is the initial target; physical
GBA support is a later, unproven feasibility study. All project commits,
documentation, comments, UI text, and diagnostics are written in English.
Original project-authored code and documentation use GPL-3.0-only.
PolyForm-licensed `gbarecomp` is research-only and blocked from integration.
The MPL-2.0 mGBA system library provides an interim authentic execution path.

## Actual state

| Item | Status | Evidence or limitation |
|---|---|---|
| Validation of both ROMs | Done | Python and C SHA-1 checks; rejection before SDL |
| `mzm` and `cvaos` sources | Done | Pinned submodules and targeted audit |
| Matching upstream GBA builds | Done | Both generated ROMs match references byte for byte |
| Two-backend interface | Done | Simulated and mGBA implementations share the exclusive init/enter/tick/render/leave/shutdown lifecycle |
| Two diagnostic rooms | Done | SDL3, distinct geometry, ROMs required |
| Four character/world profiles | Done, simulated | Separate parameters, not original physics |
| HP, KO, and persistence | Done | Core tests and version-1 save data |
| Collision-safe character swap | Done | Deterministic offsets and rejection test |
| HUD, debug, and synergy | Partial | Dual HP and overlay; synergy explicitly a placeholder |
| Authentic Aria execution | Emulated proof integrated | mGBA executes the verified ROM with input and 240x160 output; native/AOT route remains unresolved |
| Authentic MZM execution | Emulated proof integrated | mGBA executes the verified ROM with input and 240x160 output; native source-port route remains unresolved |
| Authentic state boundary | Memory-only proof | Versioned/world-typed 397,312-byte snapshots replay deterministically; no persistent format |
| Verified MZM state view | Read-only proof integrated | Exact IWRAM symbols expose mode, location, Samus movement/pose, and equipment without mutating runtime or `SessionState` |
| Verified Aria state view | Read-only proof integrated | Exact EWRAM offsets expose mode, location, player movement/animation, progression, and equipment with validated entity indirection |
| Transition observation | Read-only projection integrated | Source-local location/position and character-owned health use an explicit versioned schema; no importer or shared field yet |
| Transition plan | Native descriptors integrated | MZM door 60 and Aria's staged Entrance record replace sampled coordinates; authentic backends expose no apply operation |
| Original assets, maps, and audio | Partial, local-only | Verified Samus and Soma idle/run/jump/attack recipes; no extracted asset is tracked |
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
./build/fusion_dev --authentic-probe
./build/fusion_dev --authentic-arrival-preview
SDL_VIDEODRIVER=dummy timeout 2s ./build/fusion_dev --authentic-video-test
```

Normal and AddressSanitizer/UndefinedBehaviorSanitizer builds passed all 12 CTest
targets, and all 60 Python tests passed. Both ROMs were accepted. The SDL smoke
test remained active for two seconds and was stopped by `timeout` with the
expected status 124 and no runtime error.

The mGBA runtime adapter adds an eighth CTest target, its backend adapter adds a
ninth, the pure MZM and Aria decoders add the tenth and eleventh, and the
transition observation/planning/transaction contract adds the twelfth. The
runtime and backend tests cover ROM-free
failure paths. The local authentic probe
executed each validated ROM for 300 frames and reproduced hashes
`5ca363153253eb90` (Zero Mission) and `ce393be405213e55` (Aria). An SDL
dummy-driver authentic-mode smoke test remained active for two seconds and was
stopped by `timeout` with status 124.

The extended authentic probe captured each runtime at frame 300, advanced 30
frames, restored the snapshot, and replayed the same 30 frames. Zero Mission
reproduced hash `d2bee3834e3048a7`; Aria reproduced
`ce393be405213e55`. Both mGBA state buffers were 397,312 bytes.

The extended Zero Mission trace then reached a gameplay-ready demo state at
frame 3033: mode 11, submode 1, area 0, room 28, native position `(470,639)`
(truncated display position `(117,159)`), energy `399/399`, and pose 0. The
values come from a bounded read-only WRAM view matched to the pinned source and
exact local build.

The deterministic Aria boot trace supplied periodic A/Start pulses without
loading a save and reached a gameplay-ready state at frame 1022: mode 4, stage
0, normal in-game phase 1, area 0, room 0, 16.16 room position
`(0x00A80000,0x02BF0000)`, HP `320/320`, MP `80/80`, level 1, and animation 81
frame 0. The player-control flag was enabled, and the player pointer was
validated as an exact entity-array slot before its fields were read.

The checkpointed arrival preview moved native Samus from quarter-pixel
coordinates `(470,639)` to door 60's `(288,511)` and native Soma from room
position `(168,703)` to the Entrance record's derived `(152,653)`. Both values
remained exact after one engine frame, and the Aria position remained exact for
the 180 additional frames used to clear the entry pixelation. Each runtime was
then restored to its original position from its process-local checkpoint. Five
authentic 240x160 BMPs were written under the ignored
`captures/arrival-preview/` directory; no save or extracted asset was produced.

Both live states projected into the same read-only Q16.16 schema. MZM produced
source room `0:28`, position `(0x00758000,0x009FC000)`, and Samus health
`399/399`; Aria produced source room `0:0`, position
`(0x00A80000,0x02BF0000)`, and Soma health `320/320`. Location and position are
marked source-local, health is character-owned, and the shared-field mask is
empty.

Each observation also produced a version-2 plan for the opposite world. Samus
mapped to Aria Entrance room `0:0` through room pointer `0x0850EF9C`, camera
`(32,512)`, and player-local `(120,141)`, producing target position
`(0x00980000,0x028D0000)` with health `399/399`. Soma mapped to MZM Brinstar
room `0:28` through door 60, whose native formula produces
`(0x00480000,0x007FC000)` with health `320/320`. The probe only prints these
plans; neither authentic backend implements their target operations.

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
The default diagnostic backends remain integration boundaries and test doubles.
The opt-in mGBA path now executes original ARM code and renders the ROM's real
output, but it is still an emulated proof rather than the final native backend
strategy. It now runs through the shared backend contract and common controller
loop. No proprietary content is committed.

## Risks and next actions

1. **P0 - Loader trigger research:** verify the complete dependent state needed
   to invoke MZM and Aria room loading safely from each selected descriptor.
2. **P0 - Native strategy:** evaluate source HAL or compatible recompilation
   paths while keeping mGBA as the verified baseline.
3. **P1 - Controller transaction:** compose target rollback with source
   snapshot restoration and exclusive backend lifecycle recovery.
4. **P1 - Tests:** add frame traces, render captures, and real lifecycle tests.
5. **P2 - Content:** implement guest-character adaptations, progression, and
   synergy hooks.

## Open, non-blocking questions

- Can each native loader be entered through existing engine state alone, or is
  a backend-owned call gate required?
- What resurrection policy and progression flags should be shared?

## Next milestone acceptance criteria

- startup remains gated by both SHA-1 checks;
- one authentic frame from the Aria or MZM ROM, with no committed asset;
- the opposite backend remains fully suspended;
- reproducible capture and trace;
- explicit documentation of native, recompiled, interpreted, or emulated paths
  and every fallback;
- existing automated tests remain green and the license audit is updated.

These criteria are satisfied by the interim emulated proof. Authentic execution
is bound to the backend interface and now has a controlled, memory-only snapshot
boundary. The MZM and Aria state views and read-only ownership projection are
complete. Destination-specific plans and target rollback semantics are also
defined and tested without an authentic importer. A 16-byte, WRAM-only,
checkpoint-required diagnostic write now proves the selected coordinate and
rollback inside each already-loaded room. The next milestone is verifying
loader triggers, engine-native target adapters, and full lifecycle recovery,
not persistent savestates or unguarded memory writes.

## Patch 020 - checkpointed authentic arrival preview

- Added an opt-in local preview that reaches each verified gameplay state,
  captures its native framebuffer, injects the selected position and zero
  velocity, advances one engine frame, and captures the result.
- Added a later Aria context frame so the already-loaded Entrance room remains
  inspectable even though its first gameplay-ready frame is mid-transition.
- Restricted diagnostic writes to 16 bytes in EWRAM or IWRAM and required a
  matching in-memory runtime checkpoint before every write.
- Restored and verified each original engine position before closing the
  runtime. The path cannot write saves, mutate `SessionState`, or run through
  an authentic backend importer.
- Exposed Aria's decoded camera origin so the Entrance absolute coordinate can
  be previewed without replacing the live camera.
- Kept all ROM-derived BMP captures under ignored `captures/`; no proprietary
  data is committed.

## Patch 019 - engine-native arrival descriptors

- Replaced sampled target coordinates with a version-2 plan carrying each
  engine's native room-entry representation.
- Selected MZM Brinstar door 60 and derived its room-28 arrival through the
  source `RoomReset` formula; confirmed the live demo uses door 60 before its
  saved Samus state overrides that position.
- Added a bounded Aria read of the staged room pointer, camera origin, and
  player-local coordinates. The authentic trace reproduced the exact Entrance
  record and its area-zero room-table pointer.
- Made the authentic probe reject either plan if the live door or staged-room
  evidence diverges from the selected descriptor.
- Kept both authentic backends read-only. Loader triggers and their complete
  dependent state remain the next gate.

## Patch 018 - candidate anchors and target transaction

- Added versioned plans that preserve the source character and health while
  replacing source-local coordinates with an explicit destination anchor;
  zero-health observations are rejected without inventing resurrection policy.
- Registered the exact MZM Brinstar and Aria Entrance points reproduced by the
  authentic probes as candidates, without claiming door or collision safety.
- Added a target transaction contract with preflight, checkpoint, apply,
  verification, rollback, and checkpoint disposal.
- Covered clean rejection, commit, partial-apply rollback, verification
  rollback, and rollback failure with a synthetic target. Authentic backends
  still expose no mutation callback.

## Patch 017 - read-only transition observations

- Added a versioned common observation for source world/character, source-local
  room and position, and character-owned health.
- Converted MZM quarter-pixel position to Q16.16 exactly and copied Aria's
  native Q16.16 position without normalization loss.
- Rejected non-ready views, inconsistent field ownership, invalid health,
  unsupported Aria Julius state, and incompatible schema versions.
- Extended the authentic probe to project both live verified states. No
  importer, runtime write, `SessionState` mutation, or implicit shared field was
  added.

## Patch 016 - verified Aria state view

- Added a pure decoder for verified Aria mode, location, player entity,
  animation, movement, progression, equipment, HP/MP, experience, and gold.
- Validated the dynamic player pointer against exact entity-array bounds and
  alignment before following it.
- Reconstructed room position with the source's camera-plus-entity 16.16
  calculation and exposed the read-only view through the Castlevania backend.
- Extended the authentic probe with deterministic A/Start input to reach and
  report a real initialized Aria state without loading or writing a save.

## Patch 015 - verified Zero Mission state view

- Added an active-runtime-only memory reader limited to bounded EWRAM/IWRAM
  ranges; ROM and hardware I/O reads are rejected.
- Added a pure decoder for verified Zero Mission mode, location, Samus, and
  equipment layouts plus separate plausibility and gameplay-readiness checks.
- Exposed the view through the authentic Metroid backend without writing
  emulated memory or `SessionState`.
- Extended the authentic probe to reach and report a meaningful live MZM state;
  decoder and failure paths remain ROM-free under CTest.

## Patch 014 - memory-only authentic snapshots

- Added bounded mGBA capture/restore operations and a versioned, world-typed
  snapshot in the shared backend contract.
- Cross-world restoration is rejected; diagnostic backends deliberately expose
  no snapshot implementation.
- The authentic probe now proves deterministic 30-frame replay for both ROMs.
- Snapshots remain process-local, are disposed explicitly, and are never
  connected to F5/F9 or written to disk.

## Patch 013 - authentic backend lifecycle

- Replaced the special authentic loop in `main.c` with project-owned mGBA
  implementations of the shared `FusionBackend` contract.
- Held GBA controls and backend error propagation now cross the common
  controller boundary.
- Each authentic backend owns its runtime and SDL texture. World switching
  always calls `leave_world` before `enter_world`, and only the selected backend
  receives `tick` and `render`.
- Added ROM-free backend failure tests. A local two-ROM lifecycle test executed
  and rendered 60 Zero Mission frames, suspended that backend, then executed
  and rendered 60 Aria frames without an error.

## Patch 012 - authentic mGBA execution proof

- Added a project-owned adapter around the MPL-2.0 mGBA system library.
- Both SHA-1-validated ROMs produce authentic 240x160 frames and accept native
  GBA keypad input; automatic save handling is not enabled.
- `--authentic-video-test` maintains exactly one active runtime and switches
  worlds with `M`. `--authentic-probe` produces a reproducible 300-frame trace.
- Default simulated behavior is unchanged, no ROM-derived bytes are tracked,
  and the PolyForm/unlicensed implementations remain excluded.

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
- Added SHA-1-gated extraction for the four-frame idle cycle on a stable 18x34
  canvas, the 17-frame run loop on a stable 28x33 canvas, the 12-frame normal
  jump on a stable 29x36 canvas, and the 11-frame body-plus-knife attack on a
  stable 51x34 canvas. Live research captures matched all
  2,048 staged tile bytes for idle frames 12 and 13, run frame 99, and jump
  frame 118. Knife frame 12 matched all 128 staged tile bytes and its OBJ
  palette bank 1; Soma's body palette matched OBJ palette bank 0. The runtime
  used for comparison remains research-only and is not integrated.
- SDL asset discovery now uses the exact Soma idle/run/jump/attack durations.
- Movement animations 26 (start, 3 frames / 8 updates) and 25 (stop, 9 frames /
  86 updates) are extracted and represented by one-shot transitions around the
  continuous run loop.
- Attack animation lifetime is independent from the target-hit flash. Soma now
  displays its complete 48-update attack and Samus its complete 8-update attack,
  including misses and attacks after the diagnostic target is destroyed.
- Extracted pixels remain ignored and are never committed.

## Patch 0023 - Aria native loader timeout diagnostics

- Retained the exact native arrival verification and bounded loader timeout.
- On Aria loader timeout, capture the final verified state view (game mode,
  phase, readiness, room, coordinates, staged arrival) and expected coordinates.
- Failed transitions still invoke the existing checkpoint rollback contract;
  diagnostic output is not a substitute for verifying loader trigger behavior.

## Patch 0025 - bounded Aria loader state trace

- Added read-only loader tracing on Aria mode, phase, stage, and control
  changes, with samples every 60 frames. This targets the observed Aria
  phase 3:1 stall with null player coordinates at frame 600.
- No changed loader trigger, frame limit, transaction semantics, or
  checkpoint rollback behavior. The trace is diagnostic only.

## Patch 0026 - guarded Aria same-room arrival

- A local loader trace reached Aria phase `3:1` with a null player pointer,
  and stayed there through frame 600. The pinned `cvaos` implementation
  of `GameModeInGameUpdate` calls `sub_080104EC` in phase 3; its stage-1
  handler `sub_08010350` expects a live player entity.
- The original code forced phase `0:2` after the destination Entrance room
  had already bootstrapped. That is not a proven room-loading contract.
- For a verified, **already-loaded** Entrance room, the target now uses the
  checkpointed 16-byte player position/velocity write demonstrated by
  `authentic_preview.c`, followed by bounded health writes, one emulated
  frame, state verification, and the existing transactional rollback path.
- Incompatible rooms, uninitialized players, mismatched arrival descriptors,
  and camera-incompatible positions are rejected without writes. This does
  **not** implement general native cross-room loading or guest characters.
- Added a ROM-free regression test for the same-room gate.

## Patch 0027 - verify stable Aria in-room arrival state

- Keep the strict Entrance descriptor and valid player checks **before** any
  checkpointed write.  Staged-room fields are transient loader inputs, not
  guaranteed post-frame invariants.
- After in-room placement and HP transfer, validate the stable entity, room,
  exact Q16.16 position and health each frame; allow up to 240 frames for
  temporary loss of player control before requiring gameplay readiness.
- Reject any invalid state immediately with actual observed values.  The
  existing transaction rollback remains mandatory; cross-room loading is
  still unsupported. No ROM-derived bytes are added to Git.

## Patch 028 - bounded Aria entrance input probe

- The Aria Entrance same-room arrival still requires a checkpoint, valid
  character entity, exact target location, position, HP, and restored control.
- The initial gameplay startup may relinquish control after its first ready
  frame. Reuse the already existing Entrance preview's sparse A-input cadence
  (one frame every 90 frames) only while controls are disabled; never hold A.
- Retain a bounded 540-frame settling window and reject any invariant drift.
  A missing ready state still fails the transaction and triggers rollback.
- Emit bounded periodic state/animation traces. This is a diagnostic hypothesis,
  not a claim that the input pulses fix the engine's control flag.
- Added pure unit assertions for the input schedule.

## Patch 029 - sustained authentic Aria bootstrap

- The Aria target bootstrap no longer treats a single frame with the player
  control bit set as proof of a stable, playable destination.
- Before any target transaction, the unmodified Aria runtime must remain
  gameplay-ready in the verified Entrance room for 90 consecutive frames.
- The probe reports the first ready frame, the longest stable interval,
  and the final phase/control/animation when stabilization never occurs.
- This is a diagnostic and safety-gating change, not a claim that the Aria
  introduction or native cross-room loading is fixed. No ROM data is written.

## Patch 0030 - exclusive authentic round-trip lifecycle proof

- Added `--authentic-roundtrip-probe` as a separate, ROM-validated diagnostic.
- Bootstrap both worlds independently, then execute the ordered sequence
  MZM departure -> suspended MZM -> checkpointed Aria same-room arrival ->
  resume original MZM -> restore both initial runtime checkpoints.
- Enforce exclusive active runtime state at every handoff, compare **complete**
  MZM mGBA snapshots across the period when it was suspended, and prove that
  one MZM frame can run after re-entry before restoring its checkpoint.
- Compare the Aria outer checkpoint after undoing a committed arrival, in
  addition to its native position/HP view. Any failure initiates cleanup
  restoration for every checkpoint already captured.
- Added ROM-free tests for exclusive lifecycle gating and byte-accurate
  snapshot equality. The command never writes ROMs, saves, or assets.
- **Scope limit:** Aria still uses its native Soma entity. This is a proof of
  runtime suspension, arrival, and return, not a playable guest-character
  crossover or arbitrary cross-room loader.

## Patch 0031 - replay-verified Aria outer checkpoint restoration

- The first ROM-backed `--authentic-roundtrip-probe` succeeded in entering
  Aria, importing `399/399` HP, resuming MZM byte-for-byte unchanged, and
  running/restoring one MZM frame. It then failed the Aria outer checkpoint
  comparison, without identifying whether serialization or gameplay diverged.
- Retain serialized byte comparison as a reported diagnostic, not the only
  post-loadState acceptance criterion. Require exact decoded Aria state fields
  and sixteen frame-by-frame full-framebuffer hashes from identical checkpoint
  restores (one baseline before arrival and one after the round trip).
- Reject a changed decoded state, replay-frame divergence, failed restore, or
  lost exclusive runtime ownership. Rewind the baseline before applying the
  transaction and rewind Aria again after replay; cleanup restores both saved
  checkpoints on every exit path. No ROM or proprietary bytes are written.
- This change has ROM-free tests for decoded-state comparison and framebuffer
  hashing; whether the original mismatch is harmless mGBA serialization
  normalization is a hypothesis until the real-ROM replay passes.
- The target still uses a native Soma entity and a verified same-room arrival.
  Guest Samus and arbitrary room loading remain unimplemented.
