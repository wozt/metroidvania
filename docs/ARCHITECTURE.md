# Active architecture

## Product boundary

The final game is intended to be a native C11/SDL3 application with two
separate gameplay engines: Zero Mission rules in Zebes and Aria rules in the
castle. A shared layer will own input, rendering presentation, persistence,
story events, cutscenes, party state and cross-world progression.

Neither native gameplay engine is complete. The active repository contains
source-data reconstruction and editor infrastructure plus an experimental SDL3
Zero Mission room runtime. That runtime reproduces Samus's movement, weapons,
native collision types, doors and hatches, but has no enemies, events or
complete room lifecycle yet. It is not a simulated substitute or an emulated
frontend.

## Reconstruction principle (permanent architectural constraint)

This section is the reference for how MetroidVania is meant to be built. It
describes a target; the "Current state" list says what exists today.

### The ROMs are the source of truth

The player supplies legally obtained *Metroid: Zero Mission* and *Castlevania:
Aria of Sorrow* USA ROMs. The finished software must, without modifying them:

1. verify the ROMs and identify supported versions;
2. extract and rebuild every native resource that can be recovered faithfully:
   maps, tiles, sprites, animations, palettes, collision, doors, objects,
   enemies, events, audio;
3. run both worlds through re-implemented native engines;
4. apply the project's editor modifications on top of that reconstruction;
5. apply MetroidVania-specific behaviors and mechanics;
6. produce one coherent, playable game.

Nothing that can be reconstructed from a ROM is recreated by hand. Extraction
recovers *data* only: native *behavior* (physics, AI, pose logic, event
scripts) is re-implemented from evidence in the pinned decompilations and the
ROMs, and each port names the routine it reproduces. Data extraction and
engine reconstruction are separate concerns and are tracked separately.

### Layers

Content and behavior are resolved as ordered, independent layers:

| Layer | Origin | Ownership |
|---|---|---|
| Native resources | rebuilt from the ROMs | private generated cache, never committed, never edited |
| Native engines | MZM and Aria kernels ported from the decompilations | project source code |
| Project data | MetroidVania story, links, crossover content | tracked project files |
| Editor overlays | user modifications of native or project data | versioned project files |
| Gameplay extensions | new mechanics grafted onto one engine | project source code |
| Character adapters | Samus in Aria rules, Soma in MZM rules | project source code |
| Shared systems | input, saves, story events, cutscenes, party, travel | project source code |

Overlays are non-destructive: moving an enemy records only the new placement
while the native one stays available; adding an event complements native
events; overriding a native property never alters the source record; removing
an overlay restores native content or behavior. Overlays may target data and,
through declared extension points, behavior. They must be versioned,
validated, reproducible and independent of cache paths.

### Two engines, shared infrastructure

The Zero Mission engine keeps Metroid physics, collision, room, enemy and
event handling; the Aria engine keeps Castlevania's. They share tools,
interfaces and common systems but are not merged into a generic engine. New
mechanics attach to the engine they belong to without degrading its fidelity,
and each character runs under the active world's rules with its own
abilities through an explicit adapter.

### Target reconstruction pipeline

```text
ROMs -> verification -> native extraction -> data reconstruction
     -> overlay application -> engine initialization -> playable game
```

Requirements: complete rebuild from a clean installation; incremental rebuilds;
generated caches that are never the source of truth; explicit dependencies
between resources; stable references to native elements (for example
`mzm:brinstar:033`, native door and table indices, decompilation symbols) so
overlays never depend on cache paths; validation of overrides with
incompatibility detection; strict separation of redistributable project files
from locally extracted proprietary data; deterministic output for identical
ROMs, engine versions and overlays.

This is a direction for every new development, not a request for a rewrite:
existing tools are extended toward it as real needs appear.

### Current state

- Implemented: ROM fingerprint checks; deterministic generators for Samus
  sprites/palettes/projectiles, Soma animations and every MZM runtime room
  (background, Clipdata types, doors, hatches) under `assets/extracted/`;
  stable native room identifiers and per-room private override documents in the
  editor; `scripts/check_no_proprietary.py` guarding the repository.
