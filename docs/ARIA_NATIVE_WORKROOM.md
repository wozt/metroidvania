# Aria native editing — Patch 0050

Aria room previews (rendered from the user's locally verified USA ROM) now
convert to lossless RGBA 16×16 visual tiles in an ignored PNG atlas and
`MVNATIVE 1` workroom. The shared GTK4 editor opens Aria documents with the
same pencil, eraser, fill, picker, selection/drag, pan, grid, undo/redo and
save-as-private-override tools as Zero Mission.

These are **visual editing workrooms**, not source-faithful Aria 8×8 map
serialization. The original map's 8×8 entries, block IDs, native collision,
entities, animations, scripts and BG3 layer are not editable in this format.
Opaque and transparent source pixels are preserved in the generated 16×16 RGBA
atlas; no owned source assets are published.

Test with:

```sh
python3 -m scripts.aos_native_workspace --area 0 --room 10
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/fusion_map_editor
```

In **Aria / Native rooms**, select a room then press the edit icon or double
click it. Aria base/workroom and overrides are private:

- `assets/extracted/rooms/aria/tilesets/area_XX_room_YYY_atlas.png`
- `assets/extracted/rooms/aria/workrooms/area_XX_room_YYY.mvnative`
- `assets/extracted/overrides/aria/area_XX_room_YYY.mvnative`

The Zero Mission GTK closing crash is also mitigated by using GTK-owned
native tooltips and deferring notebook page removal until the button signal
returns. A real GTK4/ASan reproduction on the user's Debian machine is still
required to validate the fix fully.
