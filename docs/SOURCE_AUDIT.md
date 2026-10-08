# Source audit

Local observation date: **2026-10-08**. Required revisions are pinned as
submodules and have not been modified.

## Metroid: Zero Mission - `metroidret/mzm`

- URL: <https://github.com/metroidret/mzm>
- Revision: `43b7fd52f552e4d38c1521ff9d4df5ee57e61493`
- Revision date: 2026-08-23.
- License: MIT (`third_party/mzm/LICENSE`).
- Upstream status: work-in-progress decompilation, 2718/2721 functions (99.89%)
  and 100% of data outside blobs. These numbers come from the upstream README
  and were not independently recalculated.
- GBA build dependencies: `agbcc`, `binutils-arm-none-eabi`, Python, g++, and a
  base ROM; extraction uses `tools/extractor.py` before `make`.
- Documented US ROM: SHA-1 `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8`.

Verified entry points:

- main loop and modes: `src/agbmain.c`, `AgbMain`, and `InGameHandler`;
- in-game scheduling: `src/in_game.c`, `InGameHandler`, `VBlankCodeInGame`,
  and Samus/sprite/projectile/HUD updates;
- rooms: `src/room.c`, `RoomLoad`, `RoomLoadEntry`, `RoomLoadBackgrounds`, and
  `RoomUpdate`;
- player: `src/samus.c` and `include/samus.h`, including `SamusUpdate`,
  `SamusCheckCollisions`, and `SamusUpdatePhysics`;
- doors: `src/connection.c` and `src/color_fading.c`;
- saves: `src/save_file.c`, `src/save_file_load.c`, and
  `include/structs/save_file.h`;
- hardware: `src/dma.c`, `src/display.c`, register macros in `include/gba.h`,
  and VRAM/OAM transfers in `src/in_game.c`;
- audio: `src/audio.c`, m4a assembly, and data under `sound/`.

The C code is tightly coupled to GBA addresses, registers, global buffers, and
VBlank timing. Gameplay rules can be reused after abstraction, but this source
tree is not directly usable as a Linux library.

Verified runtime-state anchors from the byte-identical local build:

| Symbol | IWRAM address | Size |
|---|---:|---:|
| `gDifficulty` | `0x0300002C` | 1 byte |
| `gCurrentArea`, `gCurrentRoom`, `gLastDoorUsed` | `0x03000054` | 3 bytes |
| `gMainGameMode`, `gSubGameMode1` | `0x03000C70` | 4 bytes |
| `gSamusData` | `0x030013D4` | 32 bytes |
| `gEquipment` | `0x03001530` | 20 bytes |

The Samus subset uses offsets 0/1/2 for pose, standing status, and arm-cannon
direction; 14 for direction; 18/20 for quarter-pixel X/Y; and 22/24 for signed
velocities. The equipment subset contains maximum energy/missiles at offsets
0/2, maximum super missiles/power bombs at 4/5, current counts at 6/8/10/11,
and equipment flags at 12/14. These layouts were traced to the pinned
structures and confirmed through the matching ELF symbol table.

An input-free live trace reached demo gameplay at frame 3033 with mode 11,
submode 1, area 0, room 28, door index 60, position `(470,639)` in native
quarter pixels, energy `399/399`, and pose 0. Demo 2 stores that exact position
and overwrites the arrival calculated by `RoomReset`. Without the demo override,
Brinstar door 60 selects room 28 and the native formula yields `(288,511)`.
This is diagnostic evidence only; the adapter remains read-only.

## Castlevania: Aria of Sorrow - `testyourmine/cvaos`

- URL: <https://github.com/testyourmine/cvaos>
- Revision: `bc23d849d578c35ae12a5cec4e66549c3021a5be`
- Revision date: 2026-08-01.
- License: MIT (`third_party/cvaos/LICENSE`).
- Status: work-in-progress matching decompilation of the USA revision. The
  current README publishes no completion percentage, and many symbols still use
  address-based names, so semantic analysis is less advanced than in MZM.