- Experimental: the Zero Mission runtime (Samus, weapons, collision, doors) and
  the editor's project room documents, which already store overrides
  separately from native data but have no ROM or engine encoder yet.
- Not implemented: a single orchestrated rebuild command, incremental
  dependency tracking, overlay application inside the engines, behavior
  extension points, the Aria engine, character adapters and shared runtime
  systems.

## Active C components

`fusion_native_editor_core` contains the project-owned native map model,
selection operations and display-independent CLI launcher:

- `src/core/native_map.c`: versioned `MVNATIVE 1` room work files, editing,
  history and atomic override persistence;
- `src/core/native_selection.c`: bounded rectangular metatile selection moves.
- `src/core/editor_cli_launcher.c`: replaces the process with the versioned
  shared Python backend; no GTK initialization occurs on this path.
- `src/tools/native_map_cli.c`: internal display-independent adapter around the
  exact `native_map_load/edit/save` implementation used by GTK. The public CLI
  uses it for BG1/BG2 inspection, reads, writes and flood fills.

`fusion_aria_runtime` (`src/runtime/aria_runtime.c`) is the experimental
SDL3 Aria runtime: one exported room, Soma's sprite library as animation
timings and frames, and the SDL-free ports `aos_collision.c`, `aos_soma.c`
and `aos_anim.c`.

`fusion_room_runtime` is the experimental SDL3 Zero Mission room runtime:

- `src/runtime/mzm_samus.c`: SDL-free Samus pose controller ported from the
  pinned decompilation (pose handlers, `SamusSetMidAir`/landing/hurt carries,
  native physics constants, per-pose block hitboxes, animation-driven
  transitions). It talks to the room through a box-collision callback and to
  the animation registry through a frame-duration callback.
- `src/runtime/mzm_projectiles.c`: SDL-free weapon selection and Power Beam,
  missile and super missile projectiles ported from the decompilation.
- `src/runtime/room_runtime.c`: room/Clipdata loading, input mapping, the
  semantic animation registry, the catalogue browser and SDL rendering.

`fusion_editor_cli` is the native headless entry point. `fusion_map_editor
--headless` reaches the same backend before creating a `GtkApplication`.

The GTK4 executable is assembled from:

- `editor/main.c`: responsive/detachable native workspace shell;
- `editor/native_workspace.c`: asynchronous room import, native BG1/BG2 editor,
  collision/entity/door/event overlays, room-data inspector, metatile palette
  and safe document lifetime management;
- `editor/world_atlas.c`: original-coordinate maps and room launcher;
- `editor/object_catalog.c`: read-only shared Zero Mission/Aria object catalog.

`fusion_mzm_room_viewer` is a small SDL3 viewer for locally rendered MZM room
BMPs. It is a research utility, not the future game runtime.

## Local data pipeline

Python tools validate exact USA ROM fingerprints before reading them. They use
pinned decompilation symbols and audited ROM offsets to produce private output
under `assets/extracted/`.

New private generators use three stable roots:

- `metroid/` for Zero Mission raw data, rooms, maps and native sprites;
- `aria/` for Aria raw data, rooms, maps and native sprites;
- `shared/` for manifests and diagnostics that describe both worlds.

`scripts/asset_layout.py` is the path contract. The general importer writes raw
blocks and basic character previews below their world root and writes its
manifest below `shared/manifests/`. `scripts/mzm_samus_pipeline.py` writes the
canonical content-addressed Samus runtime library below
`metroid/sprites/samus/runtime/`; repeated pixels are stored once by SHA-256.
One invocation composes every native animation directly from the verified
MZM ROM, the pinned decompilation pointer tables and the matching reference
ELF through `scripts/mzm_samus_compose.py`, which mirrors
`SamusUpdateGraphicsOam` (body, arm cannon animation and arm cannon graphics
selection) and `SamusUpdatePalette` (suit palettes). Each frame records its
native draw offset relative to Samus's position, and the runtime index uses the
shared, versioned `metroidvania-sprite-index-v1` schema of `scripts/sprite_library.py`. Objects no longer
referenced by the index are pruned; no intermediate cache is kept.
The generated `animation_map.tsv` resolves 33 semantic actions across suit,
facing and aim variants to exact catalogue keys. Runtime loading rejects an
unknown key or duplicate selector. The runtime maps each controller pose to
one action; the controller owns the native frame index and duration counter,
so one-shot transitions end exactly when their native animation ends.
`scripts/audit_extracted_assets.py` inventories the ignored tree, identifies
exact duplicates by content hash, and records which historical top-level roots
still have tracked consumers. Historical room/editor caches are migrated only
with their consumers, never by an unverified bulk move.

