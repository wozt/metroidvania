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

Attacks. `sub_080197B4` (called from case 0 after the grounded animations,
before the slide) starts an attack when the attack button (default B,
`0x13398`) is pressed or `0x1325C & 2` is set, flags `0x160` are clear, the
weapon entity slot `gEwramData + 0x1311C` is empty, `sub_080230A8` creates
the weapon entity (update `sUnk_084F124C[class]`) and the weapon class is not
5. The equipped weapon record is `sUnk_08505D3C[weapon]` (0x1C bytes: item id
+0, class +8, flags +0x10, variant +0x16; 59 records, item ids 0x20-0x5A) or
the unarmed record `sUnk_084F1270` when `+0x13268` is 0xFF. The body
animation is `sub_080233BC(posture)`: tables `sUnk_084F1238[0..2]` (standing,
crouched, airborne) indexed by class * 3 + variant, `[3..4]` (standing and
crouched recovery) by class. Airborne attacks set `0x40`; all set `0x20` and
state 1. Case 1 (`_0801C410`) steers only in the air (without turning),
brakes on the ground, converts an air attack into the standing animation on
landing (same frame) when the weapon has flag `0x2000` (`sub_08023424`),
and ends with the animation (clears `0x200060`; on the ground requests the
recovery animation) or with a backdash. Landing clears `0x20031E` instead of
`0x20017E` for such weapons and keeps the attack state; otherwise a landing
ends the attack. The weapon entity follows Soma, takes its hitbox from his
animation frame data and deletes itself when `0x20` clears; its sprites,
damage and the subweapon/soul attack `sub_08019478` are not ported.
`scripts/aos_weapons.py` exports the records and their animations privately.

Weapon entity (`sub_080221CC`, classes 0, 2 and 3): its tile sheet is
`sUnk_084F10C0[record + 0x12]`, its frame/animation descriptor
`sUnk_084F117C[record + 0x13]` (u16 frame record count, u16 animation count,
16-byte frame records, a word, the animation table), its palette bank
`record + 0x15` of `0x082098B8`, and it plays animation `record + 0x14` once
in mode 0. It sits at Soma's position (13 pixels lower when he was
crouched, kept in `+0x4E`), faces as he did when it was created, and is
deleted when its animation ends or Soma's `0x20` clears (both free the slot
`0x1311C`). On every frame change, a frame record with byte `+4` set enables
the hitbox at its pointer `+8` (signed x, y, width, height) through
`sub_08042848`; otherwise the hitbox is off. OAM components are built by
`sub_0804311C` (ARM code copied to IWRAM `0x03004570`, called through
`0x03004564`): byte `+8` is shape | size << 4, `+9` selects an alternate tile
base only in animation mode 2, `+0xA` bit 0 flips vertically and bit 1
inverts the entity's horizontal flip, `+0xB` masks a palette offset; when the
entity faces left, components are mirrored around the record's X.

Class 1 (`sub_080224BC`, records 18 to 39) shares the graphics setup but
swings: `+0x1C` (the affine angle) starts at -0x3000 and `+0x20` is 0x980
with `+0x0B` = 16 and `+0x0C` = 5 for variant 0, else 0xCC0, 12 and 4 (both
negated when facing left); `+0x19` = `0x080E13B8[variant]` (26, 21, 21) is
its lifetime and `+0x4E` = -25 (-13 with Soma's `0x400`). Each frame the
timer `+0x0D` grows, the entity sits at Soma's Y + `+0x4E` and X + min(timer
/ `+0x0C` + 1, 3) (negated when facing left), the angle grows while the
timer is below `+0x0B`, the animation steps (it is not deleted at its end)
and the entity is deleted when the timer reaches `+0x19` or Soma's `0x20`
clears; on frame changes the hitbox comes from the frame record, unrotated.
`sub_0803E058(angle, sx, sy, hflip, vflip, slot)` writes the affine
parameters pa = cos / sx, pb = sin / sx, pc = -sin / sy, pd = cos / sy and
negates pa and pb for hflip (pc, pd for vflip): the texture is flipped,
then turned clockwise by the angle on screen (the axe uses the same call).
Class 4 (`sub_08022A54`, records 55 and 56: guns) draws its sprite with
`sub_08023508`, which puts the entity at Soma's X and Y + `+0x4C` (12 when
crouched, else 0) only for the draw: the entity itself is the shot.
Creation sets `+0x50` = 8.0 (negated facing left) and adds it to `+0x48`
once, sets its attack box (slot + 0, the box `sub_08041864` reads for the
attacker; `sub_080428F0` sets the hurtbox at slot + 4) by
`sub_08042884(e, 0x0810FCF8)`, which copies the pointer's own bytes F8 FC 10
08 as the box (-8, -4, 16, 8), keeps that box off (+ 0x72 bit 1), and
creates a casing (`sub_080228F8`, placed by
`sub_08031EA4(e, soma, 8, +0x4E - 26)`: 8 pixels ahead, mirrored like Soma;
vx = (r & 0x7FFF) - 1.0 negated when facing left, vy = -1.25 - (r & 0x7FFF),
the draws only when it is created). Each frame: the timer grows; at 3 (and
no wall yet) the attack box goes on; above 3 `+0x48` += `+0x50`; the shot
sits at Soma + (`+0x48`, `+0x4C` - 28); until `+0x1C` is set it probes the
wall ahead (`sub_080020D8` moving right, `sub_0800210C` left, i.e. the
pushes of `sub_080022E8` / `sub_080022A8` in mode 0) and on a push creates
the impact `sub_08045CEC(x + push, y, 7, 8)`, sets `+0x1C` (sound 0x14E) and
afterwards keeps the attack box off; it is deleted when its animation ends.
The casing checks its cell first (bits 8 and 1: deleted), shows common
frame 0x35 with row 8 and ay 0x3000, moves with `sub_0806D430` /
`sub_0806D460`, bounces once on a solid cell 2 pixels below (vy = -(vy / 2),
vx += `sub_0802D5EC(0x4000)`, sound 0x14F) and is deleted off screen
(`sub_08068AD4(e, -16, -16, 0x110, 0xC0)`). Class 5 (`sub_08022DEC`,
record 58) is class 0 without the deletion when Soma's `0x20` clears; the
attack refuses class 5 and only `sub_08033F58`, a player copy driven by
`gEwramData + 0x4DC`, calls `sub_080230A8` otherwise. `src/runtime/aos_weapon.c`
ports classes 0 to 5 and
`src/runtime/aos_effect.c` the shared effects; the runtime draws the blade
or gun, rotating class 1 about the entity position (`--hitboxes` outlines
the active hitbox).

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

Room objects. `sub_0800F4F8` walks the room's entity records (sorted by
X) and `sub_0800F1FC` spawns those whose X lies within the BG1 X - 80 ..
X + 320 window, once per visit (bit `+4` of the record in `gEwramData +
0x3D0`). Kind 2 records use the create/update tables `sUnk_084F0DF8` /
`sUnk_084F0ED8` (`Object00Create` ... `Object37Update`).

Collision override: `sub_08002200(x, y)` marks the 16x16 block in the
bitmap `gEwramData + 0xF0C0` and sets `unk_A074_6`; `sub_08002248` clears
it; `sub_08002278` clears the bitmap on every room load. While `unk_A074_6`
is set, `sub_08001A00` returns `0x03` for an empty or bit-3 cell whose block
is marked.

Wooden door (special object 0x00, `Object00Create` / `Object00Update`):
`sub_0804D8F0` loads its graphics (`0x081CBE0C`, palette `0x08209AE0`,
frames `0x0820F160`, parameter 0 choosing `0x08525564` or `0x08525574`) and
faces it left when its screen X is above `0x78`. Contact is `sub_08068AD4`:
`(u16)(door.x - (soma.x - 20)) <= 40` and `(u16)(door.y - soma.y) <= h`.
If Soma overlaps it at creation (h 40), the door starts open (`+0x14` =
`0x4000`, state 10) and walks him away from it until contact ends (sound
`0x114`), then closes (state 11, `+0x14` -= `0x1A0` per frame) and marks its
blocks; otherwise it marks the blocks at y - 8, y - 24 and y - 40 at once.
Closed (state 0), contact with h 2 while Soma faces the door, without
`0x13260 & 0x200` or a backdash, locks his input (`SetPlayerInput(1, 0)`),
plays `0x115` and opens it (state 1: `+0x14` += `0x100` for 64 frames, then
clears the blocks); state 2 walks Soma through it. While Soma passes (from
the opening or the open start until it closes again) the door sets
`gEwramData + 0x42C` bits 0x101 and clears `gEwramData + 0xA074` bit 1
through `sub_0800C5A8(0)` (`sub_0800C5A8(1)` restores it); every update
hides it (`+0x58` bit 0x20) unless `sub_08068AD4(e, -32, 0, 0x130, 0xD0)`
holds on its screen position. With `+0x59` bit 2 clear it enters state
`0x63`: input locked, the `0x42C` bits cleared, the entity deleted and its
graphics slots freed (`sub_0803E6BC`). `src/runtime/aos_door.c` ports this
except state `0x63` (the setter of that bit is not traced); the global
flags are exposed as `holding`, their readers are not traced.

