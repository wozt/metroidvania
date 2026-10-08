# Project status

Last updated: **2026-10-08**.

## Current product state

The repository is a native-content reconstruction and editor project. It is not
yet a playable crossover and contains no active simulated or emulator-backed
game frontend.

| Area | State | Verified boundary |
|---|---|---|
| Story | Data baseline | Complete campaign bible and dependency-checked parallel timeline |
| Boss inventory | Verified native identities | 9 MZM encounters and all 11 Aria campaign bosses source/ROM-mapped |
| Savepoints | Verified metadata | 29 MZM platform rooms and 17 Aria map-flag rooms |
| MZM rooms | Partial | descriptors, atlas, BG1/BG2 editing and experimental BG3 |
| Aria rooms | Structural inventory + preview | 343 descriptors, 725 transitions, 2,336 entities; experimental background renderer |
| Character assets | Partial | local verified Samus and Soma animation extraction |
| GTK4 editor | Active | native MZM documents, safe tab lifetime, atlas and asset previews |
| Native gameplay engines | Not started | no player physics, combat, entities or room runtime yet |
| Dual-character AI | Not started | story/design requirement only |
| Cross-world travel | Not started | savepoint metadata exists; no native runtime loader |

## Patch 0047 - active-tree and documentation cleanup

- Copied 94 superseded files to local ignored `legacy/` storage with their
  original paths preserved.
- Removed the rectangle-room demo, sample tilemaps/world graph, simulated
  backends, mGBA diagnostic frontend, transition experiments and their tests
  from the active tree.
- Removed superseded feasibility/recompilation/side-project documents and
  replaced accumulated prototype documentation with concise current references.
- Reduced CMake to the native editor core, GTK4 editor, SDL3 room viewer,
  active native tests and the Python extraction/data suite.
- Removed obsolete graph/tile/demo/capture controls from GTK4 while retaining
  native room tabs, atlas browsing and local authentic sprite previews.

## Patch 0048 - Aria native room inventory

- Expanded the hash-gated Aria importer from the global map into all 343 native
  room descriptors, including three background records, resource references,
  entity placements and 725 resolved room transitions.
- Bounded every pointer walk, variant chain and terminated list; the private
  generated catalog contains metadata and ROM addresses, never payload bytes.
- Matched all eleven campaign bosses to native rooms and enemy-table identities.
  Chaos has two explicit phase rooms; repeated ordinary boss-type enemies are
  not misclassified as campaign encounters.
- Recorded raw enemy-table health values and named cvaos create/update symbols
  in the tracked boss inventory.

## Verified commands

Run after each change:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 scripts/check_no_proprietary.py
```

The latest exact result is recorded in the corresponding commit handoff.

Cleanup validation on 2026-10-08: the reduced CMake project built without
warnings, CTest passed `5/5`, the consolidated Python suite passed `82/82`, the
GTK4 editor survived a three-second Xvfb smoke run, and the proprietary-file
guard passed. The active editor binary has no mGBA linkage.

Patch 0048 validation on 2026-10-08: CTest passed `5/5`, the Python suite passed
`85/85`, the exact-ROM Aria import completed with 343 rooms and 2,336 entities,
and the proprietary-file guard passed.

## Patch 0049 - Aria native room previews

- Added a verified, generated index for all native Aria rooms, with save/boss markers.
- Added a read-only GTK4 browser with asynchronous on-demand graphics decoding,
  BG1/BG2/BG3 selection, composite preview and diagnostic collision preview.
- A room lacking BG1 may now composite another successfully decoded text layer.
- This does not imply the graphics reconstruction is complete; unsupported
  affine modes, animation, blending and entities remain explicitly unfinished.

## Immediate priorities

1. Decode Aria tilemap/collision payloads into private native room work files.
2. Attach verified transition/access metadata to both games' savepoint records.
3. Complete MZM collision/entity extraction and verify room overrides.
4. Define the native engine contracts around decoded room data.
5. Implement the two prologues before general dual-character traversal.

## Non-negotiable constraints

- Never track ROMs, saves or extracted proprietary assets.
- Never mutate ROMs or upstream submodules.
- Never substitute fake geometry for missing original data without an explicit
  diagnostic label.
- Never call extracted metadata a completed native gameplay feature.

## Patch 0051 - confirm close and live tile stamp

- Native MZM and Aria documents display their real selected metatile as a semi-transparent cursor preview on hover; no painting occurs before mouse press.
- Picking a metatile from a room or the palette switches back to the Pencil stamp tool.
- Unsaved tab close offers Cancel, Discard changes, or Save and close in a GTK4 modal window; failed writes keep the document open.
- Additional GTK lifecycle test covers cancel, save and discard; private ROM-derived assets and overrides are never committed.
