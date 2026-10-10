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

Player collision: `sub_08014A04` runs every frame from `sub_0801B0D8`
(unless bit 0 of `gEwramData + 0x131B8` is set), after the position
integration and before the state dispatch; `sub_0801D1C8` is its twin. The
pinned decompilation only has a commented, non-matching m2c draft; the order
and constants below were read from that draft and checked against the
assembly (wall loop, bounce, landing masks and thresholds). Coordinates are
the integer room position (X, Y = feet):

1. Clear flags `0x28000800`. A ceiling walk at (X, Y-33) sets `0x20000000`.
2. Collision mode 1 when `0x13260 & 0x4000` and no bit-3 cell at (X+/-5, Y-7);
   otherwise mode 0 (no rewrite).
3. Walls, by the sign of vx + `+0x2C`: for each Y offset of the list at
   `+0x18` (count byte, then signed offsets), push out of a wall at X-8
   (`sub_0800207C`) or X+8 (`sub_08002058`); the first hit moves X by whole
   pixels, sets `0x40000` and either zeroes vx and `+0x50` (flag `0x80`) or
   sets vx = -vx/4 and negates `+0x50`. Lists in the ROM: `0x080E12DC`
   (-12, -20, -28) set at spawn, `0x080E12EA` (-6, -9) set by low-posture
   states, `0x080E12EF` (-14, -20, -32, -48), `0x080E12E4` (-6, -16, -28),
   `0x080E12E8` (-12).
4. A ceiling at (X+/-5, Y-20) sets `0x20008000`, else `0x8000` is cleared.
   `+0x16` counts down.
5. Bit-3 cells at (X+/-5, Y-8) set `0x1000000` (entering with vy > 1.5 calls
   `sub_0803319C(0)`; with `0x13260 & 0x8000`, vy /= 4); bit-3 cells at
   (X+/-5, Y-25) set `0xC00000` (and `0x800` without one at (X, Y-26)). This
   matches water but is not confirmed.
6. Walking: `+0x1C` = 1 when a floor slope is within 4 pixels ahead at Y+1.
7. Not grounded (`0x100000` clear) with vy <= 0: a solid or slope cell at the
   feet pushes Y up (platforms `0x01` let Soma rise through). Then vy < 0 or
   `+0x16 != 0` ends the pass.
8. Ground probes at (X, Y+1), (X-5, Y+1), (X+5, Y+1): `0x1000` stays set only
   if none is solid or a slope (feet on platforms only), slopes set `0x2000`
   (bit 2 clear) or `0x4000` (bit 2 set), and `+0x1D` keeps the steepest
   step (byte >> 6). The centre probe snaps Y onto the surface; when grounded,
   a probe at Y+7 snaps down (slopes, steps); side probes snap only off
   slopes. A platform catches falling feet only if they entered it this frame
   (penetration <= vy pixels + 2).
9. With contact and vy > 0 Soma lands: sets `0x100000`, clears `0x20017E`
   (`0x20031E` when the equipped weapon has property `0x2000`), `+0x14` = 0,
   sound `0xBB`, Y fraction cleared, vy = `+0x54` = 0; vy > 6.25 or flag
   `0x80` is a hard landing (`0x10000`, state 4), else state 0. Without
   contact `0x100000` is cleared.

The gravity routine `sub_08018B98` bumps the head with ceiling walks at
(X-5, Y-32) then (X+5, Y-32), skipped while falling with vx = 0; when rising,
vy = 0.0625 and `+0x54` = -0.125 (the `sub_08017CC8` special case needs flag
`0x10`).

Soma motion constants verified in the assembly (16.16 fixed point, pixels per
60 Hz frame, positive Y downward; `+0x4C` is Y velocity, `+0x54` a Y
acceleration term, `+0x14` an airborne frame counter, `+0x10` state flags):

