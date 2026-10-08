# Original Zero Mission BG1/BG2 offline renderer (patch 0039)

Run the verified ROM importer first:

```sh
python3 scripts/import_game_assets.py --scope all
python3 scripts/mzm_room_render.py --area Brinstar --room 33
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
