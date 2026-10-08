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

- **Native rooms** browses pinned Zero Mission room descriptors and opens a
  room as an asynchronous document.
- **Native room** edits decoded BG1/BG2 16x16 metatile layers with pencil,
  eraser, fill, eyedropper, selection move, zoom, grid and undo/redo.
- **Native metatiles** selects blocks from the room's locally rendered atlas.
- **MZM world map** displays source room coordinates and door-derived links.
- **ROM visuals** previews locally extracted Samus and Soma animation frames.

Saving creates an ignored override under
`assets/extracted/overrides/metroid/`. The base import, ROM and upstream source
remain untouched.

## Current limitations

- BG0, full BG3 composition, animated graphics and several palette effects are
  incomplete.
- Collision, doors, entities, boss parameters, scripts, music and cutscenes are
  not editable.
- Aria has save-room metadata but no native room renderer/editor yet.
- Editor output is not consumed by a native gameplay runtime yet.
