# Active architecture

## Product boundary

The final game is intended to be a native C11/SDL3 application with two
separate gameplay engines: Zero Mission rules in Zebes and Aria rules in the
castle. A shared layer will own input, rendering presentation, persistence,
story events, cutscenes, party state and cross-world progression.

Neither native gameplay engine exists yet. The active repository therefore
contains source-data reconstruction and editor infrastructure only. It does not
contain a simulated substitute or an emulated gameplay frontend.

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
