# Native editor workspace

Build and open the GTK4 editor:

```sh
python3 scripts/import_game_assets.py --scope all
cmake -S . -B build
cmake --build build -j"$(nproc)"
./build/fusion_map_editor
```

The workspace has three responsive docks. Tabs can be reordered, moved between
docks, or detached into separate windows.

## Active tools

- **Zero rooms** and **Aria rooms** browse original rooms and private authored
  drafts through one implementation. Original rooms open asynchronously in the
  shared editor; drafts remain explicitly non-playable.
- **Open editors** edits decoded BG1/BG2 metatile layers with pencil, eraser,
  fill, eyedropper, selection move, zoom, grid and undo/redo.
- The room toolbar independently toggles **Walls**, **Objects**, **Doors** and
  **Events**. **Triggers** is visible but disabled until those native records
  are decoded. These overlays inspect original data and are not saved as map
  pixels.
- **Room palettes** contains a metatile browser and a **Room data** list with
  native coordinates, dimensions, variants, identities and decoded details.
- Right-click any visible object, door/transition, event or decoded trigger on
  the room canvas to inspect its full native record, locate it in **Room data**,
  or open its type-specific editor shell. The same menu is available from the
  **Room data** list. Native fields remain read-only until the corresponding
  project schema and engine encoder are validated.
- **Global maps** displays original-coordinate Zero Mission and Aria maps,
  supports drag panning and Ctrl+wheel zoom, and launches known room editors.
- **Object catalog** shows the native definitions used by both games in two
  columns. Zero Mission health/damage/weakness expressions and Aria placement
  counts/known boss health are inspectable. Aria enemy IDs are resolved through
  the verified ROM constructor table and pinned named symbols; special objects,
  candles and pickup classes use semantic labels instead of raw kind/ID pairs.
- **ROM visuals** previews locally extracted Samus and Soma animation frames.

Saving a native room creates an ignored override under
`assets/extracted/overrides/<world>/`. Generated annotations, previews, the
base import, ROMs and upstream source remain unchanged.

## Current limitations

- BG0, full BG3 composition, animated graphics and several palette effects are
  incomplete.
- Native objects, doors, events and collision are inspectable but not editable.
  Their typed editor shells expose decoded source fields but keep Apply
  disabled. Object sprite previews and executable behavior decoding are still
  pending.
- Project-object creation and cloning are disabled until a versioned common
  schema and target-engine encoders are validated.
- Trigger regions, boss parameters, scripts, music and engine-backed cutscene
  authoring are not yet decoded or editable.
- Editor output is not consumed by a native gameplay runtime yet.