Door graphics: `sub_0806E0D0` is the generic object setup (tiles through
`sub_0803B800`, palette through `sub_08068264`, frame list through
`sub_0803B924`, draw function `sub_0803B9D0`). The door's tile descriptor
`0x081CBE0C` has encoding 1 (LZ77 tiles at `0x085F94A0`, 16 x 8 tiles); its
frame list `0x0820F160` holds 8 frames of two OAM components (12 bytes: x,
y, reserved, source x and y, width, height, then the OAM size << 4 | shape).
`+0x65` selects frame 0 (parameter 0, a wooden door) or 5 (a green metal
door); the draw path is not affine, so the swing value `+0x14` is only the
open/close timer. `sub_0803CC70` registers a palette script that
`sub_0803C150` plays: a u16 entry count, then (bank, duration) entries copied
into the object's palette slot in a loop; 0x08525564 cycles banks 3, 4, 5
(7 frames each), 0x08525574 banks 9, 10, 11, 10 (10 frames).
`scripts/aos_object_sprites.py` exports each style as a looping sequence. The
runtime mirrors doors that face left, as Soma's frames are mirrored; the
generic draw (`0x03004564` in IWRAM) is not traced, but this places both
edge doors inside their doorway art (rooms 0/3 and 0/16).

Enemies. Kind 1 records spawn through `sUnk_080E9644[id]` (0x24 bytes:
create, update, then stats: HP at +0xC, +0xE, contact damage at +0x13, +0x14,
flags at +0x1C, +0x20). `sub_0806B04C` copies the stats into the entity (HP
`+0x34`). The shared update starts with `sub_0806CC20`: the AI only runs
while the enemy's screen position is within X -128..368 and Y -64..224, and a
hit-stun check (`sub_0806AD24`) can take over; it ends with `sub_0806DF20`
(animation step, hitbox refresh) and `sub_0806E314` (collision against the
player and his weapons through `sub_080421AC`, which calls the registered
callbacks: `sub_0806E1B8` damages the player, the enemy's own callback
receives hits through `sub_0806E218`, which subtracts the damage of
`sub_0806B7D8` from the attacker's power `sub_08021530` and reports death).
Facing (`+0x58` bit 0x40) means mirrored: enemy sprites face left;
`sub_0806BC40` sets it when the player is to the right, `sub_0806E120` signs a
speed by it. `RandomNumberGenerator` is `r = (r >> 8) * 0x3243F6AD +
0x1B0CB175`; the sine table of `sub_080009E4` equals `floor(sin(i * pi /
2048) * 65536)` for all 1024 entries.

Bat (enemy 0x00): `EnemyBatCreate` loads tiles `0x081F422C`, palette
`0x0820BD4C` bank 0 and frames `0x0824B2C4`, and climbs 8 pixels at a time
until a ceiling walk hits, then hangs. State 1 (`sub_080AD364`) faces the
player; with him in the 0xE0 x 0xA0 box centred below it, it shows
animation 1, and in the 0xC0 x 0x70 box it attacks; otherwise it stretches
with probability 1/128 per frame. State 2 (`sub_080AD44C`): a hop away at
0.5 for 33 frames, a dive at 0.375 x 0.375 toward him (stepping the
animation twice per frame) until within 39 pixels of his height, then a
flight at 0.75 accelerating by 0x50 per frame with a sine wave (`+0x14`
advancing 0x200 per frame) until it leaves the screen bounds read from
the integer part of vx (+ 40 / + 240), which deletes it. State 3 is the
death below. A bat created while gEwramData + 0x8E bit 0x40 is set (by
`sub_0804C404`, reached through the table `0x085254D8`; cleared by
`sub_0804CC14`; meaning not traced) skips the ceiling search and the hit
callback and stays in state 4 (`sub_080AD6AC`): animation 1, no activity
window, no collision pass. `src/runtime/aos_enemy.c` ports the framework parts above and the
bat; `scripts/aos_object_sprites.py` exports its animations and boxes.

Collisions and damage. Every entity has a collision block at `+0x70`
(`sub_0804277C`): type (`+0x70`: player 1, weapon 3, secondary weapon 5,
enemy 8), a flag `+0x71` (1: only the attack box counts), `+0x72` bit 1
(attack box off) and bit 2 (hurtbox off), its slot `+0x73`, three
recent-hit types `+0x74..0x76` with cooldowns `+0x78..0x7A`, and two
callbacks (`+0x7C` when it hits, `+0x80` when it is hit). Per slot,
`0x03001A58 + slot * 8` holds the attack box (`sub_08042848`) and `+4` the
hurtbox (`sub_080428B4`), each (signed x, y, width, height); a width or height
of 1 disables a box. `sub_08042584` turns the player's boxes into world
rectangles (`x2 = x1 + width`, mirrored by `+0x58` bits 0x40/0x80); other
boxes become inclusive rectangles (`sub_080415A8`) and `sub_080417A0` /
`sub_08041688` test overlaps inclusively. Enemies refresh their boxes from
the frame record on each frame change (`sub_0806B1FC`: `+4` = 0 none, 1 one
box for both, 2 hurtbox then attack box). On screen, `sub_0806E314` runs
`sub_080421AC`: the enemy's attack box against the player (`sub_08041D54`),
the player's attack box against the enemy (`sub_08041BDC`, callbacks), the
weapon entity against the enemy (`sub_08041864`, the weapon callback
`sub_0802346C` sets `0x80000`, then the enemy callback), then the player's
projectiles; each test needs the attacker type absent from the victim's
recent types and a free slot, and records it with a cooldown of
`0x080E33F0[type] + 1` frames (0, 0, 8, 28, 30, 0, 0, 0, 80, 60, 120), or for
types 3/5/6/9 the values at `gEwramData + 0x1307C..0x1307F` (the weapon's
record `+0x17`, written by `sub_08042A54`). The player hit by an enemy is
therefore immune to enemy attacks for 81 frames. `sub_080426B0` counts the
cooldowns down after every entity update. When the enemy hit the player,
the pass calls the player's `+0x80` callback (none) and the enemy's
`sub_0806E1B8`, which builds a damage record for `sub_08021654`: skipped
while the player's `+0x1E` is set, damage = (attack * 4 - DEF * 2) / 2
(halved again for resisted elements, at least 1), HP drops, and the source
position and knockback type are stored for the player update
(`0x131D4..0x131DC`; a hit above a quarter of max HP becomes type 1).
On the next frame, after the integration and before the collision pass,
`sub_0801B0D8` reacts (`_0801B3C2` .. `_0801B9C8`). A type-0 hit on the
ground makes Soma flinch: animation 0x0F when crouched (low hurtbox), else
0x0E when he faces the source and 0x10 otherwise; his enemy-type cooldown
becomes 50 frames, flag 0x80 is set, sound 0x19B, state 12. Otherwise
(type 1, or airborne) he is knocked back: animation 0x11 when he faces the
source, else 0x10; vx = 1.5 away from it with friction 0.0078 toward zero,
vy = -2.0, `+0x54` = -0.0625, flags `0x82`, sound 0x195, state 13. State 12
(`_0801CBE2`) brakes by 0.5 per frame and returns to state 0 when the
animation ends. State 13 (`sub_080199A0`) brakes by 0.0625 in bit-3 cells
and ends when Soma is no longer airborne (his landing is hard because of
flag 0x80, so he crouches in state 4) or, in such cells, when the animation
ends; with `0x13260 & 0x400000` a jump press recovers in the air. At 0 HP
the reaction is the death `sub_0801AF20`: animation 0x33, hurtbox off
(`+0x72 |= 4`), vx = -1.0 when facing right (+1.0 when facing left), vy =
-1.5, `+0x54` = +0.125, sound 0x197, state 16; `sub_0801B03C` then brakes vx
by 0.0078 and vy by `+0x54`, each stopping at zero (no gravity), and spawns
particles every 4 frames. The game-over mode (`0x42C |= 0x10`, set by
`sub_08021654`), the curse (`0x13270 & 4`), types 2 and 3 and the
`0x13260 & 0x20200` immunity are not ported. Soma registers as
type 1 with the attack box off; his hurtbox changes with his animations:
standing -6, -32, 12 x 28 (`0x080E12F8`), low -5, -16, 12 x 14
(`0x080E12FC` / `0x080E1300`: crouch, jumps, falls, hard landing), slide -8,
-12, 16 x 12 (`0x080E1304`). Enemy damage (`sub_0806E218` ->
`sub_0806B7D8`): the attacker's power is `sub_08021530(type)` (weapon:
ATK * 16 / 16 with ATK = STR + equipment, `sub_08021F64`; DEF = CON / 2 +
equipment); a weakness (`record + 0x1A` & attack elements, low 6 bits) gives
x1.25 for element 1 and x2 otherwise, a matching resistance (`+0x1C`)
halves it; damage = ((power - DEF / 2) * (256 - DEF)) >> 8, at least 1,
with DEF the enemy's `record + 0x14`; `+0x2D` = 8 and HP `+0x34` drops; at
0 HP the enemy's callback switches to its death. The special weakness bits
above 0x3F (`+0x38` rules), damage numbers and hit sparks are not ported.
The bat dies in state 3 (`sub_080AD5B8`): animation 2 frame 2, vy 0.5,
vertical flip, 64 frames of fall during which `sub_0806BE74` blinks it with
the pattern `0x08118CE0` (frames below 40) and spawns a flame every 15
frames, then deletes it.

