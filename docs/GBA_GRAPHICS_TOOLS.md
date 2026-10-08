# Generic GBA graphics tools

These SHA-1-gated tools operate only on user-supplied USA ROMs and write under
ignored `assets/extracted/`. They are hardware-format diagnostics shared by the
two backends, not character-specific extraction recipes. See `MZM_SAMUS.md` for
the complete Zero Mission Samus pipeline.

## Raw 4bpp tile preview

`scripts/gba_tiles.py` decodes uncompressed 4bpp 8x8 tiles and a 16-entry
BGR555 palette into a BMP grid:

```sh
python3 scripts/gba_tiles.py --character samus \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --tiles-offset 0xVERIFIED --palette-offset 0xVERIFIED \
  --tile-count 16 --tiles-per-row 4 \
  --output assets/extracted/previews/samus_tiles.bmp
```

Replace placeholders with verified ROM byte offsets. A tile grid is not a
playable sprite and must not be renamed as an animation frame. Compressed data,
animation pointers, OAM composition and character-specific overlays are outside
this primitive.

## OBJ/OAM composition

`scripts/gba_oam.py` combines uncompressed 4bpp OBJ data, a 256-color
(16-bank) BGR555 palette and standard eight-byte hardware OAM records. It
supports 1D and 2D mapping, normal OBJ shapes and sizes, flips, priority,
palette banks and transparency. Affine objects and 8bpp mode are rejected.

```sh
python3 scripts/gba_oam.py --character samus \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --tiles-offset 0xVERIFIED --tile-count 256 \
  --palette-offset 0xVERIFIED \
  --oam-offset 0xVERIFIED --oam-count 3 \
  --origin-x -32 --origin-y -32 --width 64 --height 64 --mapping 2d \
  --output assets/extracted/previews/oam.bmp
```

This example is deliberately not a Samus recipe. MZM stores Samus as multiple
graphics subsets with compact six-byte raw OAM and separate cannon data; Aria
uses different tables. Character-specific code must stage those formats before
calling the generic compositor.

The output is a 32-bit BGRA BMP with explicit alpha masks accepted by SDL3.
Run `python3 -m unittest tests.test_gba_oam` after changing the primitive. Never
commit generated images or substitute guessed offsets.
