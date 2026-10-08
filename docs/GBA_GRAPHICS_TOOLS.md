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
- animation 1 at `0x0822A7CC` is the 17-frame continuous run loop, with
  exact per-frame durations totaling 56 updates;
- movement starts with animation 26 (`68, 69, 70`) for `2, 3, 3` updates and
  stops through animation 25 (`26, 65, 66, 67, 25, 26, 15, 12, 13`) for
  `4, 5, 8, 7, 7, 9, 13, 14, 19` updates;
- a normal level-ground jump combines animation 50, the first two frames of
  animation 12, and animation 13 into a 12-frame, 61-update sequence;
- the standing knife attack combines body animation 4 (27 updates) with body
  recovery animation 5 (21 updates); the six offensive body poses use frame
  IDs `20, 21, 136, 22, 137, 23`, followed by recovery IDs `24, 25, 26`;
- frame IDs 12 through 14 use sheet 3 at `0x081664B4`;
- `0x082097D4` describes the OBJ palettes, with Soma's 16 colors starting at
  `0x082097D8`.

The knife is not part of Soma's 64x64 body cell. Its independent chain is:

- weapon entry 0 at `0x08505D3C`, which selects graphics resource 4 and
  animation resource 4;
- the uncompressed 128x32 4bpp sheet at `0x081AC54C`;
- animation descriptor `0x0822B6C0`, whose animation 0 uses frame IDs
  `9..14` for `3, 2, 3, 6, 5, 8` updates;
- palette descriptor `0x082098B8`, palette 0, staged as OBJ palette bank 1;
- one OAM component per pose, placed relative to the same origin as Soma.

The extractor merges every body and knife boundary. This produces 11 complete
attack images with durations `3, 2, 3, 6, 2, 3, 3, 5, 7, 7, 7`, totaling the
original 48 updates. Generate the four primary states and both movement
transitions with:

```sh
for animation in idle run jump attack run_start run_stop; do
  python3 scripts/aos_soma_sprite.py \
    --rom "roms/Castlevania - Aria of Sorrow (USA).gba" \
    --animation "$animation" \
    --output-dir assets/extracted/sprites/soma
done
```

Each animation uses the union of its opaque cell bounds, retaining the native
relative anchor while removing identical transparent margins: 18x34 for idle,
28x33 for run, 29x36 for jump, 51x34 for the body-plus-knife attack, 20x34 for
the run start, and 29x34 for the run stop. Live research captures independently
confirmed that idle frames 12 and 13, run frame 99, and jump frame 118 each
match all 2,048 corresponding OBJ VRAM bytes.
Knife frame 12 also matches all 128 live VRAM bytes at OBJ tile 818, and its 32
palette bytes match OBJ palette bank 1. Soma's body palette matches OBJ palette
bank 0. The research runtime is not part of this repository or its runtime;
only the ROM-derived extractor is project code.
