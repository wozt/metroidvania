# Local GBA graphics extraction (experimental)

`scripts/gba_tiles.py` reads **only user-supplied USA ROMs**. It checks SHA-1,
decodes raw 4bpp 8x8 tiles and a 16-entry BGR555 palette, and writes a BMP
preview inside the ignored `assets/extracted/` directory. No game data is
packaged or checked into Git. Only use offsets verified for your ROM.

```bash
python3 scripts/gba_tiles.py --character samus \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --tiles-offset 0xVERIFIED --palette-offset 0xVERIFIED \
  --tile-count 16 --tiles-per-row 4 \
  --output assets/extracted/previews/samus_tiles.bmp
```

Replace both `0xVERIFIED` placeholders with actual ROM byte offsets; they
are deliberately **not supplied or guessed**. The current output is a tile
grid **not a playable sprite**. Samus graphics consist of multiple tile
subsets plus OAM, palettes, and a separate arm cannon; Aria also requires
game-specific sprite assembly and animation tables. Some graphics may be
compressed: this decoder accepts only *raw* 4bpp tiles.

To render the real characters in the current rooms, the next milestone is
verified ROM-specific pointer/OAM decoding that composes transparent
`assets/extracted/sprites/{samus,soma}/{idle,run,jump,attack}_{0..3}.bmp`.
The renderer already expects those filenames. Do not rename tile previews
as animation frames: that would display incoherent tiles instead of poses.

Documentation reference: `third_party/mzm/docs/samus/graphics.md`.
