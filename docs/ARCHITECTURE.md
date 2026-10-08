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

`fusion_native_editor_core` contains the project-owned native map model and
selection operations:

- `src/core/native_map.c`: versioned `MVNATIVE 1` room work files, editing,
  history and atomic override persistence;
- `src/core/native_selection.c`: bounded rectangular metatile selection moves.

The GTK4 executable is assembled from:

- `editor/main.c`: responsive/detachable native workspace shell;
- `editor/native_workspace.c`: asynchronous room import, native BG1/BG2 editor,
  metatile palette and safe document lifetime management;
- `editor/world_atlas.c`: read-only Zero Mission world atlas and room launcher.

`fusion_mzm_room_viewer` is a small SDL3 viewer for locally rendered MZM room
BMPs. It is a research utility, not the future game runtime.

## Local data pipeline

Python tools validate exact USA ROM fingerprints before reading them. They use
pinned decompilation symbols and audited ROM offsets to produce private output
under `assets/extracted/`.

Tracked outputs contain metadata only:

- `data/story/timeline.toml`: story dependency graph;
- `data/story/world_inventory.toml`: boss and savepoint inventory.

Decoded graphics, raw blocks, room work files and user overrides are ignored.
No tool writes back to either ROM.

## Verified native coverage

Zero Mission currently has room descriptors, a door-derived world atlas,
partial BG1/BG2 rendering, experimental BG3 reconstruction and editable local
metatile work files. Full collision, entities, animations, effects and scripts
are not reconstructed.

Aria currently has verified character sprites and a structural world decoder.
The decoder reads the global `64x35` map, distinguishes save and warp flags,
and resolves room pointers through the twelve-area directory. Native room
descriptors, tilemaps, connections and entities remain to be decoded.

## Future runtime boundary

Each native engine will expose a project-owned adapter for lifecycle, room
loading, update, rendering and persistence. Exactly one world engine may
advance authoritative gameplay at a time. After the Interzone story event,
both characters must exist in the active engine: one player-controlled and one
AI-controlled. Character switching is distinct from world travel.

Cross-world travel will only occur at authored portal/save links. Transferable
state must be specified field by field; source-local room coordinates and raw
engine state must never be copied blindly between engines.
