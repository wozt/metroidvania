# Zero Mission global map reconstruction

The Global maps GTK tab joins independent native sources instead of
guessing room rectangles:

- `RoomEntryRom.mapX/mapY` supplies each room's global origin.
- native scroll records supply the traversable room regions, including
  breakable-block and elevator extensions;
- decoded Clipdata dimensions supply a conservative fallback when a room has
  no custom scroll record; the four-block guard border is removed before
  dividing by the native 15x10-block screen size;
- native door and sprite-placement coordinates provide direct room evidence
  when two candidate regions overlap;
- original 32x32 pause-minimap tilemaps decide which candidate cells actually
  exist.

Only the intersection of engine geometry and original minimap occupancy is
assigned to a room. Irregular holes remain holes. When engine rectangles
overlap, an exact `mapX/mapY` origin or unique door/sprite evidence can
disambiguate a cell; all other unproved cells stay visibly unassigned. Rooms
sharing an origin are exposed as progression variants rather than silently
merged. The ignored private
`world_overview/mzm_ownership.json` report lists every variant family and
ambiguity.

Click a verified cell for its provenance and variant information. Double-click
or use Open selected room to create/focus its private editable tab. Unassigned
cells cannot open arbitrary room IDs. The atlas is not yet an editor for door
connections.

Import from the **user's local** decomp/ROM extraction:

```sh
python3 -m scripts.mzm_world_atlas
./build/fusion_map_editor
```

Generated data stays under ignored `assets/extracted/`, including the room
catalog, global-map TSV, ownership report, previews and editor workrooms.

Optional experimental MZM BG3 preview:

```sh
python3 -m scripts.mzm_bg3_preview --area Brinstar --room 33
```

When opening a room, the importer also produces a native Clipdata diagnostic.
The collision/wall toolbar toggle draws it over BG1/BG2 without modifying the
editable layers. Red identifies non-air/special Clipdata, yellow identifies
native slope IDs and purple identifies door IDs. This is a read-only diagnostic
overlay, not collision authoring yet.

The background toggle controls whether decoded BG2/BG3 appears under the active
layer. BG3 pixels are **diagnostic, not yet an exact hardware composite**;
compressed resources, common palette banks, scroll offsets, animation and
blending may remain unresolved. Missing optional previews do not block editing.
Ctrl+mouse-wheel changes zoom in both the room workspace and global map.

The current MZM map covers the seven production areas. Aria retains its own
verified 64x35 native minimap cells through the same GTK workspace. No
extracted or proprietary assets are committed.
