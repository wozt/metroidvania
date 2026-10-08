# Architecture

## Core rule

The project never has two active engines. The session controller calls
`leave_world` on the current backend, updates `active_world`, then calls
`enter_world` on the other backend. Only the active backend receives `tick` and
`render` calls.

The interface in `include/core/backend.h` exposes `init`, `enter_world`, `tick`,
`render`, `leave_world`, and `shutdown`. Implementations live under
`src/backends/metroid/` and `src/backends/castlevania/`. Shared
`room_sim.c` code is only a test harness; it is not a unified engine or a
reimplementation of either game.

The same interface now carries held GBA controls and backend failure state.
`src/backends/gba/gba_backend.c` owns each authentic runtime and its SDL3
streaming texture. The controller chooses either two simulated backends or two
authentic backends at startup, then uses one common lifecycle and render loop.

## State model

`SessionState` separates three categories:

- per-character state: HP, availability, inventory, and abilities;
- per-world state: position, velocity, target, door, and visitation state;
- explicitly shared state: map, boss flags, schema version, and placeholder
  synergy gauge.

A character swap preserves the room instance. The new profile is checked
against solid geometry; a stable list of offsets is tried before rejecting an
unsafe swap. HP is never copied between characters.

Save files start with the `FUSION1` signature, a version, and payload size. The
current format is ABI-dependent and intended only for the prototype. Public
compatibility will require explicit encoding, checksums, and migrations.

## ROM access

`rom_validate` reads each local file in blocks and computes SHA-1. No ROM byte
is uploaded. Startup stops before SDL initialization unless both supported USA
revisions match. The next resource layer should expose bounded, read-only
`RomView` objects and versioned local extractors without writing proprietary
data into the repository.

The opt-in authentic proof uses one project-owned `GbaRuntime` adapter per ROM
around the system mGBA library. Both cartridges can remain loaded, but only the
runtime selected by `SessionState.active_world` is entered and stepped. A world
switch marks the current runtime inactive before entering the other. Automatic
save loading is deliberately absent, so this proof neither reads nor writes a
game save. Its 32-bit 240x160 framebuffer is copied into an SDL3 streaming
texture; no extracted frame is stored in the repository.

This is an emulated execution path used to unblock authentic frame, input, and
lifecycle work. It does not make mGBA the final gameplay architecture and does
not replace the native-source/recompilation investigation.

## Authentic snapshot boundary

The shared backend contract has optional `capture_state` and `restore_state`
operations. Authentic snapshots carry a schema version, the owning world, and
an opaque mGBA state buffer. Capture and restore are accepted only while that
runtime is active at a frame boundary. Restore rejects a different world,
schema, empty buffer, oversized buffer, or any size other than the current
mGBA core's exact state size. Ownership is explicit and ends through
`fusion_backend_snapshot_dispose`.

Snapshots are memory-only and valid only inside the current process. They are
not embedded in `SessionState`, accepted by F5/F9, or treated as a stable file
format. This prevents diagnostic fields from masquerading as authoritative game
state and avoids writing ROM-derived working memory before a persistence policy
exists.

## Replacing the stubs

1. Identify verified engine fields inside the controlled mGBA snapshots.
2. Place controlled VRAM/OAM/palette and memory access behind a backend-owned
   host abstraction.
3. Add adapters between verified engine state and `SessionState` without
   pretending the diagnostic state is authoritative.
4. Select one authentic room, then adopt the native movement rules of that
   world.
5. Add the guest character without running the other engine.

Static recompilation remains a promising Aria research route, but the reproduced
PolyForm runtime is blocked from this GPLv3 project. No final native runtime has
yet been selected for either game.

## Synergy extension points

`synergy_placeholder` reserves presentation state. Future hooks should be
explicit commands such as `support_attack`, `passive_tick`, and
`on_character_swap`, all executed by the active backend. No synergy buff,
attack, or healing behavior is implemented today.