Death flames. `sub_0806BE74(e, period, w, h)` spawns, when the death timer
`+0x0D` is a multiple of the period, an entity `sub_0806E524`
(`EntityCreateInRange(0x49, 0xCF)`) at `x + rand % w - w / 2`,
`y - 6 + rand % h - h / 2` (16.16), mirrored by a third draw only when the
slot was found, priority `+0x5C - 1`; the bat uses (15, 8, 6), the crow
(6, 8, 8). The generic death `sub_0806AEAC` does the same every 4 frames
over 16 x 12 pixels. The flame plays animation 0 of the shared frame list
`0x0820ED60` (5 frames, 20 ticks) once, from the common tile sheet
`0x081C15F4` (languages 2 and 3 use `0x0826F8A4` / `0x082718A8`) with OBJ
palette row 7, which `sub_0800C5D8` fills with bank 3 of `0x082099FC`
(`sUnk_084F0DD8` maps rows 4 to 9 and 14); it rises 0.5 per frame after
its first and is deleted when the animation ends. The export names it
`Effect/flame/anim_0`; the runtime updates flames after the enemies. `src/runtime/aos_combat.c` ports the boxes,
overlaps, slots and formulas; the new-game stats are not traced, so the
runtime takes ATK, DEF and HP as diagnostic options.

Zombie (enemy 0x01, `EnemyZombieCreate` / `EnemyZombieUpdate`): tiles
`0x081CBF8C`, palette `0x0820A62C`, frames `0x082178B8` (`sub_0807B404`);
animations 0 rise, 1 and 4 walk, 2 sink, 3 death. A record with parameter 1
non-zero is a spawner (state 3: hidden, no boxes, snapped to the floor): one
frame in 32 (`RandomNumberGenerator & 0x1F`), while fewer than parameter 0
of its zombies exist (`+0x18`), it creates one at a random screen X (`% 240`)
more than 32 pixels from the player, at its own Y; a spawned zombie keeps a
link to it and decrements the count when deleted. A zombie snaps onto the
floor (`sub_0806AF98`), faces the player (`sub_0806CF2C`) and vanishes at
once (state 1, one frame, HP 0) on a slope byte (`sub_0806D104`) or when the
snap moved it 8 pixels up. `sub_0807AD28` (state 0): step 0 waits for the
rise animation, then picks walk animation 1 or 4 at random, a timer
`+0x1C` of 600 + (random & 0x1FF) frames; step 1 sets vx = 2.0 toward its
facing, vy = 1.0 and `+0x54` = 0x2800 at the start of every animation frame,
damps vx by 3/4 per frame (`sub_0806D490`) and moves with the walker
collision `sub_08069A00` (X then Y, each clamped to 8.0: walls at x +/- 8,
y - 10, the ceiling at y - 32, the floor, where vy resets to 1.0); a wall, a
slope byte or the end of the timer pauses the animation (`+0x6C |= 2`) and
starts step 2, which waits 60 frames, then plays the sink animation; step
3 vanishes when it ends. With the player in an 80 x 64 box in front of it,
the animation tick advances twice on even timer values. The attack box is
off while rising and sinking. `EnemyZombieUpdate` also makes it vanish
outside the screen margins of `sub_0806D128` (table `0x08118D08`, entry 4:
48 x 48). Hit callback `sub_0807B0DC`: damage as above; at 0 HP, sound
0x70, death animation 3 and the generic death `sub_0806AEAC` (state 1: a
40-frame blink with the same pattern, flames every 4 frames, then the
deletion; `sub_080683BC` is not ported). At 0 HP it also throws two pieces
of `sub_0807B258` (sprite frame 0xD at x - 4 + (r & 0x7FFFF), y - 26 +
(r & 0x3FFFF); once created, vx = (r & 0x1FFFF) - 1.0, vy = -1.0 -
(r & 0xFFFF), ay 0x1800, moved by `sub_0806D3D8` and deleted below the
zombie's y). While walking (step 1), on walk frame 0 it drops a drip
(`sub_0807B1CC`: frame 0xD at (+9 / -9, -24) for animation 4, (-9 / +7,
-22) for animation 1, unmirrored / mirrored; ay 0xA00 through
`sub_0806D384` / `sub_0806D3CC`, 31 updates) and on frames 0 and 2 a puddle
(`sub_0807B33C`: frame 0 at its feet, priority + 1, 121 updates), each once
until frames 1 or 3 re-arm them (`+0x14`, `+0x15`). `EnemyZombieUpdate`
calls `sub_0806AD24`: alive with `+0x2D` set (8 after a hit), it counts it
down and only runs the collision pass (not while dying). Under the global
`0x8E & 0x40` it is created in state 2 (`sub_0807B034`) without the
collision block, the floor snap or the facing: once the rise ends it plays
walk animation 1 or 4 in place, dropping a drip ((-7 / +7, -22) for
animation 1) whenever the animation is at frame 0, tick 0, with no
collision pass. Sounds, palette bits and the tracking list of
`sub_08021924` are not ported.

Blue crow (enemy 0x09, `EnemyBlueCrowCreate` / `EnemyBlueCrowUpdate`):
tiles `0x081F4274`, palette `0x0820BF10` bank 0, frames `0x0824EC04`;
animations 0 perch, 1 fly, 2 flap in place, 3 death. Unlike the bat and the
zombie, its update has no activity window and runs the collision pass
(`sub_080421AC`) without an on-screen test. `sub_080C9AF4` (state 1): step 0
faces the player and perches; step 1 waits until he is within 60 pixels on
both axes (`sub_0806BF78`, sound 0x8B), then step 3 faces him every frame:
substep 0 stops and flaps for 33 frames; substep 2 waits until he is more
than 72 pixels away horizontally and more than 16 vertically; substep 3 flies
(animation 1) toward the point 42 pixels above him and 32 pixels behind him
(to his left when he faces right, from his `+0x58` bit 0x40) at 1.625 per
frame, and on arrival flaps in place again (substep 2). The homing
`sub_080694B8` takes the whole-pixel distance to the target (rounded toward
zero), the BIOS `ArcTan2` of it, and sets vx = cos * speed, vy = sin * speed
with the product `sub_0803E86C` (24-bit operands, rounded toward zero to a
multiple of 0x100); it reports arrival when the BIOS `Sqrt` of the squared
distance, as 16.16, is at most the speed. The `gEwramData + 0xA094` offsets it
adds to the entity are also added to the crow's targets, so they cancel out.
`ArcTan2` is ported with the polynomial of mGBA's high-level BIOS, not
checked against the BIOS itself. Step 2 (a glide built from the current
velocity) is not entered by any code read and is not ported. Hit callback
`sub_080CA030`: at 0 HP, state 2 (`sub_080C9D68`): sound 0x72, death
animation 3, vy = 0.5 with no gravity, and the blink and deletion of
`sub_0806BE74` over 64 frames (a flame every 6 frames over 8 x 8 pixels).
On its first frame `sub_080C9E2C` creates two feathers
(`EntityCreateInRange(0x1B, 0x48, sub_080C9ED4)`: sprite frame 8 at
(x + 8, y + 4) and, mirrored, at (x - 8, y - 4) in whole pixels) that drift
at 0.25 sideways and 0.125 down, losing 0x300 of X speed per frame, and
blink on the timer's bit 0 for 49 + 25 frames (the palette bits set
between the two are not ported). `sub_0806E664(x, y, 1)` adds a blood
splash at the crow, then 4 pixels lower whenever the timer is a multiple
of 16: `sub_0806B5DC` reads (animation, palette row) from `0x08526D0C`
(type 1: animation 13 of `0x0820ED60`, row 8 = bank 4 of `0x082099FC`),
priority 3, mirrored by a random draw, deleted when the animation ends.
While alive, `sub_0806AD24` stuns it: with `+0x2D` (8 after a hit) non-zero,
`sub_0806AE54` counts it down and writes the flash palette bits, and the
flight and animation step are skipped (the collision pass still runs);
with the global pause `0x4BE` (unless `0x8E & 0x40`) and the record's
`+0x1C` bit 0x400, or `+0x38` bit 0x100, it is also stopped (not ported).
`sub_08021924` registers it in a four-entry tracking list (not ported).
Under the global `0x8E & 0x40` the crow starts in state 3 (`sub_080C9FFC`:
animation 0 stepped in place, no hit callback, no collision pass). Sounds,
the `+0x5A` palette bits and `sub_080683BC` are not ported.

Red crow (enemy 0x2D, `EnemyRedCrowCreate` / `EnemyRedCrowUpdate`): the
blue crow's sheet, frames and palette `0x0820BF10` with bank 1; its update,
death (`sub_080CA308`), feathers (`sub_080CA3CC` / `sub_080CA474`) and inert
state 3 copy the blue crow's. Its flight `sub_080CA0D4`: step 0 clears the
round counter `+0x14`, faces the player and perches (animation 0); step 1
waits for him within 120 pixels on both axes (`sub_0806BF78`), then step 3
counts a round, hovers still (animation 2) for 128 frames, at 64 left
facing him (odd rounds) or turning away (even), then swoops (step 2:
animation 1, sound 0x8B, vx 0.5 toward the facing with `+0x50` 0x1000 that
way, vy 2.0 with `+0x54` -0x1000, for |vy / `+0x54`| * 2 = 64 frames) and
hovers again; after an odd round other than the first with the player
beyond 120 pixels it waits for the animation's cycle, plays animation 4
once and perches again (step 0). Each frame it moves by its velocity, then
adds its accelerations.

The constructor-table audit also connected an eight-enemy batch. Merman
(0x03), Tiny Devil (0x10), Imp (0x31) and Ripper (0x4A) currently stop at
their native identities, graphics, palettes, animation descriptors, frame
boxes, room records, contact combat, basic locomotion and global inert states;
their specialized attacks remain partial. Tiny Devil and Imp share one sheet
with palette banks 1 and 0, including the per-component OBJ palette-row byte
used by their OAM records.

