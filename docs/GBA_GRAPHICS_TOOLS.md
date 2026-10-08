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

## Aria of Sorrow Soma cell extraction

`scripts/aos_soma_sprite.py` implements the first hash-gated Soma recipe. Unlike
Samus, Soma's player graphics are stored as complete 64x64 cells. Each graphics
resource is an uncompressed 128x128 4bpp sheet containing four cells. The game
copies the selected quadrant into 2D OBJ VRAM and draws it as two 64x32 objects.

The pinned matching USA build establishes this exact chain:

- `sub_08014628` initializes Soma;
- `0x080E11D4` describes 37 graphics sheets;
- `0x080E11C4` points to 83 animations through the table at `0x08639660`;
- animation 0 at `0x0822A7B8` is idle: frame IDs `12, 13, 14, 13` with
  durations `30, 11, 11, 11` at 60 Hz;
- frame IDs 12 through 14 use sheet 3 at `0x081664B4`;
- `0x082097D4` describes the OBJ palettes, with Soma's 16 colors starting at
  `0x082097D8`.

Generate the first complete cell with:

```sh
python3 scripts/aos_soma_sprite.py \
  --rom "roms/Castlevania - Aria of Sorrow (USA).gba" \
  --frame 0 \
  --output assets/extracted/sprites/soma/idle_0.bmp
```

The output remains a full 64x64 cell so its native animation anchor is not
lost. A live research capture independently confirmed that frame 13's 2,048
tile bytes match the corresponding OBJ VRAM bytes and that the 32 palette bytes
match OBJ palette bank 0. The research runtime is not part of this repository
or its runtime; only the ROM-derived extractor is project code.
