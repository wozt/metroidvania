# MZM world atlas and experimental BG3 (0044)

The `World map / MZM` GTK tab displays **original Zero Mission minimap
anchors** and **verified intra-area door indices** extracted from pinned
`third_party/mzm/src/data/rooms_data.c`. It does NOT infer room bounding boxes,
cross-area transitions or event-dependent routes. Some rooms overlap on the
original minimap and may share the same marker. Click a marker for details;
double-click or use Open selected room to create/focus its private editable tab.
The atlas is not yet an editor for door connections.

Import from the **user's local** decomp/ROM extraction:

```sh
python3 -m scripts.mzm_world_atlas
./build/fusion_map_editor
```

Generated data stays under ignored `assets/extracted/rooms/metroid/`:
`world_atlas.json`, `world_atlas.tsv`.

Optional experimental MZM BG3 preview:

```sh
python3 -m scripts.mzm_bg3_preview --area Brinstar --room 33
```

When opening a room via GTK, the native workspace importer tries to generate
its BG3 preview automatically. The toggle in the room toolbar controls whether
the decoded preview repeats behind the selected BG1/BG2 metatiles; when editing
BG1, decoded BG2 metatiles are also shown underneath (toggleable). BG3 pixels
are **diagnostic, not yet an exact hardware composite**; compressed resources,
common palette banks, layers, scroll offsets, animated tiles, clipping and
blend may be unresolved. Missing resources do not block editing. The generated
BG3 `.bmp` and ALL original ROM data remain local and ignored by Git.

The current map covers original MZM's seven normal areas only. Aria of Sorrow
worlds require a separate decoder; the existing cross-world demo graph remains
unchanged. No extracted/proprietary assets are committed.