Harpy (0x38) and Siren (0x0F) share `EnemyHarpyUpdate`. They idle facing Soma,
choose between the native animation-2 flight toward his position 24 pixels
above and the animation-1 ranged attack, brake back to idle, recoil when hit,
and fall through the 40-frame flame/blink death with four fragments. Harpy's
two-stage volley uses frames 12/10/8 followed by 11/9 at speed 0.75; Siren uses
its palette-bank-1 sheet and fires the frames 14/13/14 angular notes, whose
heading bends for 16 frames in each direction. Each shot carries its native
6x6 box, can be struck and leaves at the activity margin.

Gorgon (0x33) and Catoblepas (0x1C) share `EnemyGorgonUpdate`, with palette
banks 0 and 1. They patrol on the floor, turn at collision, wind up through
animation 2 when Soma is within 80 by 24 pixels, then emit the 21-cloud
30..70-frame breath stream. A cloud changes from static frame 25 to 26 at 25
frames, checks its native 14x14 box on alternating frames and expires at 40.
Death emits static frames 17..22 before the 60-frame flame/blink lifetime.
The exact hand-authored native gait-frame table is represented by the exported
animation sequences; sounds, palette flashes and native slot ordering remain
outside the runtime.

Killer Doll (enemy 0x1F, `EnemyKillerDollCreate` /
`EnemyKillerDollUpdate`): tiles `0x081F426C`, palette `0x0820BEEC` bank 0
and frames `0x0824DB0C`. Creation starts animation 0, snaps it to the floor
and seeds the attack wait (+ 0x14) to 128..2175 frames, the current leg (+
0x16) to 32..287 and the flight timer (+ 0x18) to 256..767. `sub_080C7CE4`
wakes it when the player is within 60 pixels on both axes, plays animation
1 once and rises at 0.25 through animation 2 for 34 frames. Animation 3 is
the pause between flight legs. Each leg is chosen with r & 3: zero flies
horizontally away from the player at 1.0, while the other values fly
diagonally toward him at 0.53125; `sub_080C86A0` reflects it at the room
edges and stops the applicable speed at walls, floors and ceilings. With
the attack wait expired, the player ahead and within 40 pixels on both
axes, animation 5 sends it at 1.0 toward the player's position 16 pixels
higher until it crosses his X. When the flight timer expires it sinks into
a solid cell, rests in animation 6 for 64..127 frames and rises again with
a new 512..1535-frame flight timer.

Its hit callback `sub_080C81AC` hides it and `sub_080C8968` creates five
pieces from `0x08528AC4` (sprite frames 13..17). `sub_080C82F4` applies
each record's velocity and timer; piece 1 alone bounces against the floor
and stops when slow, while every piece blinks near the end and emits the
flame and dust of `sub_0806D5C0` at its record's intervals. Each keeps a
looping death-flame glow (`sub_080C8244`) 16 pixels above it, and the hidden
parent leaves only after all five pieces and their glows finish. Under
global `0x8E & 0x40`, `sub_080C8914` only loops animation 0. Sounds 0x6E
and 0x153, palette flags, the tracking list and `sub_080683BC` rewards are
not ported.

Lightning Doll (enemy 0x3B, `EnemyLightningDollCreate` /
`EnemyLightningDollUpdate`): tiles `0x081F426C`, palette `0x0820BEEC` bank
0 and frames `0x0824E7F4`. Creation starts animation 0, snaps it to the
floor and seeds the attack wait (+ 0x14) to 16..47 frames and the flight
timer (+ 0x18) to 512..1535 frames. `sub_080C8B3C` wakes it when the player
enters its 240 x 128 box, plays animation 1 once, then flies in bounded
random legs: three patterns move diagonally toward the player at 0.5, one
moves horizontally away at 0.25 and two use `sub_0806BAC8` to fly at 1.0
toward a point 16 pixels above him. `sub_080C9788` reflects those paths at
the room edges, walls, floor and ceiling. When its attack wait expires with
the player ahead within 20 pixels (or on a one-in-1024 random draw), it
charges with animation 3 and then loops electrical animation 4 for 128..383
frames with attack box (-14, -32, 30, 28). Contact during either animation
creates the common spark animation 16 (`sub_080C9108` / `sub_080C9198`),
which follows the player's upper body while rising and alternating its
horizontal flip. When its flight timer expires the doll sinks into a solid
cell, rests in animation 5 for 64..127 frames and rises into flight again.

Its hit callback `sub_080C8FBC` hides it and `sub_080C9A24` creates the five
pieces described at `0x08528AE4` (sprite frames 20..24). `sub_080C9284`
applies each record's velocity and timer; piece 1 alone bounces against the
floor and stops when slow, while every piece blinks near the end and emits
the flame and dust of `sub_0806D5C0` at its record's intervals. Each piece
keeps a looping death-flame glow (`sub_080C9058`) 16 pixels above it, and the
hidden parent is removed only after all five pieces and their glows finish.
Under global `0x8E & 0x40`, `sub_080C9630` cycles the six animations in
place. Sounds 0x75, 0x6E and 0x153, palette flags, the tracking list and
`sub_080683BC` rewards are not ported; the native doubled draw during the
land animation is represented by one sprite.

Beam skeleton (enemy 0x59, `EnemyBeamSkeletonCreate` /
`EnemyBeamSkeletonUpdate`): tiles `0x081F427C`, palette `0x0820BF54`, frames
`0x0824F9AC`, walker probes `0x08528B04`; an affine slot, + 0x5C 0x11, the
floor snap and a patrol home (+ 0x22, bounds + 0x14 / + 0x16 at +/- 64).
State 0 (`sub_080CA770`) patrols at 0.375 (`sub_0806CAF8` mode 12),
turning 40 pixels from home, at walls and the room's edges, until the
player is in its 0xC0 x 0x80 box. State 1 (`sub_080CA884`) returns to the
patrol with the player more than 32 pixels above or below
(`sub_0806BDEC`), else waits r & 0x1F frames (+ 0x1C) and takes 20-frame
steps at (r & 0x3FFF) + 0x8000 braked by 1/16 (away from him when he is in
the ((r & 0x7F) + 0x40) x 0x80 box of `sub_0806C1B0`); after a step it
fires with the wait over and him in its 100 x 40 box, else its + 0x19
grows and r % 40 below it either (him beyond 48 pixels, `sub_0806BD64`)
clears it and pauses 60 frames (`sub_080CA9E0`) when r % 50 <= 14, or
halves it. State 3 (`sub_080CAA2C`): animation 2 to its end, animation 3
for 33 frames, then + 0x1A bit 0 and animation 4 at full slot scale (+
0x20 = 0xFFFF) until sprite frame 0x13 makes the flash (`sub_080CB0D0`,
frame 0x15, translucent) and the beam (`sub_080CAE74`, frame 0x16 then
0x17, + 0x3C = 3/2 of its own, box (-4, -6, 8, 12)) 11 pixels ahead and
17 above; animation 5 for 43 frames, then the scale shrinks by 0x400 a
frame and 9 frames later + 0x1A clears. The beam waits at the eye, shoots
at 12.0 and restarts there beyond 92 pixels; it collides while the scale
is above 0x400, is drawn scaled (`sub_0803E058`: 1 / scale, nothing at
0x400 or less) and stops hurting (+ 0x72 bit 1) once the scale leaves
0xFFFF. Its hit callback `sub_080CB4F0` (sound 0x6B) hides it and throws
seven bones (`sub_080CB1B4`, records at `0x08528B17`: x, y, frame, vx /
0x1000, -vy / 0x1000, a byte their update `sub_0806C5AC` ignores; with the
player to its right their x and vx point left).

Nemesis (enemy 0x26, `EnemyNemesisCreate` / `EnemyNemesisUpdate`) uses the
compact direct-frame descriptor `0x0824D2D8`, loaded from graphics
`0x081F4264` and palette `0x0820BEC8` by `sub_080C6714`. Its four native OAM
records are exported as one-frame animation states, preserving the recorded
hurt and attack boxes. The runtime ports the observable body loop from
`sub_080C67CC`: an idle hover, a homing approach, a short charge and a
recovery pause, with the native facing, sine bob and collision pass. The
global `0x8E & 0x40` path keeps the idle frame without combat. The linked
helper from `sub_080C76D4`/`sub_080C7784` is emitted as a parent-linked child:
its static frames 1-3 use the exported native anchors, follow the parent's
fixed-point position and mirror, and reproduce the +9/+10 pixel horizontal,
+37/-4 pixel vertical and 10/40-frame/flicker timings. The three technical
effect slots created by `sub_080C7B54` (sequence identities 9, 12 and 6) are
also emitted as translucent, non-combat children. `sub_080458E4` allocates a
zero-filled 24-record scratch table for each child; `sub_08045748` shifts that
table backwards, writes the owner's integer position, sprite-frame id and
mirror at record 0, then selects the child sequence index. The runtime keeps
that history per child, so the three slots render delayed trails (9, 12 and 6
frames), including the native first-frame zero-filled records. Their state-0 to
state-2 activation, timer, visibility, vertical flip and owner removal match
the callback. Palette callbacks and native sound calls remain open; death uses
the shared 40-frame flame and blink sequence.