| Behavior | Evidence | Values |
|---|---|---|
| Ground jump | `sub_08019180`: jump button (`gEwramData + 0x1339A`) pressed while grounded or while `+0x14 <= 3`; sound `0xB9`; sets airborne flag 2, `+0x14 = 16`, `+0x54 = 0` | vy = `0xFFFB0C00` = -4.953125; -1.234375 (`0xFFFEC400`) when flag `0x4000000` is set |
| High jump (state 5) | same routine, ability `sub_08032AB8(4)`, button `0x1339C` pressed while airborne or with Up held, flag `0x10` clear; sound `0xBA`; sets `0x12`, clears `0x100404`, `+0x54` = 0 | vy = -10.0, or -2.5 with flag `0x4000000`; state 0 again once vy > 0 |
| Mid-air jump | `+0x14 > 3` with ability `sub_08032AB8(2)` calls `sub_080190E0`: needs `0x800000`, or neither `4` nor `0x4000000`; jump pressed, `+0x16` = 0; requests animation 0x14, sets `4`, clears `0x10`, sound `0xB9` | vy = -4.125 with `+0x54` = +0.078125; with the slow-fall flag `0x400000` vy = -4.5 and `+0x54` = 0, only when vy >= 0 |
| Dive kick (state 7) | `sub_08017D90` at the end of the jump routine: ability 3, airborne, `+0x10 & 0x800004` = 4 (flag 4 is only set by the mid-air jump in the code read), Down held, jump pressed; sound `0xA9` | vy = +4.875, `+0x54` = +0.125, vx = +/-4.0 toward the held side (else friction -/+0.25); animation 0x26 when \|vx\| > 1.0, else 0x27. State 7 (`sub_08017F94`) returns to state 0 with the slow-fall flag, or bounces (vy -3.5, `+0x54` -0.125, sets `8`, clears `4`) after a hit flag `0x80000` |
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
| Slide (state 3) | `sub_0801B0D8` case 0: ability `sub_08032AB8(1)`, Down held and jump pressed, flags `0x1122` clear; sound `0xBD`, flags `0x20000420` set, `0x10000000` cleared | vx = 3.125 toward the facing side with `+0x50` = -/+0.09375 per frame. Case 3 applies the friction for 32 frames (`+0x0D` counts to 0x20, or jumps there when airborne), then clears `0x20` and returns to state 0; animation 0x2E on a step-1 slope going down, else 0x1F, keeping the frame and tick |
| Ceiling crash (state 6) | `sub_08017CC8` from a ceiling bump while rising with vy <= -4.9375 and `+0x10 & 0x90` = 0x10 (high jump); sound `0xB8`, screen shake | animation 0x25, vx = vy = `+0x54` = 0, gravity stops for the frame; case 6 returns to state 0 when the animation ends |
| Backdash | case 0: ability `sub_08032AB8(0)`, button `0x1339C` pressed without Up, flags `0x10008402` clear; sound `0xA9`, sets `0x10000000` | vx = 3.75 away from the facing side; friction -/+0.25 while `0x10000000` is set; the flag clears when its animation ends |
| Damage recoil | `sub_0801B0D8`, state `0x0F`, source X at `+0x131D8` | vx = 1.5 away from the source, vy = -2.0, `+0x54` = -0.0625, `+0x50` = -/+0.0078125 |

Ability bits live at `gEwramData + 0x13396` (`sub_08032AB8(bit)`): 0
backdash, 1 slide, 2 mid-air jump, 3 dive kick, 4 high jump; what grants
them (souls, game start) is not traced. Default buttons come from
`sub_0804C3C8` and the option table `0x08525504`: attack B (`0x13398`), jump
A (`0x1339A`), the backdash/high-jump button L (`0x1339C`) and guardian R
(`0x1339E`). During a high jump (flag `0x10`) the gravity routine also bumps
the head at y - 24 and clears `0x10` once vy exceeds 1.125 (0.5 with the
slow-fall flag). Airborne animations are skipped while `gEwramData + 0x131B8
& 4` is set (the dive kick sets it each frame); the end of `sub_0801B0D8`
(`_0801CF2C`) clears bits 1-7 and `0x800` of `0x131B8` and entity flags
`0x4080000`.

Earlier revisions called the slide a "probable backdash" and the case
`_0801C410` the air state; case 1 is an attack state (weapon animations,
vx kept only while airborne), and state 0 handles both ground and air.
An earlier revision of this table read the impulse flag as `0x400000`; the
assembly builds it as `0x80 << 0x13` = `0x4000000`, and `0x400000` is the
separate slow-fall flag. Its meaning (probably water) is unconfirmed.
Friction stops on a sign change only: a vx that lands exactly on zero keeps
its friction term for one more frame.

