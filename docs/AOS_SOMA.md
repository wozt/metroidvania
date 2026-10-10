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

Ported so far: `python3 -m scripts.aos_runtime_room --area <A> --room <R>`
exports a room's background composite and its BG1 collision bytes as
`sub_08001A00` returns them (342 of 343 rooms; the remaining one has no text
BG1). `src/runtime/aos_collision.c` ports the cell lookup, slope height, walk
modes, the four vertical walks, the bit-3 test and both horizontal pushes,
with ROM-free tests. It is not yet used by a Soma controller.

Player collision: `sub_08014A04` (Soma) and its twin `sub_0801D1C8` call these
probes; the pinned decompilation only has a commented, non-matching m2c draft
of it. Probe points read from that draft and the assembly include a ceiling
walk at the origin minus 33 pixels (`sub_08001C1C`), floor walks at the
origin plus one pixel and at x +/- 5 (`sub_08001E58` with a mode), bit-3 tests
at x +/- 5 (`sub_08001F3C`) and wall pushes at x +/- 8 (`sub_08002058`,
`sub_0800207C`). Interleaved loops over `gEwramData + 0x1316C` entities look
like moving-platform handling. These offsets are recorded as reading notes;
the control flow is not yet understood well enough to port.

Soma motion constants verified in the assembly (16.16 fixed point, pixels per
60 Hz frame, positive Y downward; `+0x4C` is Y velocity, `+0x54` a Y
acceleration term, `+0x14` an airborne frame counter, `+0x10` state flags):

| Behavior | Evidence | Values |
|---|---|---|
| Ground jump | `sub_08019180`: jump button (`gEwramData + 0x1339A`) pressed while grounded or while `+0x14 <= 3`; sound `0xB9`; sets airborne flag 2, `+0x14 = 16`, `+0x54 = 0` | vy = `0xFFFB0C00` = -4.953125; -1.234375 (`0xFFFEC400`) when flag `0x4000000` is set |
| Second jump button branch | same routine, ability check `sub_08032AB8(4)` and button `0x1339C`; sound `0xBA` | vy = -10.0, or -2.5 with flag `0x4000000` |
| Mid-air jump | `+0x14 > 3` with ability `sub_08032AB8(2)` calls `sub_080190E0` | writes -4.5 / -4.125 (not yet traced) |
| Flag `0x800000` jump | same routine | vy += -4.875 when vy > 1.0, otherwise vy = -4.875 |
| Ceiling bump | `sub_08018B98`: ceiling walk at y-32 pushes the body down; while rising, `sub_08017CC8` handles a special case (vy <= -4.9375, state 6, sound `0xB8`), otherwise | vy = +0.0625, `+0x54` = -0.125 |
| Air gravity | `sub_08018B98` each airborne frame | +0.125 while vy <= 0x1FFF; then +0.1015625; when falling vy += `+0x54`, `+0x54` += 0.015625 up to 0.0625 |
| Fall cap | `sub_0801B0D8` | vy <= 8.0 |
| Heavy gravity | `sub_08018B98`, flag `0x4000000` (`0x80 << 0x13`) | vy += 0.375 and `+0x54` = 0 before the normal gravity |
| Ledge start | `sub_08018020` entry, when `+0x10 & 0x100002` is clear | sets airborne flag 2, vy = 0, `+0x54` = -0.0625 |
| Jump release | `sub_0801938C`, airborne and `+0x10 & 0x18` clear | vy < -0.25 without the jump button held: vy = -0.25, `+0x54` = -0.125 |
| Apex float | same routine, -0x1BFFF <= vy <= 0x1FFF with jump held | `+0x54` -= 0.03125 per frame, floor -0.125 |
| Slow fall | same routine, flag `0x100` of `gEwramData + 0x13260` or entity flag `0x400000` (`0x80 << 0xF`) | vy -= 0.15625 while vy > 0.15625 |

| Air steering | `sub_0801B0D8` airborne branch: held right (`0x10`) / left (`0x20`) in `gEwramData + 0x1C` | vx = +/-1.5 (4.0 when flag `0x400` of `+0x13260` is set); otherwise `+0x50` = -/+0.25 per frame until vx reaches 0 |
| Probable backdash | `sub_0801B0D8`, sound `0xBD`, flags `0x20000420` | vx = -3.125 with `+0x50` = +/-0.09375 per frame |
| Damage recoil | `sub_0801B0D8`, state `0x0F`, source X at `+0x131D8` | vx = 1.5 away from the source, vy = -2.0, `+0x54` = -0.0625, `+0x50` = -/+0.0078125 |

An earlier revision of this table read the impulse flag as `0x400000`; the
assembly builds it as `0x80 << 0x13` = `0x4000000`, and `0x400000` is the
separate slow-fall flag. Its meaning (probably water) is unconfirmed.
Friction stops on a sign change only: a vx that lands exactly on zero keeps
its friction term for one more frame. Ground walking on slopes and the
grounded state routines (`sub_08016DE4`, `sub_080168F0`) have not been traced
yet.

`src/runtime/aos_soma.c` ports the rules of this table except the ceiling
probe, the flag `0x800000` and second/mid-air jumps, backdash and recoil;
`tests/test_aos_soma.c` checks them without a ROM (a held jump rises about
56.7 pixels over 56 frames, a tapped jump about 9.7 pixels).

The next research step is the player update routine `sub_0801B0D8`: which of
these probes it calls, with which body offsets, and Soma's movement constants.
