# Original Zero Mission BG1/BG2 offline renderer (patch 0039)

Run the verified ROM importer first:

```sh
python3 -m scripts.rebuild --task raw_import
./build/fusion_editor_cli --command=room-render --world=zero_mission --area=Brinstar --room=33
./build/fusion_mzm_room_viewer assets/extracted/rooms/metroid/previews/brinstar_033_bg1.bmp
./build/fusion_map_editor
```

- Uses pinned MZM `RoomEntryRom` room metadata and raw ROM blobs, including
  the actual two-pass MZM room RLE, GBA 0x10 LZ77, tileset metatile entries,
  4bpp tile pixels and BGR555 palette colors.
- MZM writes tileset graphic data to VRAM+0x5800 and maps non-common palette
  rows to BG banks 3-15; the renderer accounts for these offsets.
- Exports **partial** original BG1/BG2 layer BMPs and JSON diagnostics to
  `assets/extracted/rooms/metroid/previews/` (ignored by Git).
- Common tiles, BG0/BG3, animations, blending, sprite entities, lighting,
  scrolling and some palette rows are not yet reconstructed. Dark or missing
  cells are **not** synthetic asset substitutes. This is not a native game port.
- The GTK4 `Native rooms` tab displays the actual generated BG1 image when
  available. The SDL3 `fusion_mzm_room_viewer` displays the same local BMP.
- No copyrighted ROM bytes, audio, tiles, or frame images are committed.
- Aria has not received a full room decoder yet.

## Runtime backgrounds (patch 0255)

The native importer (`src/assets/mzm/runtime_room.c`) rebuilds BG VRAM and
palette RAM as `RoomLoadTileset` and `RoomLoadBackgrounds` leave them, and
the runtime room export (`fusion_importer --mzm-export-rooms`, run by the
`metroid_rooms` rebuild task) writes, privately:

- `rooms/runtime/tilesets/tileset_NN.bmp`: the 1024 tileset metatiles and the
  208 common ones (doors, breakable blocks, tanks), 32 per row, 32-bit with
  transparent color 0;
- in `room.tsv`: `L` (tileset, transparency, BG0/BG2/BG3 properties, BG3
  scrolling, effect), `E` (the room entry's `effectY` in blocks, 255 for
  none; the runtime gives sprites `gEffectYPosition` from it, without
  `gWaterMovement`'s wave), `G`/`R` (the BG0 when RLE, BG1 and BG2 block maps as
  sheet indices) and `T` (the BG3 and LZ77 BG0 text layers, written as
  `bg3.bmp` / `bg0.bmp`).

`fusion_room_runtime` draws the layers by the native priorities of the
room's transparency value around the sprites (priority 2, or 1 when Samus is
on top of the backgrounds; BG1 normally passes in front of Samus), blends
BG0 like `BLDALPHA` and scrolls BG3 with `ScrollBg3`. Animated tiles and
palettes, BG movement effects, gradients and windows are not reproduced yet.

## Hatches and breakable blocks (patches 0256-0257)

Hatches and breakable blocks change BG1 blocks and raw Clipdata values like
the game (`BgClipSetBg1BlockValue`, `BgClipSetClipdataBlockValue`):
`src/runtime/mzm_hatches.c` runs the hatch state machine and
`src/runtime/mzm_blocks.c` the block routines of `src/block.c`. The block,
tank and bomb chain tables and the collision and behavior of every raw
Clipdata value are exported privately as `rooms/runtime/blocks.tsv`
(`src/assets/mzm/runtime_room.c` reads them from bounded ELF objects).
