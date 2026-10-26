# Active architecture

## Product boundary

The final game is intended to be a native C11/SDL3 application with two
separate gameplay engines: Zero Mission rules in Zebes and Aria rules in the
castle. Each original game must first run independently from its native title
screen to its credits. Crossover mechanics, character adapters and combined
progression remain deferred until both reconstructions satisfy that boundary.
A later shared layer may own input, rendering presentation, persistence, story
events, cutscenes, party state and cross-world progression.

Neither native gameplay engine is complete. The active repository contains
source-data reconstruction and editor infrastructure plus experimental SDL3
Zero Mission and Aria room runtimes. They reproduce bounded, tested subsets of
their native player, room and combat systems, but neither has a complete game
lifecycle. They are not simulated substitutes or emulated frontends.

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
  editor; static C/ASM inventories for both pinned decompilations; an
  incremental inventory/checklist task graph; and
  `fusion_check_no_proprietary` guarding the repository.
- Experimental: the Zero Mission runtime (Samus, weapons, collision, doors),
  the Aria runtime (Soma, weapons, doors, bats and zombies), and the editor's
  project room documents, which already store overrides
  separately from native data but have no ROM or engine encoder yet.
- Not implemented: migration of every private extractor into the orchestrated
  rebuild graph, dynamic callback and runtime dependency capture, overlay
  application inside the engines, complete original-game lifecycles, behavior
  extension points, character adapters and shared crossover runtime systems.

## Native inventory and rebuild orchestration

`build/fusion_rebuild --all` is the dependency-aware reconstruction entry point.
Its current graph has two deterministic, ROM-free tasks:

1. `inventory` scans both pinned source trees for C definitions, assembly
   function markers, header declarations, top-level data, named aggregate
   types and object-like constants. It assigns stable IDs, records source
   addresses when exposed, resolves direct calls and table-based dispatch, and
   emits the static dependency graph;
2. `checklist` validates separate human annotations and generates
   `docs/NATIVE_PARITY_CHECKLIST.md`.

Task input hashes, task versions and output paths are cached under ignored
`.cache/rebuild/state.json`. A changed source, generator or annotation
invalidates the affected task and its consumers. Automatic discoveries use
`data/native_parity/inventory.json` as a small manifest over deterministic,
checksummed record fragments in the same directory. Fragments are capped below
the repository's large-file guard. Reviewed feature associations, local ports,
tests and status live in `data/native_parity/annotations.tsv`, so rescanning a
submodule cannot overwrite a human validation. The TSV is the single canonical
human source: one tab-separated row per feature with columns `game`, `id`,
`category`, `status`, `title`, `native_routines`, `local_sources`, `tests`,
`dependencies`, `divergences`, `notes` and `next_action`; multi-value columns
join entries with `|`. TSV is preferred over CSV because free-text notes
contain commas and semicolons but never tabs, so the file stays editable in
any spreadsheet or text editor without quoting rules. Rows are sorted by
unique, game-prefixed stable ids; validation rejects unknown games, statuses,
categories, dependency ids, missing local paths, and native symbols that no
longer exist in the regenerated inventory (renamed or removed routines
surface as orphaned references). The Markdown checklist is a generated view
and must never be edited by hand. The same pass writes
`data/native_parity/aos_unassigned_routines.tsv`: one compact row for every
Aria C or assembly function that is not yet attached to a reviewed parity
feature, including its stable id, category, address, source location and
direct call counts. For an unclassified routine whose known outgoing targets
all share one category, the report adds that category as a triage suggestion;
it never overwrites the proven classification. This is a generated
reconstruction backlog, not a claim that each low-level helper needs its own
feature.

The second static pass links header declarations to definitions, inventories
data/type/constant records and proves indirect edges when a lexical table and
its dispatch expression are both visible. Schema v4 also links a routine or
data initializer that writes a named routine into a member declared as a
function pointer (directly or through a function-pointer typedef); the member
name is matched lexically, so every type declaring it is listed as a
candidate. Schema v5 adds `address_references`: the routines whose address
a routine takes without calling it (a C identifier used as a value, outside
member names and call sites, or an assembly literal-pool `.4byte` word). They
are possible callback targets, kept apart from proven calls: taking an
address does not prove the routine runs, nor which stored callback a dynamic
dispatch selects. Computed addresses and runtime-observed calls stay
unresolved.

