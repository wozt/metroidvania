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

## Replacing the stubs

1. Embed a GBA runtime or static recompilation runtime inside the relevant
   backend.
2. Place VRAM/OAM/palette access, DMA, IRQ/VBlank, audio, and input behind a
   host abstraction.
3. Define an engine-specific snapshot contract and adapters to `SessionState`.
4. Replace one simulated room with a minimal authentic room, then adopt the
   native movement rules of that world.
5. Add the guest character without running the other engine.

Static recompilation is promising for Aria, but it must be reproduced locally
and evaluated against the noncommercial runtime license. No native runtime has
yet been selected for Zero Mission.

## Synergy extension points

`synergy_placeholder` reserves presentation state. Future hooks should be
explicit commands such as `support_attack`, `passive_tick`, and
`on_character_swap`, all executed by the active backend. No synergy buff,
attack, or healing behavior is implemented today.
