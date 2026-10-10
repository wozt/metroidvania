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

Not yet found: no other routine references `0xE0CC` as a literal, so the
collision readers reach the buffer through a cached pointer or a computed
offset. The next research step is to trace writes of that buffer address into
IWRAM/EWRAM globals, then the player update routine that reads Soma's position
and velocity, and document their constants with ROM addresses before
implementing the kernel.