Skull Millione (enemy 0x5B), Cagnazzo (0x49) and Lubicant (0x63) share
`sub_080B7E80` (create: tiles `0x081F4214`, palette `0x0820BCA0`, the
variant's 20-byte record at `0x0852883C`: frames, bank, flags, lead, wait,
top speed, dive speed; records 0 `0x0824A8E0` bank 2 flags 0 lead 4 wait
32 top 16.0 dive 5.0, 1 `0x0824A334` bank 0 flags 3 lead 2 wait 32 top
20.0 dive 4.0, 2 `0x0824A334` bank 1 flags 1 lead 8 wait 0 top 24.0 dive
4.0) and `sub_080B7F40` (update, no activity window; two animation steps
a frame outside the death). State 0 (`sub_080B7A34`) bobs with vy = sin(+
0x18 * 0x5B0) / 2 and, after the wait, chases once the player is in the
0xDC x 0xA0 box centred 40 pixels below. State 1 (`sub_080B7AEC`) heads
(`sub_080694B8`) at a speed growing by 0x2000 a frame for 80 pixels
above the player, the speed short of him, plus his vx times the lead.
State 2 (`sub_080B7BC8`) rises with animation 1 (vy -2.0, + 0x54 0x1000)
and plunges at the dive speed from its frame 4 (+ 0x54 -0x2000, sound
0x87) until the animation ends, then recovers; flag bit 1 first circles
the player for 64 frames with animation 3. State 3 (`sub_080B7D90`, set
by `sub_080B8004`) plays animation 2, three blood splashes over 32 frames,
then sinks through the generic death. Flag bit 0 adds a fin
(`sub_080B7988` / `sub_080B7780`): sprite frame 0xD, held 12 pixels ahead
and 6 above while its owner plays animation 3, turning by 0x1200 a frame,
then flung off when it dies.

Dead crusader (enemy 0x52, `EnemyDeadCrusaderCreate` /
`EnemyDeadCrusaderUpdate`): the skeleton knight's sheet, palette and frames
with palette bank 1 (`sub_0808F8EC`). Its walk `sub_0808F364` is the
knight's at 0.75 with a guard: on the floor, while `sub_080212C8` (the
player's + 0x10 flags when he is the current player) & 0x360 is set, it
plays animation 7 once, stops and sets + 0x21 (step 5); in step 5 it
faces the player, keeps + 0x21 and walks on after 60 frames on the floor.
The update clears + 0x21 each frame and does nothing while alive beyond
margin entry 4; its hit callback `sub_0808F848` deals 0 damage (sound
0x69) while + 0x21 is set. The attack `sub_0808F6E8`: animation 2 for 39
frames, animation 3, then one chance in two a thrust (sound 0x85 at the
12th frame, 31 frames, animation 4 for 21) or a slash (sound 0x85, 13
frames, then animation 6 four times 13 frames), then the walk again.

Mimic (enemy 0x60, `EnemyMimicCreate` / `EnemyMimicUpdate`): tiles
`0x081CC04C`, palette `0x0820AB7C` with bank = parameter 1, frames
`0x08223698` (`sub_08099430`). With parameter 0 set it is a closed chest
(sprite frame 0, its own hurtbox `0x0E10F2F8` = (-8, -14, 16, 14), no
attack box, step 3, floor snap); otherwise it waits hidden in step 0
behind a trigger entity (`sub_080992C8` / `sub_0809937C`: hurtbox
`0x100CF8FA` = (-6, -8, 12, 16), type 8). A struck trigger
(`sub_080993EC`) sets state 1 with a 40-frame timer, steps its waiting
parent (state 0, step 0) to step 1, and hidden sends flames of
`sub_0806D5C0` (8 x 8) every 4 frames above 30. Steps (`sub_080990D8`):
1 shows the chest with `+0x54` 0x2000; 2 falls with `sub_080698A8(e, 1.0,
-16, 4.0)` (each axis only with a speed or acceleration, the ceiling only
rising, the floor only falling) and lands into step 3 with its own boxes;
3 opens when the player is in (x - 16, y - 20, 32, 20) (animation 0 once,
step 4), then animation 1 (step 5) with the frame boxes. A hit
(`sub_0809922C`, sound 0x72) plays animation 3 in step 3 or 2 after and
runs the generic death for 40 frames; the contact (`sub_080994B0`) is a
knockback (type 1). Under gEwramData + 0x8E bit 0x40: state 2, animation
0 once, then animation 1.

Zombie soldier (enemy 0x0C, `EnemyZombieSoldierCreate` /
`EnemyZombieSoldierUpdate`): tiles `0x081CC024`, palette `0x0820AA48`, frames
`0x082215C4` (`sub_08093100`); animations 0 walk, 1 close attack, 2 throw,
3 death. The ROM enemy table at `0x080E9644` maps ids to these routines
(0x02 skeleton, 0x04 axe armor, 0x05 skull archer, 0x0C zombie soldier...).
Create: walk animation, one step, stats, a type-8 collision block with the
contact callback `sub_0809314C` (the same type-0 record as `sub_0806E1B8`)
and the hit callback `sub_08092B38`, then the floor snap. The update runs
inside the activity window of `sub_0806CC20` and ends with the animation step
and `sub_0806E314`. State 0 (`sub_080928FC`): at the start of walk frames 1
and 5 the soldier faces the player (`sub_0806CF2C`); with him in the
80 x 70 box `sub_0806CFFC(x - 40, y - 64)` and `(random & 0x7F) <= 0x3F` it
attacks (state 1, animation 1, sound 0x85 at frame 2); otherwise, with
`(random & 0x7F) <= 0x1F` and him in the 220 x 70 box at `x - 110`, it
throws (state 1, step 0xA, animation 2); otherwise vx = 0.25 toward its
facing, vy = 1.0, `+0x54` = 0x2800. Frames 0 and 4 stop it; it moves with
the walker collision `sub_08069A00` (the zombie's arguments). Both attacks
return to walking when their animation ends. The throw faces the player and,
at the start of frame 10, creates a grenade (`sub_08092BC0`) 16 pixels ahead
and 37 up with vy = -2.0, gravity 0x1800 and vx = (player x - its x) / 44,
at most 1.25 toward the facing; it shows sprite frame 18 of the soldier's
list (entity `+0x65`), carries the soldier's contact power and a 4 x 4 box
for both roles. The grenade (`sub_08092CCC`) moves with `sub_08069770`
(walls at its point by the sign of vx, the ceiling at y - 4, the floor, which
sets vy to the third of vy before gravity, which the grenade then negates):
the first floor hit bounces (sound 0x11B), a second one or a ceiling makes it
explode (state 2, sound 0x76); beyond margin entry 2 of `sub_0806D128` it is
deleted. Touching Soma (`sub_08093098`: element 2, knockback type 1) or
being struck by a weapon (`sub_080930E0`, no damage) makes a flying grenade
burst harmlessly (state 1, hidden, 7 frames). The explosion (state 2, hidden)
sets its attack box to 2 + t / 2 pixels around it while t <= 14, switches the
hurtbox off, collides while t <= 18 and ends after t = 24. Death
(`sub_08092B38`): sound 0x70, animation 3, 40 frames, state 2 and the
generic death `sub_0806AEAC`. On the first death frame (timer 0x28) the
update creates two gibs with `sub_08092FCC(e, dx, dy, vx, vy, frame)`
(`EntityCreateInRange(0x49, 0xCF, sub_08093078)`, sprite frame 0x13 then
0x14, mirrored like the soldier, ay 0x1800): dx = (r & 0xFFFFF) - 8.0, dy =
-1.0 - (r & 0xFFFFF) for the first and -(r % 24.0) for the second, vx =
0.5 + (r & 0x7FFF) (negated when mirrored), vy = -1.5 - r % 1.5, drawn in
that order; `sub_08093078` moves them with `sub_0806D3D8(e, 8.0)` (velocity,
clamp, then position) and deletes them once below the soldier's y at the
time (`+0x1C`). Under the global `0x8E & 0x40` the soldier is created in
state 3 without its collision block or floor snap, and only steps its
animation. The grenade's burst (state 1, every frame) and explosion
(state 2, while its timer, carried over from the bounces, is at most 8)
emit particles after incrementing the timer: `sub_0806D894` with a random
direction (angle `r & 0xFFFF`, speed (sin(a + 0x4000), sin(a)) through
`sub_0803E86C` at 1.0; animation 5 of `0x0820ED60`, OBJ row 6, `+0x18` = 7),
`sub_0806D930(x, y, 0, 0, p, 1, 7, 5)` at timer 3 (animation 5, row 5), and
on odd timers `sub_0806D644` with a second random direction (animation 0,
row 7, `+0x18` = 3). Their update `sub_08068F78` keeps 7/8 of vx / vy
(`+0x18` bits 0 / 1, `sub_0806D490` / `sub_0806D4A8`), moves with
`sub_0806D3D8(e, 8.0)`, blinks with `0x08118CE0[0x28 - t]` (bit 2, t
counting to 0x28, `sub_0806D518`), draws random flips by `+0x19` (unused
here) and is deleted on the update after its animation ends (`+0x59` bit
0). Row 5 renders black; the OBJ blending that may apply to it is not
modelled. Framework corrections found on the
way: starting an animation (`sub_0803F2C8`) clears `+0x59` bit 0, and
`sub_080683BC` (rewards, not ported) sets `+0x3E` bit 1 at death, which makes
`sub_0806E314` skip the collision pass, so a defeated zombie, soldier or bat
can no longer be hit nor hurt.

Axe armor (enemy 0x04, `EnemyAxeArmorCreate` / `EnemyAxeArmorUpdate`):
tiles `0x081E41AC`, palette `0x0820B7F4`, frames `0x0823C290`; animations 0
walk (18 frames), 1 high and 2 low throw, 3 the axe, 4 death. Create: walk
animation, the contact callback `sub_0806E1B8` and hit callback
`sub_080B1370`, the floor snap, then one pixel down. The update runs inside
the activity window, then the collision pass `sub_0806E314` before the
animation step (the reverse of the zombie soldier). It walks with the probe
walker `sub_0806CAF8(e, speed, 0x08528708, 0x14)`: the speed of walk frame
f is 0x1000 + (f % 9) * 0x2800 (0 when f % 9 > 6), toward its facing.
`sub_0806C828` moves X (vx += ax, clamped to 8.0) and probes walls at the
position before the move, `half_width` (6) to the side of the motion, at each
wall offset (-7, -24); then Y, with the ceiling probe (-40) when rising or the
floor probe (-1, 4 pixels lower while vy has no integer part), recording the
floor cell byte at `+0x3F` and slope flags. Mode 0x14 undoes a step that hits
a wall, lands on a slope or finds no floor; without a patrol length
(parameter 0) the armor then turns around, otherwise it turns after that many
steps (counted at walk frame 17). At the start of frame 17, the player ahead
(`sub_0806BBC4`) within 99 pixels (`sub_0806D044`, BIOS Sqrt) starts
`sub_080B1030`: a high (animation 1) or low (animation 2) throw at random;
at frame 12, tick 2, the axe leaves 24 pixels ahead and 16 up; at the end it
backs away playing the walk once if the player is still in reach, or walks
again. The axe (`sub_080B0D5C`, collision type 0xA, contact callback
`sub_0806E1E8`: element 1, type 0) flies at 2.5 toward the throw,
decelerated by 0x800 per frame so it returns, spins 0x800 per frame (a high
throw starts 20 pixels higher and spins the other way) with an affine
sprite, has one hit point and a 16 x 16 box, runs the collision pass before
moving, vanishes when struck (`sub_080B13BC`) and is deleted beyond screen
margin 4. Being type 0xA, it uses its own slot in Soma's recent hits (121
frames). Hit callback: the armor turns to the attacker on every hit; at 0 HP,
state 2 (`sub_080B11DC`, sound 0x6F) plays animation 4 and deletes it when it
ends. Its first death frame sets `+0x4C` = -40.0 and `+0x48` = 0, then
each frame (timer `+0x0D` incremented) the height `y + +0x4C` is taken
before `+0x4C` grows by 0x12000 and the phase `+0x48` before it grows by
0x5000; during the first 32 frames, on even frames a fire
(`sub_08045CEC(x, y, 1, 8)`: animation 1 of `0x0820ED60`, OBJ row 8,
priority 0x0D, vy 0.5, ay -0x1000) appears at x + (sin(phase + 0x4800) << 4)
+ `sub_0802D5EC(0x40000)` (`(random & (n - 1)) - n / 2`) and that height,
and every 8 frames (sound 0x6E) a spark (`sub_08045CEC(.., 16, 6)`) at x +
`sub_0802D5EC(16)` and the height + `sub_0802D5EC(4)` pixels. The drifting
effect `sub_08045C9C` adds vx to x, ax to vx, vy to y and ay to vy, steps its
animation and is deleted when it ends. A struck axe leaves a flame of
`sub_0806D5C0(x, y, 8.0, 8.0)`: `sub_0806E524` at a random point of the w x h
area around (x, y), mirrored by a third draw. Under the global `0x8E & 0x40`
the armor is created in state 3 (animation only; no hit callback or floor
snap). Not ported: the palette bits, the global pause `0x4BE` and the
sounds. The walker's one-way platform bit (mode bit 1) is not used by the
armor (mode 0x14) or the archer (0xC, 0xD). The axe's affine rotation
follows `sub_0803E058` (see the class 1 weapon).