- GBA build dependencies: `agbcc`, `binutils-arm-none-eabi`, a base ROM, the
  extractor, and `make`.
- Documented US ROM: SHA-1 `abd71fe01ebb201bcc133074db1dd8c5253776c7`.

Verified entry points:

- main loop and VBlank: `src/main.c`, `AgbMain`, `VblankInterrupt`, and
  `GameModeUpdate`;
- gameplay: `src/code_0800B700.c`, `GameModeInGameUpdate`;
- input and entities: `src/code_080009A0.c`, `SetPlayerInput`, and
  `EntityDeleteAll`;
- transitions: `src/code_08001194.c`, `CheckRoomTransition`;
- DMA and VRAM: `src/code_08001194.c`, `DmaQueue_Process`, and
  `BgCmdBuffer_TransferToVram`;
- saves: `src/code_08012744.c`, `SaveData_LoadSlotFromSram`,
  `SaveData_SaveSlotToSram`, and `src/agb_sram.c`;
- memory model: `include/structs/ewram.h` and `gEwramData`.

Large EWRAM aggregates, direct register access, and provisional symbols make a
direct source port risky. Static recompilation deserves an isolated proof of
concept before the backend is changed.

Verified runtime-state anchors from the byte-identical local build:

| Field | EWRAM address | Size |
|---|---:|---:|
| game mode and update stage | `0x02000010` | 2 bytes |
| in-game phase and phase stage | `0x02000064` | 2 bytes |
| current area and room | `0x0200009E` | 2 bytes |
| staged camera and player-local arrival | `0x02000334` | 8 bytes |
| staged room pointer | `0x020003CC` | 4 bytes |
| gameplay control flags | `0x0200A074` | 1 byte |
| background-1 camera X/Y | `0x0200A098` | 8 bytes |
| active player entity pointer | `0x02013110` | 4 bytes |
| character progression subset | `0x02013266` | 46 bytes |
| active player entity | dynamic verified slot | 112-byte subset |

The matching ELF places the `gEwramData` pointer at ROM address `0x084F0B14`;
its initialized value is `0x02000000`. The progression subset exposes current
character, equipment, level, signed current HP/MP, maxima, experience, and
gold. The player pointer must be aligned to a `0x84`-byte slot in the 224-entry
entity array at `0x020004E4`. The decoded entity subset includes its 16.16
position and velocity plus animation ID, frame, counter, and flags.

The source's `GetEntityRoomXPositionWhole` and
`GetEntityRoomYPositionWhole` add the background-1 camera position to the
entity-relative coordinates. The adapter performs the same fixed-point
addition. A deterministic A/Start boot trace reached normal in-game phase 1
with the player-control flag enabled at frame 1022: mode 4, area 0, room 0,
room position `(0x00A80000,0x02BF0000)`, HP `320/320`, MP `80/80`, level 1,
and animation 81 frame 0.

The same trace retained the native staged arrival for area 0 room 0: room
pointer `0x0850EF9C`, camera `(32,512)`, and player-local `(120,141)`. The
pointer is entry zero in the area-zero table reached through `sUnk_0850EF08`.
`sub_0800EBE0` consumes this record by selecting the room, restoring player-local
coordinates, and applying the camera origin. The resulting arrival coordinate
is `(152,653)` before later gameplay or cutscene movement.

## Reproducible upstream build check

Both pinned source trees were cloned into temporary workspaces, leaving the
submodules untouched. Toolchain used:

- Debian `binutils-arm-none-eabi` 2.44;
- `jiangzhengwenjz/agbcc` revision
  `59b966ed1b8f371856dcf99f1546c2fe89c678ca`, built locally outside this
  repository because it has no clear root license file and includes GCC code.