Player states (`sub_0801B0D8` jump table at `0x0801BA50`, 18 cases): each
frame integrates the position, runs the collision pass, dispatches on
`+0x0A`, then most cases call `sub_0802E0C4`. Movement part of the cases
read so far:

- Case 0, normal (ground and air, `_0801BA98`). Speed 1.5 (4.0 with
  `0x13260 & 0x400`). Without flags `0x10008400`: steering; on the ground,
  vx moving up a slope (vx > 0 with `0x4000`, vx < 0 with `0x2000`) becomes
  vx / 24, 20 or 18 * 16 for `+0x1D` = 1, 2, 3. During a backdash
  (`0x10000000`) directions only turn Soma and friction is -/+0.25. Crouched
  (`0x400`) or under a low ceiling (`0x8000`): no steering, the same slope
  rule with 23, 19, 17 and friction -/+0.15625. Then friction is applied.
  Grounded: `+0x0D` counts frames and, unless flags `0x1000001E`, Down,
  `0x8000` or `0x1325C & 1` set the crouch `0x400` (clearing `0x10000000`),
  which is released when none holds. Then the attack and slide checks, the
  jump routine `sub_08019180` and `sub_0801938C` (which also starts falls
  off ledges).
- Case 4, hard landing (`_0801C994`): friction -/+0.1875; with `0x10000`
  it keeps the crouch and returns to case 0 (clearing `0x10080`) when Soma
  is no longer grounded or `0x200000` is set. Every animation start clears
  `0x200000`, so it marks the end of the landing animation.
- `sub_08019180`: no jump with `0x20000000` or `0x160`. On the ground over
  platforms only (`0x1000`), Down + jump drops through: `+0x16` = 16 (32
  with `0x1000000`), airborne, vx = vy = 0, `+0x54` = -0.0625. Then the high
  jump (ability 4, state 5); a non-zero `+0x16` stops there. With
  `0x800000`, jump (unless `0x2000000`) sets vy -4.875 (or adds it when
  falling faster than 1.0). Otherwise the normal jump, or the ability-2
  mid-air jump; those paths end in `sub_08017D90` (ability 3: Down + jump in
  the air, state 7, vy +4.875).

`src/runtime/aos_soma.c` ports the rules of this table, the tile path of
the collision pass and the movement part of cases 0 and 4
(`aos_soma_update`) with the ability moves and states 3, 5, 6 and 7, except
attacks, recoil, hurtboxes, effects and moving platforms
(`gEwramData + 0x1316C`, `+ 0x131B4`);
`tests/test_aos_soma.c` checks them without a ROM (a held jump rises about
56.7 pixels over 56 frames, a tapped jump about 9.7 pixels).

Animations. `sub_0803F2C8(entity, id, mode, loop)` starts animation `id`
of the entity's descriptor (the table the sprite library exports): it
stores the pointer at `+0x68`, the id at `+0x6D`, clears the frame `+0x6E`
and tick `+0x6F`, and sets `+0x6C` = loop | 8. The player always uses mode
3 (`sub_0803EFF0`: tile upload, then the step chosen by the animation's
encoding through the table at `0x080E2B34`); Soma's animations use encoding
1, stepped by `sub_0803EC34`: the tick counts up to the frame duration,
then the frame advances; after the last frame a looping animation restarts
and a non-looping one holds its last frame and sets `+0x6C & 4`. At the
start of each player frame `sub_0801B0D8` clears `0x240000` and, when that
bit is set, sets `0x200000` and ends the backdash (`0x10000000`). After the
state routine, a pending one-shot animation in `+0x20` (`0xFF` = none) is
started if it is not current, and dropped once `0x200000` is seen; then
`sub_0803F17C` steps the animation. Every direct animation change follows
the same pattern (start unless current, `+0x20` = `0xFF`, clear
`0x200000`) and also sets a hurtbox with `sub_080428B4` (standing -6, -32,
12 x 28 from `0x080E12F8`; low -5, -16, 12 x 14 from `0x080E12FC`/`0x080E1300`;
slide -8, -12, 16 x 12 from `0x080E1304`) and a palette from the byte table
at `0x080E126C`.

