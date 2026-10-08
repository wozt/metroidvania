# Unified GTK4 workspaces (patch 0053)

## One room editor, two worlds

Both Metroid and Aria launch the same native tile painter. Its pencil, stamp,
selection, pan, grid, undo/redo and private `.mvnative` overrides are shared.
`Ctrl+W` uses the same unsaved-document close confirmation as the tab's X.
The original ROMs and extracted base files are never overwritten.

## Global maps: authentic cases, partial graphics

The new **Global maps** tab switches between Zero Mission and Aria of Sorrow.
It shows original *minimap cases*, not invented world-room geometry:

- **Aria:** the decoded 64×35 original minimap cells, preserving save and warp
  markers. Several cells can legitimately belong to one native room.
- **Zero Mission:** verified original `mapX`/`mapY` room *anchors*. Exact
  multi-cell room extents remain unknown and must not be inferred by size alone.
- Each occupied case uses a generated *original room graphics preview* when
  available. Preview images are diagnostic 2D composites, not exact minimap
  pixel graphics. Until generated, a case shows its original room number.
- Opening the tab, changing world/area, or pressing **Generate more original
  previews** runs a bounded batch in a background Python process. Unsupported
  source formats are recorded under ignored `assets/extracted/world_overview/`
  so they do not stall future batches. The GTK UI remains responsive.
- Double-click a case to open/focus its room document in the common editor.

All generated pixels and map cases are private to `assets/extracted/`.

## Orchestration and cutscenes

**Event orchestration** presents the canonical story as three selectable tracks
(Metroid / Castlevania / Shared). Click an event to jump to its section in the
editable `data/story/timeline.toml` source. **Save** checks the dependency
DAG and schema before updating the authored source.

**Cutscene editor** offers three initial authored scenes (the Interzone meeting,
Samus's Aria portal, and Soma's Zero Mission portal) in a selection dropdown.
It edits their TOML and validates actor references and portable actions. Scene commands and target
worlds share one format; `world = "aria"` or `world = "zero_mission"` selects a
future native engine adapter. **No cutscene runtime/engine-specific translator
exists yet**, and the UI is a data/step source editor, not a graphical keyframe
sequencer. Native gameplay kernels and 1:1 scene playback are later milestones.

Run:

```sh
python3 -m scripts.world_overview --world mzm --area 0 --budget 6
python3 -m scripts.world_overview --world aria --area 0 --budget 6
python3 -m scripts.validate_story_assets --kind timeline data/story/timeline.toml
python3 -m scripts.validate_story_assets --kind cutscene data/cutscenes/interzone_first_meeting.toml
./build/fusion_map_editor
```

## Remaining work

- Extract the exact Zero Mission minimap occupancy/masks, not only room anchors.
- Verify sprites, layers, scroll offsets, authentic composition and collision.
- Provide a visual timeline inspector/editor and cutscene tracks with preview.
- Bind the common scene schema to **both** native gameplay engines when built.
- Run actual GTK4 builds, sanitize close-dialog lifecycles and test on Debian.