`fusion_rebuild` records, per task, a digest of its inputs (including its
dependencies' digests) and of its declared outputs: a changed input, a
missing output or an output edited by hand triggers the task again. A dry run
reports a task whose dependency would be rebuilt as pending instead of
reading inputs that do not exist yet.

The orchestrator is implemented in
`include/fusion/pipeline/rebuild.h`, `src/pipeline/tasks.c` and
`src/pipeline/graph.c`, with cache, planning and execution split across the
other files in `src/pipeline/`. It registers the same 15 tasks, their versions,
groups, explicit dependencies and local prerequisites, and resolves selected
tasks with deterministic topological ordering, duplicate suppression, unknown
dependency diagnostics and cycle detection. `fusion_rebuild --list` exposes
that registry; every task runs native code. Cache loading and deterministic atomic
writing, exact input/dependency/decompilation
stamps, output fingerprints, room-export and content-addressed sprite-index
validation, prerequisite propagation, forced invalidation and incremental
dry-run planning and real execution are native. Each successful action is
rehashed, validated and atomically recorded before its dependents run; failures
leave the last successful state resumable. The C planner reads the existing v1
cache without rewriting it, preserves unknown records and fields, rejects
unsafe state/output paths and produced the same complete dry-run output as
the former Python orchestrator. The asset tasks dispatch to `fusion_importer`
without a shell. `fusion_checklist` validates every checksummed inventory fragment and
the annotation TSV, then generates the Markdown checklist and Aria backlog
byte-identically in C11; the orchestrator calls its library directly.
`fusion_inventory --write` (`src/pipeline/inventory_writer.c`) regenerates the
complete tracked inventory byte for byte as the former Python writer:
`fusion_inventory --audit-routines` ports the lexical C/assembly function scanner, stable C-over-assembly
deduplication and direct/ambiguous call resolution: all 2,970 Zero Mission and
3,573 Aria routine identities and source metadata match the tracked v6
fragments, and the writer adds declarations, data, types, constants,
callback/address relations, classification and deterministic sharding.

Routine categories record their evidence (`category_evidence`, schema v6):
`path` when the source location decides it (all of Zero Mission's sorted
tree), `name` when a native name rule does (`Enemy*Create/Update`,
`ObjectNN*`, `Skill*Use`, `SoulInventory_*`, the `GameMode*` handlers, the
m4a/MP2K driver...), and `referrers` when every routine calling it or taking
its address already has one same category, repeated to a fixed point.
Shared helpers (mixed referrers) and unreferenced routines stay
unclassified; bosses are not split from Aria's enemies until a boss marker
is traced. The checklist prints this breakdown per category. Remaining gaps
are emitted in the generated checklist rather than silently excluded.

Asset tasks join the same graph through a `fusion_importer` command. Their
digests cover the ROMs and executables they need, plus the
pinned revision and tracked-file state of the matching decompilation when
applicable. Their declared outputs are index files or small manifests;
`src/assets/core/export.c` validates them (per-file SHA-256 for room exports,
content-addressed objects for sprite libraries), so a deleted or edited
private file reruns the task. A task whose required file (ROM, reference ELF)
is missing is skipped together with its dependents, which keeps `--all`
usable on a ROM-free checkout. Private outputs stay below the ignored
`assets/extracted/`; the rebuild state stays in the ignored `.cache/`.

## Active C components

### Native asset foundation

`fusion_assets` (`include/fusion/assets/rom.h`, `include/fusion/assets/gba.h`
and `src/assets/`) is the
first C11 layer of the native import pipeline. It owns only generic ROM
verification for the two pinned USA revisions so far: bounded loading,
expected-size checks, SHA-1 fingerprints, error reporting and zeroing on
release. It also atomically serializes the deterministic private identity
manifest for either verified image; the `aria_rom` and `metroid_rom` rebuild
tasks invoke that writer through `fusion_importer --rom-manifest`, with no
Python output path. Its generic GBA layer also owns a bounds-checked little-endian
reader, standard BIOS 0x10 LZ77 / 0x30 RLE decoders and bounded GBA header
metadata/checksum parsing. `fusion_importer`
(`src/tools/fusion_importer.c`) exposes `--list`,
`--verify` reports the parsed identity after ROM fingerprint validation,
`--rom-manifest` writes one verified private manifest, and
an explicit dry-run plan remains available for future extraction tasks. Every
extractor passed the equivalence gates in `docs/ASSET_PIPELINE_MIGRATION.md`
before its Python predecessor was removed.

The first game-specific reader is `fusion/assets/aos.h`. It decodes Aria's
12-entry area directory from a verified ROM, validates the GBA table pointers
and exposes per-area room counts. It also decodes the 64x35 global map into
bounded room-coordinate records with save/warp flags and room pointers. Room
descriptor headers and their bounded variant chains are native as well.
Transition records are decoded and resolved to unique native room identities.
The three background descriptors and their optional screen dimensions are also
native. Graphics/palette loading-list references and parameters are native too;
bounded entity placements are now native as well. The audited boss identities,
their unique room entities and enemy-table health values are native and checked
against the Python reference. A native aggregate reader also checks all world
summary totals. The same library now serializes the complete world JSON and its
derived room-browser index deterministically through atomic replacement. The
`aria_world` rebuild task writes both through `fusion_importer --aria-catalog`;
the Python writer, index generator and decoder are gone. Its
runtime-room layer also expands every compressed BG1 block/collision table and
produces the final flipped per-cell collision map. It also resolves direct and
LZ77 graphics resources and applies every native graphics/palette load entry to
bounded 64 KiB VRAM and 512-byte palette snapshots. All non-affine room
background block maps and direct/LZ77 metatile tables are expanded natively to
their final flipped 8x8 tile entries, then rendered to RGBA with native 4-bpp
character-base, palette-bank, flip and transparency semantics. Supported room
layers are also composed natively at a static origin using BG1 dimensions,
native depth keys, modulo repetition and transparent-pixel rejection. The
resulting RGB composites are serialized natively to byte-identical 24-bit BMP
files with bottom-up BGR rows and aligned pitch. The same layer serializes the
runtime's `AOSROOM-NATIVE` v4 TSV directly from final collision cells,
transitions, entities and save/warp map flags. `fusion_aos_export_runtime_rooms`
(`src/assets/aos/export.c`) writes all 342 supported room TSV/background pairs
and their SHA-256 export index atomically, and the `aria_rooms` rebuild task
runs it through `build/fusion_importer`; the Python room exporter is gone.
Parallax, affine/8-bpp layers and hardware blending remain unsupported. The
editor previews use the native renderer in `src/editor_backend/aria_render.c`.

The Zero Mission side now has a bounded native reader for the pinned reference
ELF's sized data objects. It applies the ROM-range, ordering and duplicate-name
rules proven against the former Python writer, and writes byte-identical
`MZMROM01` and symbol-manifest outputs through atomic replacement. The
`metroid_rom_bank` rebuild task runs that writer through `fusion_importer
--mzm-rom-bank`; the Python writer is gone. It also resolves all eleven room
tables from those symbols and decodes the 331 bounded `RoomEntryRom` records,
including their layer, Clipdata and spriteset references and scalar room
properties. The room layer now decodes the custom two-pass RLE behind every
Clipdata pointer into its final 16-bit cell map. It also classifies all
BG0/BG1/BG2 property combinations, expands their custom-RLE block maps and
BIOS-LZ77 text tilemaps, and exposes the final bounded 16-bit cells. The same
text-tilemap path now covers BG3, including its starts-from-bottom property.
The native reader also resolves all 79 tileset records and reproduces the
standard `RoomLoadTileset` 64-KiB VRAM and 512-byte BG-palette snapshots,
including both LZ77 graphics placements, common tiles/palettes and the final
VRAM clear. Animated tiles and palettes, the Mother Ship common-tile variant,
and dynamic effects are not reproduced by this snapshot. The native path reconstructs each
tileset's effective metatile table as well: up to 1,024 four-cell tileset blocks
are copied within their exact ELF object bounds and zero-filled, then the 208
four-cell common blocks are appended. It renders RLE block layers from those
tables on character base 1 and LZ77 text layers on character base 2 with native
4-bpp palette, flip and transparency semantics. Equal-sized decoded BG1/BG2
pairs are composed natively using the intentionally diagnostic BG1-over-BG2
order; rooms without BG2 retain their supported BG1. Both paths serialize to
the byte-identical private 24-bit BMP and a separate raw visibility mask that
distinguishes native black from unresolved transparent pixels. Hardware
priority/blending is not reproduced by this diagnostic renderer. The native
room-package path now reads the main/tilemap collision, behavior and movement
tables from bounded ELF objects and serializes the byte-identical
`MVROOM-NATIVE` v1 header plus its `C`, `B` and `M` Clipdata core. It also
decodes every production door table and `sAreaConnections`, resolving native
same-area and cross-area destinations before serializing ordered `D`, `X` and
location-banner `Y` records. It reads the hatch type, animation, tilemap,
weakness and event-lock tables as well, derives room hatches from Clipdata and
emits byte-identical `H/V/A/W/K/Q` records while retaining the distinction
between hatch slots and door indices. The same cumulative serializer now adds
the room's music, symbolic layout/effect metadata, block-layer rows and text
background descriptors as `U/L/E/G/R/T`, and the sprite records `P/S/F/J/N`
between the hatches and the music record: `sSpritesetPointers` and the room
entry's default and event sprite data are read from bounded ELF symbols and
resolved against a table of the decompilation's PrimarySprite names. The
native serializer therefore covers the complete room TSV payload. The
metatile sheets and text layers are rendered and serialized as the export's
BMP32 files too, and the shared `blocks.tsv` and `power_bomb.tsv` tables are
serialized from the bounded `block_data.c` and haze objects. The export's
`background.bmp`, which the runtime displays, keeps the older diagnostic
renderer (tileset metatiles only, inferred character base) and is
reproduced exactly by `fusion_mzm_render_export_background`.
`fusion_mzm_export_runtime_rooms` (`src/assets/mzm/export.c`) writes the
complete private export and its SHA-256 `export_index.tsv` atomically, and
the `metroid_rooms` rebuild task runs it through `build/fusion_importer`;
the Python room exporter is gone. The shared native sprite-library writer
now owns cropped BITMAPV4 frames, SHA-256 object deduplication, deterministic
runtime indexes and stale-object pruning. `fusion_aos_export_soma` uses it to
write all 83 Soma animations and the supported knife animation; `aria_soma`
runs this through `fusion_importer --aria-export-soma`, and the former Python
production writer and Python Soma decoder are gone. `fusion_aos_export_weapons` uses the same
writer for all 59 weapon records, 60 sprite sequences and their frame hitbox
metadata; `aria_weapons` now runs entirely through the native importer too.
`fusion_aos_export_objects` likewise writes the traced door, enemy and shared
effect library, frame boxes, anchors, probe/volley metadata and all 113 enemy
stat records; `aria_objects` no longer invokes Python.

`fusion_native_editor_core` contains the project-owned native map model,
selection operations and display-independent CLI launcher:

- `src/core/native_map.c`: versioned `MVNATIVE 1` room work files, editing,
  history and atomic override persistence;
- `src/core/native_selection.c`: bounded rectangular metatile selection moves.
- `src/core/editor_cli_launcher.c`: invokes the versioned C11 backend directly;
  no Python process or GTK initialization occurs on this path.
- `src/tools/native_map_cli.c`: internal display-independent adapter around the
  exact `native_map_load/edit/save` implementation used by GTK. The public CLI
  uses it for BG1/BG2 inspection, reads, writes and flood fills.

`fusion_aria_runtime` (`src/runtime/aria_runtime.c`) is the experimental
SDL3 Aria runtime: one exported room, Soma's sprite library as animation
timings and frames, and the SDL-free ports `aos_collision.c`, `aos_soma.c`
and `aos_anim.c`. Its simulation frames pass through the host-neutral native
game-mode table in `aos_game_lifecycle.c`; explicit room/save launches enter
IN_GAME directly while the incomplete boot/title path remains independently
testable. The same table now reaches the native main-menu operation selector
in `aos_main_menu.c`, including PLAY/CREATE/LOAD, COPY and DELETE, their
confirmation controllers, special completed-file positions and final direct
load/intro fades. Their typed slot-operation requests and the still-missing
native name editor remain separate from the portable launcher.

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

The native tools validate exact USA ROM fingerprints before reading them. They use
pinned decompilation symbols and audited ROM offsets to produce private output
under `assets/extracted/`.

New private generators use three stable roots:

- `metroid/` for Zero Mission raw data, rooms, maps and native sprites;
- `aria/` for Aria raw data, rooms, maps and native sprites;
- `shared/` for manifests and diagnostics that describe both worlds.

`src/pipeline/tasks.c` declares these paths. The raw import
(`fusion_importer --raw-import`, `src/assets/core/raw_import.c`) copies the
blocks the pinned decompilation databases identify below each world's `raw/`
root, writes its manifest below `shared/manifests/` and the Zero Mission room
descriptors to `rooms/metroid/rooms.tsv`; the native world overview
(`src/editor_backend/world_overview.c`) writes `rooms/metroid/world_atlas.tsv`
from the decompilation source when it needs it. `fusion_importer --mzm-export-samus`
(`src/assets/mzm/samus.c`) writes the canonical content-addressed Samus
runtime library below
`metroid/sprites/samus/runtime/`; repeated pixels are stored once by SHA-256.
One invocation composes every native animation directly from the verified
MZM ROM, the pinned decompilation pointer tables (generated into
`src/assets/mzm/samus_tables.c`) and the matching reference ELF, mirroring
`SamusUpdateGraphicsOam` (body, arm cannon animation and arm cannon graphics
selection) and `SamusUpdatePalette` (suit palettes). Each frame records its
native draw offset relative to Samus's position, and the runtime index uses the
shared, versioned `metroidvania-sprite-index-v1` schema of `src/assets/core/sprite_library.c`. Objects no longer
referenced by the index are pruned; no intermediate cache is kept.
The generated `animation_map.tsv` resolves 33 semantic actions across suit,
facing and aim variants to exact catalogue keys. Runtime loading rejects an
unknown key or duplicate selector. The runtime maps each controller pose to
one action; the controller owns the native frame index and duration counter,
so one-shot transitions end exactly when their native animation ends.
Historical room/editor caches are migrated only with their consumers, never
by an unverified bulk move.

`fusion_editor_backend` is the shared C11 operation layer for headless commands
and GTK subprocess actions. It owns draft rooms, global-map placements,
project entities, story validation/storage, native workspace orchestration,
render audits and room-package export. GTK's interactive tile canvas and the
headless tile commands share the same C native-map core rather than duplicating
its file or editing rules. The superseded Python CLI has been removed;
the Python operation layer and room-render audit have also been removed after
their remaining focused contracts moved to `tests/test_editor_backend.c`.
Neither editor path replaces a gameplay engine.

Tracked outputs contain metadata only:

- `data/story/timeline.toml`: story dependency graph;
- `data/story/world_inventory.toml`: boss and savepoint inventory.

Decoded graphics, raw blocks, room work files, room annotation tables and user
overrides are ignored. The native editor catalog decodes definitions, Aria
item names and private icon thumbnails in `src/editor_backend/object_catalog.c`.
The room-annotation and reporting paths query that catalog through
`fusion_editor_cli`; they do not maintain another definition parser. No tool writes back to either ROM.
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