Skull archer (enemy 0x05, `EnemySkullArcherCreate` /
`EnemySkullArcherUpdate`): tiles `0x081E419C`, palette `0x0820B78C`, frames
`0x0823A52C`; animations 0 stand, 1 walk, 2 to 4 volleys, 5 to 11 bone
pieces. Create: idle animation, `sub_0806E1B8` / `sub_080AFD3C`, the floor
snap plus one pixel; with parameter 0 it patrols (state 2, animation 1),
otherwise it stands (state 0). The update runs inside the activity window,
the state, then `sub_0806E314` before the animation step. State 0
(`sub_080AF8D0`) faces the player every frame and, once he is in the
240 x 35 box centred on it (`sub_0806E29C`), starts the next volley: `+0x14`
counts 0..4 and the volley is half of it (0, 0, 1, 1, 2), from the pointer
table `0x08528614` (lists of signed frame / y offset entries ended by frame
-1: volleys 0 and 1 shoot at frame 3, volley 2 at frames 3, 9 and 14 with y
offsets 0, 1, 2); it plays animation 2 + volley once. State 1
(`sub_080AF934`) shoots an arrow at the start of each listed frame (sound
0x85) and, 32 frames after the animation ends, stands or patrols again.
State 2 (`sub_080AFA9C`) patrols with the probe walker (table `0x08528620`:
one wall probe at -15, half width 8, ceiling -30; mode 0xC) at 0.25, turning
every 129 frames; with the player ahead within 79 pixels it backs away at
0.75 (mode 0xD, facing him at the start of its walk), and shoots once he is
farther than 99. The arrow (`sub_080AF7EC`, collision type 0xA, sprite frame
25, a 2 x 2 box 18 pixels ahead and 24 up) flies at 3.0; hitting Soma
(`sub_080AF78C`: a knockback, type 1) it stops colliding and stays on him at
its offset for 30 frames; struck (`sub_080AFD8C`) it vanishes; beyond margin
4 it is deleted. Hit callback: at 0 HP, `+0x20` = +1 or -1 away from the
attacker and state 3 (`sub_080AFB9C`, sound 0x6B), which on the next update
throws seven pieces (animations 5 to 11) and deletes the archer: vx =
(0x8000 + (random & 0xF) << 13) away from the attacker, vy = -3.5 +
(random & 0x1F) << 12, gravity 0x2000. A piece (`sub_0806C5AC`, generic) keeps
79/80 of vx each frame, adds its accelerations (clamped to 8.0) and moves;
it vanishes when the centre of its sprite's last OAM component
(`sub_0806C48C`), 4 pixels lower, is in a solid cell (`sub_080020A0`: bit 0;
sound 0x162) or when it leaves the screen widened by 32 pixels; it never
collides. When it hits the floor it leaves a dust puff
(`sub_08045CEC(x, y, 4, 6)`) at the probe point (4 pixels higher): vy =
`sub_0802D5EC(0x1000)` - 0.25 with ay = vy / 32, vx = `sub_0802D5EC(0x2000)`
minus the piece's vx with ax = -vx / 64 (the draws only when the puff is
created; its `+0x0F` = 1 is not modelled). A struck arrow leaves a flame of
`sub_0806D5C0` within 4 x 4 pixels of (x + 8 vx, y - 22). Under the global
`0x8E & 0x40` the archer is created in state 4 (animation only, no hit
callback or floor snap). The attacker is the weapon entity; Soma's position
stands for it. Not ported: the arrow's `+0x0F` change, the global pause and
the sounds.