Animation ids used by the ported code: 0 idle, 1 walk, 2 crouch, 3 fast
movement, 9 crouch down, 0x0A stand up, 0x0B forward jump, 0x0C fall, 0x0D
landing, 0x13 hard landing, 0x17 look up (Up held 4 frames), 0x18 turn,
0x19 stop (from |vx| > 1.0), 0x1A walk start (then 1), 0x24 flag-0x10 jump,
0x29 turn with `0x800000`, 0x32 vertical jump. Airborne animations are
chosen at the end of the gravity routine (`_08018E4A`), grounded ones in
case 0 (`_0801BEAC` .. `_0801C298`), landing ones in the collision pass.

Wall probes (`+0x18`) are chosen at the end of each frame (`_0801CD80`):
state 3 or `0x8000` uses `0x080E12E8`; `0x800000` uses `0x080E12E4`; on the
ground `0x080E12DC`; in the air the list at `gEwramData + 0x13218`
(initialized to 4 probes -8, -12, -20, -28), whose first offset becomes -12
on a slope contact or when |vx| > 2 pixels while rising, else
-(|vy| / 2) - 4 pixels.

Room exits. Entity positions are screen-relative: the room position is
the BG1 scroll plus the entity position (`GetEntityRoomXPositionInteger`).
Each frame of in-game mode 1, `sub_08011A44` reports an exit when the room
X (unsigned, so negative values count) exceeds `W * 256` (`0xF0` for one
screen), when Y is below `0x30` or above `H * 256 - 0x30` (`0xD0` for one
screen), or when the BG1 byte under the feet is `0xF0` (`sub_08001FE8`;
only rooms 5/5 and 9/1 use it, behind doors). `sub_08010244` then starts
mode 3: within screen Y `0x31..0xCF`, vx pointing back into the room
(by screen half) and the friction are cleared, and vy < -5.0 becomes -1.0.
`sub_08010350` picks the room's transition entry (16 bytes: target room
pointer, source screen X and Y as `s8`, X adjustment at +6, unused halfword
at +8, BG1 X and Y at +0xA/+0xC, unused +0xE) whose screen matches
X >> 8 and the row (multi-screen rooms: `(Y - 0x30) >> 8` above the top
bound, `(Y + 0x30) >> 8` below the bottom one, else Y >> 8; one-screen
rooms: `(Y - 0x30) >> 8`, plus one when Y - 0x30 > 0xA0; a one-screen room
also maps X > 0xF0 to column 1). It moves every entity so Soma is on
screen at local X (X + 0xF0 when negative, else X & 0xFF, minus 0xF0 above
0xF0) plus the adjustment, and local Y (Y + 0x70 below 0x30, else
(Y - 0x30) & 0xFF, minus 0xA0 above 0xA0). `sub_0800F9EC` loads the target
with BG1 at `sub_0800ED5C(load_x)` and `sub_0800EE54(load_y)` = load_y +
0x30 (BG1 scrolls 1:1); `sub_0803FBBC` pins one-screen axes to X 0 and Y
0x30. The arrival is therefore X = (W > 1 ? load_x : 0) + local X +
adjustment and Y = (H > 1 ? load_y + 0x30 : 0x30) + local Y.
`src/runtime/aos_room.c` ports these rules. `fusion_aria_runtime
--audit-transitions` leaves every exported room through each entry: all
725 entries match an exit, all 723 edge transitions arrive inside their
target, and the 2 exit-cell transitions arrive outside when leaving from
the first exit cell (X = 240), presumably because their door entities,
which are not ported, control that crossing. Walking from 0/6 into 0/9
arrives on the floor (Y 159 to 1439).

Animation palettes: every direct animation change compares the byte
`0x080E126C[id]` with `+0x26` and, when it differs, loads that bank of
Soma's palette descriptor `0x082097D4` into OBJ palette 0 with
`sub_0803C7B4(descriptor, bank, 1, 0)` (32 bytes at `+4 + bank * 0x20`).
Soma's 83 animations use bank 0 except 0x21-0x22 (bank 2, a white
silhouette), 0x2A-0x2B (bank 3, a red demon), 0x49-0x4A (bank 4, a grey
Soma) and 0x4B (bank 5); banks 1 and 6 are not referenced by this table.
One-shot requests through `+0x20` do not reload the palette, so they keep the
previous one; `scripts/aos_soma_pipeline.py` colorizes each animation with
its own bank.

`fusion_aria_runtime` runs these rules with Soma's library frames
(README). Next steps: the hurtboxes, the doors and the transition rooms of `sUnk_0850E968` (an Aria
entity system), then attacks.
