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
- Collision authoring has continuous **Wall** (red), **Platform** (green
  one-way), **Water** (blue) and **Air** (passable eraser) brushes. A cell cursor
  previews the exact 16px Zero Mission or 8px Aria target before painting.
- The room toolbar independently toggles **Walls**, **Objects**, **Doors**,
  **Events** and **Triggers**. Native event records remain inspectable, while
  project event/trigger regions are editable overlays saved in the shared room
  document rather than as map pixels. Native trigger decoding remains pending.
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
- Native objects, doors, events and collision remain inspectable and immutable.
  Separate project collision cells can be painted with Wall, Platform, Hazard,
  rising/falling Slope, Water and Air brushes or
  changed from the canvas context menu, and project doors can be created,
  viewed and deleted. Project door properties and transition destinations have
  a dedicated GTK form backed by the same validated CLI operations. None of
  this data is playable yet.
- Project enemy, item and object forms edit the label, snapped position and
  verified native reference in both modes. Aria pickup forms additionally edit
  the typed item/soul ID, two native parameters and flags. Runtime behavior and
  native stats remain read-only reference data until engine adapters exist.
- Project event and trigger regions can be created from the canvas context
  menu, resized and typed in a dedicated form, moved with Grab, inspected and
  deleted. Their trigger/action/reference metadata uses the 16px Zero Mission
  or 8px Aria grid and the same staged Undo/Redo/Save workflow.
- Native-record editor shells expose decoded source fields but keep Apply
  disabled. Complete object sprite previews and executable behavior decoding
  are still pending.
- Project-object creation and cloning are disabled until a versioned common
  schema and target-engine encoders are validated.
- Native trigger regions, boss parameters, scripts, music and engine-backed
  cutscene authoring are not yet decoded or editable. Project trigger regions
  are authorable but are not consumed by either gameplay engine yet.
- Editor output is not consumed by a native gameplay runtime yet.

The collision toolbar is shared deliberately: both source formats have verified
native counterparts for the seven exposed semantics. Zero Mission uses 16px
project cells and Aria uses 8px project cells. Moving and crumbling platforms
remain objects rather than static one-way collision. Tooltips and the headless
`collision-capabilities` command expose this distinction; native encoding is
still unavailable for both games.