`scripts/editor_backend.py` is the shared operation layer for headless commands
and GTK subprocess actions. It owns draft rooms, global-map placements, project
entities, story validation/storage and native workspace orchestration. GTK's
interactive tile canvas and the headless tile commands share the C native-map
core rather than duplicating its file or editing rules. `scripts/room_audit.py`
executes real MZM and Aria
decoders for every discovered room, isolates work by native area and emits a
structured private report. It does not replace either future gameplay engine.

Tracked outputs contain metadata only:

- `data/story/timeline.toml`: story dependency graph;
- `data/story/world_inventory.toml`: boss and savepoint inventory.

Decoded graphics, raw blocks, room work files, room annotation tables and user
overrides are ignored. `scripts/room_annotations.py` reconstructs bounded
per-room native records, while `scripts/object_catalog.py` joins definition and
placement metadata for the shared editor. No tool writes back to either ROM.
Canvas and Room-data context menus share the same bounded in-memory annotation
records; typed editor windows retain their document lifetime safely and cannot
write native data without an encoder.

Private room authoring uses a single `metroidvania.project-room-data` version 4
document for entity markers, semantic collision cells, doors, transitions and
typed event/trigger regions. The schema has per-world collision resolution and
strict spatial/identity bounds, but deliberately has no ROM or gameplay
encoder. Entity-only version 1 and room-data versions 2/3 are migrated in
memory and preserved until an explicit project write. Version 4 adds bounded
ordered condition groups to each event without creating another sidecar.

Event action references are resolved by the shared backend against the current
room document and validated story sources. Create/update rejects missing or
self-referential targets, `event-validate` exposes structured diagnostics, and
full project validation checks every saved room reference without executing it.
Condition groups use explicit `all`/`any` combination, per-condition negation
and the same dependency protection as action references.

## Verified native coverage

Zero Mission currently has room descriptors, original minimap structure,
partial BG1/BG2 rendering, experimental BG3 reconstruction and editable local
metatile work files. Collision, doors, default entities and event-dependent
spriteset variants are inspectable. The object catalog exposes all primary
sprite identities and raw health/damage/weakness expressions. Zero Mission
global-map ownership prefers native scroll regions and direct door/sprite
coordinates, with Clipdata dimensions only as fallback. Object graphics,
behavior and authoring encoders are not reconstructed.

Aria currently has verified character sprites and a structural world decoder.
The decoder reads the global `64x35` map and all 343 rooms through the
twelve-area directory. It resolves bounded room descriptors, three background
records per room, graphics/palette references, 2,336 entity placements and 725
transitions. It also identifies all eleven campaign bosses through their native
enemy records. The editor can inspect room entities, their parameters,
transitions and collision while the common catalog groups native types and
placement counts. Enemy names are joined from the verified ROM constructor
table to pinned cvaos symbols; other entity kinds expose their decoded semantic
role. Object graphics, music and executable behavior remain private-source
reconstruction work; room rendering is still incomplete.

## Future runtime boundary

Each native engine will expose a project-owned adapter for lifecycle, room
loading, update, rendering and persistence. Exactly one world engine may
advance authoritative gameplay at a time. After the Interzone story event,
both characters must exist in the active engine: one player-controlled and one
AI-controlled. Character switching is distinct from world travel.

Cross-world travel will only occur at authored portal/save links. Transferable
state must be specified field by field; source-local room coordinates and raw
engine state must never be copied blindly between engines.