After local data extraction, both builds completed successfully. The generated
8 MiB files were byte-for-byte identical to the user's reference ROMs:

- `cvaos_us.gba`: `abd71fe01ebb201bcc133074db1dd8c5253776c7`;
- `mzm_us.gba`: `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8`.

Generated ROMs and extracted assets remained in `/tmp`; none were copied into
or tracked by this repository.

## Additional runtimes and references audited

### `mgba-emu/mgba`

- Official project: <https://github.com/mgba-emu/mgba>.
- Evaluated and integrated Debian library version: 0.10.5+dfsg-1.
- License: Mozilla Public License 2.0. The project uses the system shared
  library; no mGBA source is copied into this repository.
- The public `mCore` API was verified with both supported ROMs: core discovery,
  32-bit video-buffer attachment, ROM loading, reset, input, frame execution,
  and teardown all work without a BIOS file or automatic save loading.
- After 300 input-free frames, Zero Mission produced 38,129 nonzero pixels and
  hash `5ca363153253eb90`; Aria produced 38,400 and
  `ce393be405213e55`. Both frames are 240x160. The repository command
  `fusion_dev --authentic-probe` reproduces this trace locally after ROM
  validation.
- mGBA is an interim emulated execution route, not evidence that either engine
  has been natively ported. Its project-owned adapter now implements the same
  exclusive backend lifecycle as the diagnostic engines.
- The public fixed-size `stateSize`, `saveState`, and `loadState` operations were
  exercised in memory for both supported ROMs. Version 0.10.5 returned 397,312
  bytes. Replaying 30 frames after restore reproduced the pre-restore forward
  hash for both games. This is a same-process determinism result, not a portable
  savestate compatibility claim.

### `sergiomanzur/ariaOfSorrow-recomp`

- Observed revision: `f00abd91ee8338b4378e279e1aa4a5a3a8ab8525`
  (2026-09-18).
- Its ahead-of-time Linux build was reproduced locally from the user's ROM.
  The cartridge pass emitted 12,736 functions, and a 300-frame cold run after
  recompiling the generated clean-room BIOS reported zero dispatch misses and
  zero interpreted instructions. A TCP capture returned a nonempty 240 x 160
  framebuffer. All 36 configured upstream tests passed.
- The placeholder BIOS build did use the interpreter for three BIOS addresses,
  so the clean-room BIOS generation step is mandatory for the zero-fallback
  result.
- Its root checkout has no license file covering the host code. It depends on
  `gbarecomp` under PolyForm Noncommercial 1.0.0, plus SDL2 and C++20. The
  current project's GPL-3.0-only license does **not** match this noncommercial
  runtime: incorporating it into a combined GPLv3-only deliverable is blocked
  pending a compatible grant or a separate GPL-compatible implementation.
  Unlicensed root frontend code remains excluded.
- Full commands, revisions, counters, limitations, and integration gates are
  recorded in [`ARIA_RECOMP_EVALUATION.md`](ARIA_RECOMP_EVALUATION.md).

### `LTSchmiddy/metroid-zero-mission-pc-edition`

- Observed revision: `25cd4a1448ba24fa5b04b314afa51fca25e41b13`
  (2020-07-28).
- Its README confirms that game data is integrated into VBA-M/SDL2. This is an
  emulator wrapper/modification, not a native port of the decompiled code.
- The VBA-M base uses GPLv2 with additional components. The repository also
  contains asset directories and will not be imported here.

## Priority unknowns

- Final native or recompiled runtime strategy for each game beyond the mGBA
  integration proof.
- Loader triggers and their complete dependent state for MZM door 60 and the
  Aria staged-room record. The native descriptors and target-side rollback
  semantics are defined, but neither authentic apply path is enabled.
- Source-runtime recovery and backend lifecycle composition around the target
  transaction; target rollback alone does not make a complete world switch.
- Licensing and redistribution status of code generated from a ROM; legal
  review is required before distribution.
