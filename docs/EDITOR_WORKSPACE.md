# Native editor workspace

Build and open the GTK4 editor:

```sh
./build/fusion_rebuild --task raw_import
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

The Zero Mission runtime plays those overrides: `fusion_room_runtime` loads
an edited room's BG1 and BG2 blocks from its `.mvnative` override, at the start
and through doors, when the override names the same room and tileset and has
the room's dimensions (otherwise it reports it and keeps the native room).
Deleting the override restores the original; `--no-overlays` ignores them all.
The collision painted in the editor is played as well: each project cell of the
room's document (`overrides/metroid/entities/<area>_<NNN>.json`) replaces the
native block with the native Clipdata value of its type (solid, one-way
platform, lava hazard, rising or falling steep slope, water, air), when the
document names the same room and dimensions. Its entities whose native
reference is a Zero Mission sprite (`PSPRITE_*`) join the room's sprites at
their block, with the spriteset's graphics slot when the room uses that
sprite; unassigned entities are skipped. Its transitions to Zero Mission rooms
are played too: a door adopted from a native door redirects that door to the
transition's room, any other project door becomes a new hatchless transition
over its blocks (a side door for left and right, a vertical transition for up
and down). Samus arrives with her feet at the spawn point or, without one,
beside the saved target door. Transitions to Aria and events stay unplayed.

## Current limitations

- BG0, full BG3 composition, animated graphics and several palette effects are
  incomplete.
- Native objects, doors, events and collision remain inspectable and immutable.
  Separate project collision cells can be painted with Wall, Platform, Hazard,
  rising/falling Slope, Water and Air brushes or
  changed from the canvas context menu, and project doors can be created,
  viewed and deleted. Project door properties and transition destinations have
  a dedicated GTK form backed by the same validated CLI operations. Zero
  Mission's runtime plays the project collision and the transitions between
  its rooms; door types, cross-world transitions and everything in Aria are
  not playable yet.
- Project enemy, item and object forms edit the label, snapped position and
  verified native reference in both modes. Aria pickup forms additionally edit
  the typed item/soul ID, two native parameters and flags. Zero Mission's
  runtime spawns the entities that reference a native sprite; runtime behavior
  and native stats remain the native ones, read-only.
- Project event and trigger regions can be created from the canvas context
  menu, resized and typed in a dedicated form, moved with Grab, inspected and
  deleted. Their trigger/action/reference metadata uses the 16px Zero Mission
  or 8px Aria grid and the same staged Undo/Redo/Save workflow. Saving verifies
  typed timeline, cutscene, entity, event, transition and checkpoint references;
  missing targets are rejected without replacing the saved room document.
- The same form contains an **All/Any** condition selector and up to 16
  removable condition rows. Each row has a typed reference and **Not** toggle;
  an empty **All** group is the explicit unconditional state.
- Native-record editor shells expose decoded source fields but keep Apply
  disabled. Complete object sprite previews and executable behavior decoding
  are still pending.
- Project-object creation and cloning are disabled until a versioned common
  schema and target-engine encoders are validated.
- Native trigger regions, boss parameters, scripts, music and engine-backed
  cutscene authoring are not yet decoded or editable. Project trigger regions
  are authorable but are not consumed by either gameplay engine yet.
- Only Zero Mission's block layers, project collision, sprite entities and
  transitions are consumed by a gameplay runtime; Aria's runtime, door types,
  cross-world transitions, item settings and event edits are not.

The collision toolbar is shared deliberately: both source formats have verified
native counterparts for the seven exposed semantics. Zero Mission uses 16px
project cells and Aria uses 8px project cells. Moving and crumbling platforms
remain objects rather than static one-way collision. Tooltips and the headless
`collision-capabilities` command expose this distinction; native encoding is
still unavailable for both games.
