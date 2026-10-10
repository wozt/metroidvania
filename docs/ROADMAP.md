# Roadmap

Every milestone follows the reconstruction principle in
[`ARCHITECTURE.md`](ARCHITECTURE.md#reconstruction-principle-permanent-architectural-constraint):
native resources are rebuilt from user-supplied ROMs, native behavior is ported
from the pinned decompilations, and project changes remain independent overlays.

The permanent priority is to complete the two original games independently.
Crossover gameplay, character adapters, world travel and combined balancing
remain deferred until both games can be completed natively from title screen to
credits.

## P0 - Inventory, coverage and reproducible reconstruction

- extend the automatic inventory from C/ASM definitions and direct calls to
  headers, data tables, structures, pointer tables and indirect call edges;
- keep automatic discoveries separate from reviewed associations and test
  results;
- migrate existing extractors into `python3 -m scripts.rebuild --all` with
  declared inputs, outputs, schema versions and deterministic invalidation;
- generate coverage statistics without treating function count as fidelity;
- add ROM-backed local fixtures and deterministic traces where redistribution
  rules permit them.

Current progress: both pinned decompilations are inventoried into stable routine
records, direct static call edges are resolved, reviewed feature annotations are
validated, and the parity checklist is generated. The initial task graph is
incremental and ROM-free. Header declarations, data symbols, indirect calls and
runtime evidence remain open.

Exit criterion: a clean checkout plus supported local ROMs can rebuild every
required private resource and report every known unported or unvalidated native
system without relying on historical caches.

## P1 - Complete Metroid: Zero Mission

- reproduce boot, title, menus, new game/load and pause flows;
- complete Samus's state machine, physics, equipment, weapons and effects;
- implement every room mechanism, entity, enemy, boss, event and cutscene;
- reproduce HUD, map, inventory, audio, saves, death/game over, endings and
  credits;
- validate timing, state transitions, collision, damage and progression against
  the native routines and reconstructed data.

Current progress: the experimental room runtime has a partial Samus controller,
native animation library, Clipdata collision, beams/missiles, doors, hatches and
room transitions. It is not a complete game and no broad category is marked
validated in the parity checklist.

Exit criterion: Zero Mission is normally completable from its native menu to
its credits without debug controls or emulation, with residual differences
explicitly measured and resolved.

## P2 - Complete Castlevania: Aria of Sorrow

- reproduce boot, title, menus, new game/load, pause, inventory and map flows;
- complete Soma's state machine, weapons, equipment, statistics, souls and
  progression;
- implement every room mechanism, enemy, boss, object, drop, event and
  cutscene;
- reproduce HUD, audio, saves, death/game over, final branches, endings and
  credits;
- extend shared enemy infrastructure only where the native routines actually
  share behavior.

Current progress: the experimental runtime covers bounded Soma movement and
combat states, several weapon classes, room transitions, wooden doors, bats and
zombies. Most native routines remain unnamed or unclassified and the original
game lifecycle is absent.

Exit criterion: Aria is normally completable through every native final branch
from its menu to its credits without debug controls or emulation, with residual
differences explicitly measured and resolved.

## P3 - Complete input and validation tooling

- provide full SDL3 keyboard/gamepad hot-plug input through device-independent
  actions for both games and every menu;
- add configurable bindings and persistent preferences;
- add an F1 debug menu to both runtimes with pause, frame stepping, state and
  entity inspection, teleportation, authentic equipment/ability toggles and
  collision/hitbox views;
- ensure debug state cannot contaminate normal saves;
- generate behavior-focused tests, frame traces and deterministic captures.

Exit criterion: either original game can be started, navigated, played and
completed with a controller, while every implemented subsystem can be inspected
through real runtime state.

## P4 - Native editor overlays

- preserve the current room, collision, entity, door, trigger and event editor
  work while the engines mature;
- finish immutable native references and versioned overlay schemas;
- add validated engine adapters only after the corresponding native behavior is
  understood;
- guarantee that removing an overlay restores the reconstructed original.

Exit criterion: edits for both games apply reproducibly above native resources
and behavior without modifying ROMs or generated source data.

## Deferred - Metroidvania crossover

Only after P1 and P2 are complete and validated:

- restore the parallel campaign, portal links and Interzone story work;
- add Samus-in-Aria and Soma-in-MZM adapters without weakening either engine;
- add character switching, companion AI, cross-equipment and soul mappings;
- validate softlock recovery, combined progression and crossover balancing.

## Production

- complete accessibility, localization, configuration and save migrations;
- audit licensing, packaging and clean-install reconstruction;
- ship only redistributable source/tools and require local compatible ROMs;
- publish releases only after explicit approval.
