# Aria of Sorrow Soma graphics and engine research

This document tracks the native Soma Cruz pipeline and the evidence still
missing before an Aria gameplay kernel can be written. Generated data stays
under ignored `assets/extracted/`; never commit ROM bytes or extracted images.

## Canonical Soma library

```sh
python3 -m scripts.aos_soma_pipeline
./build/fusion_room_runtime --check-library \
  assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
```

The pipeline verifies the USA ROM SHA-1 and exports every animation listed by
Soma's native animation descriptor (`0x080E11C4`): 83 animations, 376 frames.
Each frame selects a 64x64 body cell from the graphics descriptor
(`0x080E11D4`, 37 sheets of four cells) and is colorized with palette bank 0
of the palette descriptor (`0x082097D4`). Frames keep their native 60 Hz
durations, are cropped, and carry their offset from Soma's draw origin, which
lies 32 pixels right of and 47 pixels below the cell's top-left corner. Keys
are `Soma/animation_NNN` by native index; `animations.json` records frame ids,
sheets and the few observed semantic labels (idle 0, run 1, run stop 25, run
start 26).

The knife descriptors (`0x0822B6C0` animation, `0x081AC54C` graphics,
`0x082098B8` palette) add `Knife/animation_000` with origin-relative OAM
offsets. The library uses the shared `metroidvania-sprite-index-v1` format of
`scripts/sprite_library.py`, the same one the SDL runtime loads for Samus.

The older `scripts/aos_soma_sprite.py` remains the decoder library and the
editor preview generator for six hand-selected sequences.

## Explicitly unresolved

- Palette banks 1-6 of Soma's palette descriptor are not identified. Some
  transformation animations (native indices 33-35 and 42-44)
  clearly need another bank and currently render with bank 0.
- Only the knife weapon is located. `Knife/animation_001` uses multi-component
  OAM records that the decoder rejects, and the weapon table for every other
  weapon has not been found.
- Semantic names exist for a handful of animations only.

## Native engine blockers

The pinned `third_party/cvaos` decompilation names enemy and object handlers
but not the player, camera or collision routines (about 2,400 functions are
still `sub_*`). The room importer decodes one collision byte per 8x8 cell from
each layer's metadata, but the meaning of those bytes (solid, platform,
slopes, hazards, water) and Soma's movement constants (walk and dash speed,
gravity, jump impulse and hold rules, backdash, hitbox) have no verified
source yet. Until those routines are identified, the repository does not ship
an Aria movement kernel or a collision interpretation, and the collision
preview colors remain diagnostic.

Verified so far (cvaos `asm/code/code_08039340.s`):

- `sub_0803F8A8` (0x0803F8A8) initializes a background layer from its metadata
  record: it stores the record in the 28-byte layer slot at
  `gEwramData + 0xA078 + 28 * layer`, and when flag bit 1 (compressed) is set
  it LZ77-decompresses the block table (offset 4) to `gEwramData + 0xA104`
  for BG1 or `+ 0xC0E8` otherwise. Only for BG1, and only in the compressed
  path, it decompresses the collision table (offset 8) to
  `gEwramData + 0xE0CC`. This confirms that collision is a BG1 property and
  that the importer reads the right table.
- `sub_0803F970` is the same initializer without the collision step;
  `sub_0803FBBC`/`sub_0803FC6C` update layer scrolling.

Player entity (cvaos `src/code/code_08014548.c` and its assembly):

- `sub_0801487C` creates the player with
  `EntityCreateInRange(0, 0, sUnk_084F10B4[currentCharacter])` and runs
  `sUnk_084F10AC[currentCharacter]` on it. For Soma these are the update
  routine `sub_0801B0D8` and the initializer `sub_08014628`, which loads the
  graphics (`0x080E11D4`), palette (`0x082097D4`, bank 0) and animation
  (`0x080E11C4`) descriptors used by the Soma library; Julius uses
  `sub_0801FEF8`/`sub_08014720`.
- At the start of every update `sub_0801B0D8` adds the X velocity at entity
  offset `+0x48` plus a one-frame extra velocity at `+0x2C` (then cleared) to
  the 16.16 fixed-point X position at `+0x40`, clamps the Y velocity at
  `+0x4C` to `0x80000` (8 pixels per frame downward) and adds it to the Y
  position at `+0x44`. The rest of the routine (3,862 assembly lines, with 27
  calls to `sub_080428B4` and 19 to `sub_0803F2C8`, likely animation and
  state helpers) is not yet understood.

BG1 collision readers (cvaos `src/code_08001194.c`, decompiled C):

- `sub_08001A00(x, y)` returns the collision byte of the 8x8 cell containing a
  room pixel. It reads `gEwramData->unk_E0D0` (the decompressed table after its
  four-byte header, i.e. `0xE0CC + 4`) for compressed layers and the raw
  metadata pointer otherwise, clamps the cell to the layer (30 x 26 cells for a
  one-screen dimension), and for slope bytes toggles bit 2 when the block is
  X-flipped and bit 1 when it is Y-flipped.
- `sub_08001800` computes the table index exactly as `scripts/aos_room_render.py`
  does: block map entry at metadata `+0xC` (`& 0x3FFF`, minus one), `* 16`,
  plus the local 4x4 cell with the 0x4000/0x8000 flips. The importer's
  collision decoding is therefore verified.
- `sub_08001B40(byte, x)` is the slope height inside a cell:
  `((byte & 0x30) >> 3) + (((byte & 4) ? 7 - x : x) & 7) >> ((byte >> 6) - 1)`.
  Bits 6-7 give the step (1, 1/2 or 1/4 pixel per pixel), bits 4-5 the starting
  height in steps of two pixels, bit 2 the direction.
- `sub_08001D94(x, y)` walks up (at most 8 pixels) while bit 0 is set and
  returns the negative distance out of the solid; `sub_08001C1C(x, y)` walks
  down (at most 9 pixels) while bit 1 is set. `0xFF` skips a whole 16-pixel
  block. A slope byte without bit 1 ends the upward walk on its surface; a
  slope byte with bit 1 ends the downward walk. `sub_080020A0` returns the
  distance to the top of a bit-0 cell. `sub_08001E58`/`sub_08001CCC` are the
  same walks after `sub_08001BA0` rewrites bytes with bit 3 for a mode argument.

Interpretation, checked against all 342 decodable BG1 tables (1.0 M cells):
bit 0 blocks movement from above (floors), bit 1 from below (ceilings).
`0x03` is a solid cell (337,055 cells); `0x01` is a floor-only platform (2,409
cells, almost always with air both above and below); slope bytes without bit 1
(`0x41`, `0x45`, `0x81`, `0xA1`, ...) always have air above and never below
(floor slopes), while those with bit 1 (`0x43`, `0x47`, `0x83`, `0xA3`, ...)
have air below (ceiling slopes). Still unidentified: bit 3 (`0x08`, 22,424
cells, mostly with air above), bit 2 outside slopes (`0x04`) and the rare
`0x14`, `0x27`, `0x37` values; `gEwramData->unk_A074_6` with the bitmap at
`unk_F0C0` overrides some cells to `0x03`.

The next research step is the player update routine `sub_0801B0D8`: which of
these probes it calls, with which body offsets, and Soma's movement constants.