Fleaman (enemy 0x18, `EnemyFleamanCreate` / `EnemyFleamanUpdate`):
`sub_080C1E88` loads tiles `0x081F423C`, palette `0x0820BD94` and frames
`0x0824B92C`; animations 0 hop, 2 death, 3 jump. Create: state 1, `+0x14`
= 1 when parameter 0 & parameter 1 is 0xFF (carried; no floor snap),
otherwise the floor snap; state 3 (animation 0 only) under the global
`0x8E & 0x40`. The update (no activity window): `sub_0806AE54` counts
`+0x2D` down, and while it is set (and not dying) only the collision pass
runs; state 1 `sub_080C1FBC`, state 0xFF the death `sub_080C2388`, then
the animation step and, unless dying, the collision pass. `sub_080C1FBC`:
step 0 plays animation 0 paused, vx 0.5 toward the facing, then step 4
(or 1 for a carried one, which waits: its check of the slot record at
`gEwramData + 0x4E4 + 0x84 * +0x12` never sees zero); step 2 falls with
animation 3, frame 3, until its cell is solid, snaps to the floor and rests
16 frames on frame 0 (step 3). Step 4 moves, faces the player
(`sub_0806BD64`) and jumps by `r % 30`: below 10 a hop (0.8125, -3.0,
`+0x54` 0xA000, animation 0, step 6); 10 a long jump (1.625, -3.0, 0x2800);
11 to 13 a high jump (0.8125, -5.0, 0x3C00); otherwise straight up (0,
-3.0, 0x5000, frame 1); these three play animation 3 paused (step 5); vx
is negated when facing left. Steps 5 and 6 move with gravity; step 5 shows
frame 2 near the apex (|vy| <= `+0x54`), else frame 1 from 0 and 3 from 2;
the ceiling probe `sub_08001C1C` at y - 10 and the wall probes 8 pixels
ahead and 8 up push it out; a solid cell at its feet lands it (step 4,
frame 0 for step 5). Hit callback `sub_080C2458`: at 0 HP `sub_080C2388`
(sound 0x72, animation 2, timer 50, state 0xFF, still with `+0x54`
0x2000; a carrier's `+0x19` is decremented), then `sub_0806BE74(e, 15,
8.0, 6.0)`. Contact `sub_080C242C`: type 0.

Dead warrior (enemy 0x2F, `EnemyDeadWarriorCreate` /
`EnemyDeadWarriorUpdate`): tiles `0x081EC1F4`, palette `0x0820BBD4`,
frames `0x08246F00` (animation 3, the death, has 70 frames); walker probes
`0x08528830`. Create: animation 0 looping and one step, the floor snap plus
one pixel, state 0 with `+0x1C` = 50 (state 2 under the global `0x8E &
0x40`). The update (inside the activity window) hides it above screen Y -16
(`sub_0806E4F8`), runs the state, then the collision pass before the
animation step. State 0 (`sub_080B72F8`) first walks with
`sub_0806CAF8(e, +0x18, 0x08528830, 4)`; step 0 clears `+0x18`; step 1
starts the charge (animation 1, step 2) with the player in the centred 100
x 60 box (`sub_0806E29C`) at walk frame 0, tick 1; steps 1 and 2 add 0x4000
to `+0x18` up to 2.0, play sound 0x7D every 25 frames on screen and count
`+0x1C`: past 100, sound 0x127, animation 2 (step 3), which removes 0x1000
per frame from `+0x18` and every 4 frames raises dust
(`sub_08045CEC(x, y, 4 + (r & 1), 6)`, vx 0.5 backward); when it ends the
warrior turns around and walks again. Hit callback `sub_080B7644`: state
1 (`sub_080B747C`): animation 3, vx 2.5 backward, vy -2.5, `+0x54` =
0x2000 through `sub_0806D3D8(e, 8.0)`, a palette flash for 40 frames, sound
0x6E every 8 frames, a fire `sub_08045CEC(x +/- 8, y - 32 +/- 8, 1, 7)` every
4, deleted off screen or once the animation ended within 128 frames.

Skeleton knight (enemy 0x0D, `EnemySkeletonKnightCreate` /
`EnemySkeletonKnightUpdate`): `sub_0808F120` loads tiles `0x081CC00C`,
palette `0x0820A95C` and frames `0x0821FA5C`; animations 0 idle, 1 walk, 2
to 4 the sword attack. Create: animation 0 looping and one step, the
collision setup, frame boxes, the floor snap, state 0 (state 3 under the
global `0x8E & 0x40`). Inside the activity window, state 0
(`sub_0808ECF8`): step 0 waits for the player in (x - 90, y - 48, 180, 96)
or a wound, then plays animation 1 with vy 1.0 (step 1). Steps 1 and 2
face the player whenever their timer passes 60 frames and move at 0.5
(forward in step 1, backward in step 2) with `sub_08069A00(e, 1.0, -32,
8.0, 8)`, `+0x54` = 0x2800. On the floor, step 1 with the player in (x -
40, y - 40, 80, 80) attacks (`r & 0x7F` <= 0x40) or backs away (step 2);
otherwise past the room's edge (`sub_0806B120(e, 1)`: clamped back to
column 1 or width - 1, 30 columns for one screen) it backs away, and
beyond margin entry 4 it plays animation 0 and waits again. Step 2
advances again (step 1) once the player leaves (x - 60, y - 40, 120, 80)
or its timer passes 60, at the room's edge or at a wall, and attacks him
within (x - 40, y - 40, 80, 80) when its timer passed 60 with the same
chance. State 1 (`sub_0808EFD8`) faces him and plays animation 2 for 45
frames, 3 for 31 (sound 0x85 at the 12th) and 4 for 21, then walks again.
Hit callback `sub_0808F0A4` (sound 0x72): 40 frames of the generic death
(state 2). Contact `sub_0808F16C`: type 0, element 0.

Valkyrie (enemy 0x29, `EnemyValkyrieCreate` / `EnemyValkyrieUpdate`):
`sub_0806EB34` loads tiles `0x081CBF2C`, palette `0x0820A3FC` (bank 0 for
id 0x29, else 1) and frames `0x0821285C` (the area 4 rooms 12-13 draw
routine as Curly); animations 0 hover, 1 strike, 2 death. Create:
animation 0 looping and one step, frame boxes, state 0 (state 4 under the
global `0x8E & 0x40`). The update has no activity window: `sub_0806AD24`
(stun), then state 0 (`sub_0806E7A8`): it faces the player; step 0 waits
until he is within 120 pixels (`sub_0806CEF8` / `sub_080689F8`: Sqrt of the
whole-pixel deltas) or it is hurt; step 1 heads at 2.0 (`sub_080696B0`:
v = delta * speed / distance, whole pixels) for the point 64 pixels in front
of him and 48 above, striking (state 1) once there or once within 60
pixels of him on both axes while more than 32 pixels above. State 1
(`sub_0806E8AC`): animation 1 once, speeds kept at 5/6 through
`sub_0806D3D8(e, 3.0)`; frame 2 is held (its tick reset) for 16 frames,
then the tick is set to 100; at frame 3 (sound 0x87) vx = 3.0 forward, vy =
2.5, after frame 7 `+0x54` = -0x4000; when the animation ends animation 0,
31/32 of vx for 20 frames (or until the player leaves 120 pixels), then 48
frames facing him at 7/8 speeds back to state 0 step 1. Hit callback
`sub_0806EBDC`: at 0 HP sound 0x1CD, animation 2, 40 frames of state 2
(9/10 of vx, a fall with 0x1000 clamped to 3.0, the generic death); alive,
struck by an attacker of type 3 or 4 during state 1, it plays animation 0
and recoils (state 3, `sub_0806EA8C`: 3.0 away and up, 11/12 per frame,
facing the player again above -1.25, state 0 above -0.125). Contact
`sub_0806ECA8`: type 0, its attack halved except during frames 4 to 6 of
the strike.

Erinys (enemy 0x62) and the Gremlin (enemy 0x37) only call the
Valkyrie's and the gargoyle's routines, both with palette bank 1; the
Valkyrie's recoil is reserved to id 0x29.

Curly (enemy 0x2B, `EnemyCurlyCreate` / `EnemyCurlyUpdate`):
`sub_08093540` loads tiles `0x081CC02C`, palette `0x0820AA6C` (bank 1 for
id 0x2B, else 0) and frames `0x0822184C`, with the draw routine
`sub_0806CCE8` in area 4 rooms 12 and 13 (`sub_0803B9D0` elsewhere);
animations 0 walk, 1 attack, 2 death. Create: animation 0 looping and one
step, the collision setup, frame boxes, the floor snap, state 0, vy = 1.0,
`+0x54` = 0x2800 (state 3, animation only, under the global `0x8E & 0x40`).
Inside the activity window, state 0 (`sub_08093218`): step 0 moves to step
1 once it is hurt (`sub_0806D54C`: HP below the record's). Outside walk
frames 4 to 7, each frame start (step 1 facing the player first) sets vx =
0.5 toward its facing through `sub_0806AC5C` (halved on a type 1 slope, 2/3
on type 2, only uphill), and on frame 1 the player in (x - 50, y - 64,
100, 70) with `r & 0x7F` <= 0x5F starts the attack (state 1, animation 1);
frames 4 to 7 stand still; sound 0x85 at frame 4. It moves with
`sub_08069A00(e, 1.0, -32, 8.0, 8)`; in step 0, on the floor, a wall, a
drop or wall one pixel ahead (`sub_0806B1A0`) or a column outside [1,
width - 1] (`sub_0806D188`, 30 columns for one screen) turns it around.
State 1 (`sub_08093460`) lunges at 1.0 for 8 frames, then stops, and walks
again when the animation ends. Hit callback `sub_080935E8` (sound 0x1C3):
animation 2 and 40 frames of the generic death (state 2). Contact
`sub_08093674`: type 0, element 0.

Durga (enemy 0x11): `EnemyDurgaCreate` and `EnemyDurgaUpdate` only call
Curly's routines; `sub_08093540` gives it palette bank 0.

Ghost Dancer (enemy 0x1D, `EnemyGhostDancerCreate` /
`EnemyGhostDancerUpdate`): `sub_080C2E74` loads tiles `0x081F4244`, palette
`0x0820BDD8` with bank = parameter 0 (0 to 2 in the rooms) and frames
`0x0824BC84`; animations 0 and 1 dance, 2 dies. Create: animation 0
looping and one step, state 1, `+0x5A` bits 1-2 cleared then set to 2,
`+0x14` = 0, `+0x16` = 0, `+0x17` = 1, `+0x18` = `+0x19` = 0; state 5
(animation only) under the global `0x8E & 0x40`. The update (no activity
window): hidden beyond margin entry 4; unless dying, the fade of `+0x18`:
0 opaque, 1 semi-transparent on even `+0x19` frames for 33 frames, 2
semi-transparent, 3 the reverse, then 0 (`+0x5A` bits 1-2 = 2 is the
semi-transparent OBJ mode); `sub_0806AD24` (stun); states 1-2
(`sub_080C3190`): step 0 plays animation 0, stops, faces the player
(`sub_0806BD64`), clears `+0x14`; step 1: vx = 2 sin(`+0x14` + 0x4000), x +=
vx, y += sin(4 `+0x14`) + vy, `+0x14` += 0x80 (`+0x16` counts the turns);
when vx is 0 it swaps animations 0 and 1; whenever `+0x14` % 0x8000 is 0
with `+0x18` even, `+0x17` flips and `+0x18` advances. Then the animation
step and, except in state 3, the collision pass (`sub_080421AC`). Hit
callback `sub_080C32EC`: sound 0x6D, state 3 through `sub_080C327C`
(animation 2 once, semi-transparent), deleted when it ends. Contact
`sub_080C339C`: type 0, element 0.

Gargoyle (enemy 0x57, `EnemyGargoyleCreate` / `EnemyGargoyleUpdate`):
`sub_08086900` loads tiles `0x081CBFDC`, palette `0x0820A804` (bank 0 for
id 0x57, else 1) and frames `0x0821C688`. Create: animation 0 looping and
one step, frame boxes, state 0 (state 2, animation only, under the global
`0x8E & 0x40`). Inside the activity window, state 0 (`sub_08086534`): step
0 faces the player, vx = 0.5 that way, vy = 0, `+0x50` = 0x1000, `+0x54` =
1.0; step 1 aims at the player's position (`sub_0806CD38` / `sub_0806CD78`)
64 pixels higher: on each axis beyond 8 pixels the speed moves by `+0x50`
toward it, clamped to `+0x54`; it faces its vx and moves; once its timer
`+0x0D` (signed) runs out, with the target within 8 pixels vertically and
96 horizontally it plays animation 3 (step 2). Step 2 faces the player,
keeps 9/10 of both speeds for 21 frames, then plays animation 1, sets
(`+0x50`, `+0x54`) = (0xC00 toward the facing, 0x1800), sound 0x87 (step
3): `sub_0806D3D8(e, 2.0)` for 25 frames; step 4 doubles `+0x50`, moves X
with `sub_0806D430(e, 2.0)`, keeps 15/16 of vy for 61 frames (in steps 3
and 4 animation 2 one frame in 256), then sets 0x1000 / 1.5 and animation
0 (step 5): it chases on X and climbs while the target point is above,
returning to step 1 with a 20-frame timer. Hit callback `sub_08086878`
(sound 0x6E): state 1, 40 frames, `+0x50` = 0, `+0x54` = 0x1000; state 1
keeps 9/10 of vx, falls with `sub_0806D3D8(e, 3.0)` and runs the generic
death. Contact `sub_08086964`: type 0, element 0.

Zombie officer (enemy 0x20, `EnemyZombieOfficerCreate` /
`EnemyZombieOfficerUpdate`): `sub_08092680` loads tiles `0x081CC01C`,
palette `0x0820AA24` and frames `0x08220F84`; animations 0 walk, 1 attack, 2
death. Create: walk animation and one step, stats, the soldier's collision
setup and frame boxes, the floor snap, state 0 (state 3, animation only,
under the global `0x8E & 0x40`). Inside the activity window: state 0
(`sub_080923E4`) at the start of walk frames 3 and 7 faces the player
(`sub_0806CF2C`) and attacks when he is in the box (x - 50, y - 64, 100, 70)
of `sub_0806CFFC`, else sets vx = 0.625 toward its facing, vy = 1.0,
`+0x54` = 0x2800; frames 2 and 6 stop it; then `sub_08069A00(e, 1.0, -32,
8.0, 10)`. State 1 (`sub_080924AC`) plays animation 1 once (sound 0x85 at
frame 4) and walks again. The hit callback `sub_0809252C` (sound 0x70)
plays animation 2 for 40 frames of state 2; at timer 0x28 three gibs of
`sub_080925B4` (as `sub_08092FCC`, with the officer's graphics): frames
0x10 and 0x11 drawn like the soldier's, then 0x0F at (-6, or +6 when
mirrored, -13) with the usual vx and vy = -1.0 - (r & 0xFFFF); then the
generic death. Contact `sub_080926CC`: type 0, element 0.

Medusa head (enemy 0x39, `EnemyMedusaHeadCreate` / `EnemyMedusaHeadUpdate`):
`sub_08093B68` loads tiles `0x081CC034`, palette `0x0820AAB0` (bank 1 with
parameter 1 bit 0, a gold head) and frames `0x08221978`; animations 0 and 1
fall, 2 rises, 3 dies. Create: animation 0 looping and one step; under the
global `0x8E & 0x40` state 4 (animation only); both boxes from the bytes of
`0x0A0AFBFB` (-5, -5, 10, 10); parameter 1 bit 1 makes a hidden spawner
without boxes (state 3), otherwise state 0 faces the player
(`sub_0806CF2C`), vx = 0.75 + (r & 0x3FFF) toward the facing, `+0x54` =
0xC00, `+0x20` = r & 1, `+0x22` = 45 + (r & 0xF), `+0x21` = `+0x22` / 2. The
update: on screen, `sub_08021924` (tracking list); outside margin entry 4
(not for the spawner) state 2 with timer 1; `sub_0806AD24` (stun: only the
collision pass); state 0 `sub_08093A3C`: vy -= / += `+0x54` by `+0x20`,
which flips each time `+0x21` reaches `+0x22`, x += vx, y += vy
(`sub_0806D3AC`), animation 2 while vy < 0, else 0 or 1 at random once it
stops rising; state 2 the generic death, decrementing the spawner's
`+0x18` (through `+0x1C`, id 0x39) when deleted; state 3 the spawner: one
frame in 32 (`r & 0x1F`), while `+0x18` < parameter 0, a head at screen X
256 when Soma's screen X (`sub_0806CDB8`) is at most 0x67, or at most 0x88
facing right (`sub_0806CEC0`), else -16, screen Y 64 + (r & 0x3F), with
parameter 1 & 1. Then the animation step and the collision pass. Hit
callback `sub_08093AE0`: at 0 HP sound 0x72, animation 3, 40 frames of
state 2. Contact `sub_08093BF0`: type 0, element 0x100 for gold heads
(petrification, not ported).

Needles (enemy 0x3F, `EnemyNeedlesCreate` / `EnemyNeedlesUpdate`): tiles
`0x081F4284`, palette `0x0820BF78` bank 0, frames `0x0824FB10`; one hit
point, contact attack 38. Create: animation 0 looping, not mirrored, state
1 (state 3, only stepping its animation, under the global `0x8E & 0x40`);
`sub_08042884` and `sub_080428F0` receive the constant `0x1010F8F8`, whose
bytes are the attack and hurt boxes (-8, -8, 16, 16); with parameter 0 set
`+0x14` starts at `RandomNumberGenerator % 0xFFFF`. The update (inside the
activity window) runs `sub_080CB5E0`, the animation step and the
collision pass. `sub_080CB5E0` marks the cycle done at once; every time a
cycle ends it pauses the animation (`+0x6C` bit 1) for
`RandomNumberGenerator & 0x7F` + 1 frames; with parameter 0 set it bobs:
dy = sin(`+0x14` + 0x4000) / 4 (rounded toward zero), `+0x14` += 0x400, x
+= vx, y += dy. The hit callback `sub_080CB930` sends it to state 2
(`sub_080CB894`): sound 0x70, hidden, `sub_080683BC`, the palette bits,
then `sub_080CB6BC` creates five pieces (`sub_080CB7C8`) from `0x08528B44`
(s8 dx, s8 dy, sprite frame, s8 vx, s8 upward vy in 1/16 pixels, frames;
dx and vx negated when mirrored), counting them in `+0x13`; it is deleted
when they are all gone. A piece is deleted outside `sub_08068AD4(e, -128,
-64, 0x1F0, 0x120)`, when its timer underflows or below screen Y 0xAA, and
otherwise moves and falls with vy += 0x3000.

Entity positions in the game are screen-relative: the collision probes add
the camera (`gEwramData + 0xA094 + 6 / + 0xA`) and screen tests compare them
directly. The port keeps room coordinates and subtracts the camera wherever
the game compares an absolute screen position.

Skeleton (enemy 0x02, research in progress, not ported). Tiles
`0x081F424C`, palette `0x0820BE3C`, frames `0x0824C910`. The body entity
reserves an affine matrix (`sub_0803E654`; slot in `+0x20` and the low five
bits of `+0x58`) and every update rotates it by a wobble angle (`+0x1E` moves
by -0x708 or +0x708 by its facing, `sub_0803E058`). Create builds two child
parts through `sub_080C46AC` (which also sets `+0x58` bit 0x20 on the body):
`sub_080C4148`, drawn with animation 0x10 (or 7 while body flag `+0x1A`
bit 0x20 is set), and `sub_080C43C4`, the held bone (animation 8); both
follow an attachment point of the body's current frame (`sub_0806C3F8`) plus
per-animation offsets from `0x0852892C`, and track the body through its slot
(`gEwramData + 0x4E4 + 0x84 * slot`). State 0 (`sub_080C35D0`) patrols at
0.375 between `x - 32` and `x + 32` of its record (with 40 pixels of
hysteresis) using the probe table `0x08528948` (mode 0xC), turning at walls
or ledges (`sub_0806B120`), until the player enters a 512 x 128 box. State 1
(`sub_080C36E8`) faces him and walks with a decaying random step, choosing,
by its body flags `+0x1A` (bits 1, 2, 4, 8) and timers, state 2 (a 60-frame
pause), 3 (picking up a bone: animation 5 or 6), or 4 (throwing:
`sub_080C3BD8` at frame 2, a bone of collision type 0xA flying at
0.54 with vy -3.0 and gravity 0x1800 from 30 pixels up, deleted below
screen y 170). Death (`sub_080C4624`, sound 0x6B) hides the body and throws
up to seven pieces described by `0x08528952` (6 bytes each: x and y
offsets, sprite frame, vx, vy, timer) through the generic debris
`sub_0806C5AC`, then deletes it 256 frames later.

Open question before porting: `sub_0803AC40` skips drawing an entity whose
`+0x58` bit 0x20 is set, `sub_080C46AC` sets it on the body, and part
`sub_080C4148` copies the body's bit every frame, so the code read so far
would leave both hidden; the routine that makes the skeleton visible (or a
different meaning of the bit for this entity) has not been found yet.

`fusion_aria_runtime` runs these rules with Soma's library frames
(README). Next steps: more enemies (damage through the weapon hitboxes), the
class 1, 4 and 5 weapon entities, the boss
doors (object 0x02 deletes itself once boss bit `0x37E >> param1` is set) and
the save-room objects (0x1C save point, 0x1D walls closing the side opposite
the room's single exit) behind the `0xF0` exit cells, the transition
rooms of `sUnk_0850E968`, the hurtboxes, then attacks.
