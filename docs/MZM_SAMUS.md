# Zero Mission Samus graphics

This document is the single reference for discovering, decoding, composing,
and displaying Samus graphics from a user-owned Metroid: Zero Mission USA ROM.
All tools verify SHA-1 before reading known addresses. Generated data stays
under ignored `assets/extracted/`; never commit ROM bytes or extracted images.

## Current result

The canonical full-library command is:

```sh
python3 -m scripts.mzm_samus_pipeline
```

It composes 1,384 sequences (6,008 frames, 3,192 unique BMP objects) in about
three seconds and stores them in a SHA-256 object store below
`assets/extracted/metroid/sprites/samus/runtime/`. Every native variant
resolves; none is synthesized.

`scripts/mzm_samus_compose.py` reproduces the native selection instead of
heuristic candidate lists:

| Step | Native source |
|---|---|
| Body animation per pose and selector (ACD, Space Jump flag, shinespark direction, crawling aim) | `sSamusAnimPointers_<PowerSuit/FullSuit/Suitless>[_<Table>]` |
| Arm cannon animation with the same selector, or `_All[pose]` when the source has no dedicated table | `sArmCannonAnimPointers_<Suit/Suitless>_*` |
| Arm cannon graphics: running right uses `_Standing`, hanging uses `_Hanging`, zipline right uses `_OnZipline`, everything else `_Default`; never for Dying | `SamusUpdateGraphicsOam`, `sArmCannonGfxPointers_*` |
| Draw order: cannon in front (header bit 12), body, cannon behind (bit 13) | `SamusDraw` |
| Draw position: OAM relative to Samus's position plus 2 pixels down | `SamusDraw` |
| Palette: first two banks of the suit default palette | `SamusUpdatePalette` |

Graphics and palettes combine into five visual suits:

| Visual suit | Body graphics | Palette |
|---|---|---|
| PowerSuit | Power Suit | `sSamusPal_PowerSuit_Default` |
| VariaSuit | Power Suit | `sSamusPal_VariaSuit_Default` |
| FullSuit | Full Suit | `sSamusPal_FullSuit_Default` |
| GravitySuit | Full Suit | `sSamusPal_GravitySuit_Default` |
| Suitless | Suitless | `sSamusPal_Suitless_Default` |

Each frame is rendered once into OBJ palette indices, colorized per suit,
cropped to its visible pixels and stored with its native top-left offset. The
shared runtime index (`metroidvania-sprite-index-v1`, see
`scripts/sprite_library.py`) lists
`key, frame, duration, offset_x, offset_y, object path`; keys are
`<VisualSuit>/<table|pose>/<selector>/<side>`, for example
`GravitySuit/Standing/ACD_DIAGONALLY_UP/left` or
`PowerSuit/pose/SPOSE_SPINNING/right`. `sequences.json` records the exact
body, cannon, cannon-graphics and palette symbols behind every key.

Validation against the previous diagnostic library: all 387 previously
exported sequences that had no arm cannon or a verified composition are
reproduced pixel for pixel; the remaining old Full Suit, Suitless and hanging
entries were body-only and now gain their native arm cannon.

Not yet exported: missile-armed cannon graphics (`*_Armed_*`), beam-charge,
speed-boost, shinespark, flashing, unmorph and dying palette rows, the echo's
palette bank, and effect sprites such as the Screw Attack and Speed Booster
overlays. The semantic registry binds 600 rows for 33 actions; the only
missing bindings are Suitless skid, Screw Attack and shinespark actions, which
have no native Suitless sequence.

## Runtime state mapping

`animation_map.tsv` is loaded and cross-validated against `runtime_index.tsv`
before SDL creates any animation textures. Gameplay no longer chooses
animations from held keys: `src/runtime/mzm_samus.c` ports the native pose
handlers (`SamusStanding`, `SamusRunning`, `SamusMidAir`, `SamusSpinning`,
`SamusCrouching`, `SamusMorphball`, `SamusRolling`, `SamusHangingOnLedge`,
`SamusPullingSelfUp/Forward`, `SamusGettingHurt`, ...) and the pose-change
carries of `SamusSetPose`, `SamusSetMidAir`, `SamusSetLandingPose` and
`SamusChangeToHurtPose`. The runtime maps each pose to exactly one semantic
action, and the controller owns the native animation frame and duration
counter, so transitions such as spin start, turning, landing, morphing,
unmorphing, wall-jump start and both ledge pulls end exactly when their native
animation does. Spin, Space Jump and Screw Attack are poses selected by
`SamusSetSpinningPose` from the equipment flags, and their registry fallback
is the spin sequence, never MidAir.

Native values now used directly:

| Quantity | Native value | Source |
|---|---|---|
| Units | 4 subpixels per pixel; velocity in 1/8 subpixel per frame | `types.h`, `VELOCITY_TO_SUB_PIXEL` |
| Ground acceleration / cap | 8 / 96 | `SAMUS_X_ACCELERATION`, `SAMUS_X_VELOCITY_CAP` |
| Midair acceleration / cap / morphed cap | 8 / 64 / 48 | `SAMUS_X_MID_AIR_*` |
| Gravity, rise cap, fall cap | 10, 192, 128 (no gravity below -231) | `SamusUpdateVelocityPosition` |
| Jump velocity: low / High Jump / Suitless / Morph Ball | 192 / 232 / 212 / 212 | `SAMUS_*_JUMP_VELOCITY` |
| Standing / crouched / morphed block hitbox | 14x31 / 14x23 / 14x15 px | `sSamusBlockHitboxData` |
| Spin poses hitbox | crouched | `sSamusCollisionData` |
| Wall-jump window | 8 frames after side contact while spinning; probe 10 px | `SamusCheckCollisions`, `SamusSpinning` |
| Ledge hang | Power Grip probes; feet aligned 34 px below the ledge | `SamusCheckCollisions`, `SamusCheckCarryFromCopy` |
| Pull up / forward | 6, 2, 1 px per frame by animation frame / 1 px per frame | `sSamusPullingSelfUpVelocity`, `SamusPullingSelfForward` |
| Hurt | rise 112 grounded or 56 midair, 48 invincible frames | `SamusChangeToHurtPose` |
| Suit damage reduction | Varia 0.8, Gravity 0.7, both 0.5, minimum 1 | `SpriteUtilTakeDamageFromSprite` |

Releasing the D-pad while running returns directly to standing with zero
velocity, as in the source; skidding is a Speed Booster pose and is not
entered. A standing jump is a straight MidAir jump, a jump while holding a
direction starts a spin, and A pressed in midair starts spinning without extra
height. Wall jumping requires facing away from the touched wall before A, and
is replaced by Space Jump when that item is equipped.

Known gaps, kept explicit: block collision is resolved with the runtime's
verified Clipdata boxes and a subpixel sweep, not the original point probes, so
slope speed changes (`SamusChangeVelocityOnSlope`) and the partial-ceiling
position nudges are absent. Speed Booster, Shinespark, bombs, beams, aiming
while hanging, crawling and Morph Ball tunnel pulls are not implemented. The
fire button only triggers the native shooting-pose reaction until projectile
entities exist. Lethal damage enters the dying pose without the original
screen-centre drift and fade. F6 remains a separate raw-catalogue browser for
all 1,384 sequences.

The jump trail is the native `SamusEcho` behavior documented by the pinned
decompilation in `src/samus.c`, not a generic motion blur. The runtime keeps a
64-position ring at 60 Hz and activates on the source condition: MidAir, spin,
Space Jump, Screw Attack or midair Morph Ball with a Y velocity above 80. It
renders one past body position while cycling four distance-two offsets. Native
code forces OBJ palette bank 1 for the copy; the SDL path still uses a
translucent violet modulation until that palette bank is exported.

H applies a diagnostic 20-point hit through the native suit reduction rule.
The 48-frame invincibility interval prevents repeated damage, energy equal to
the damage is lethal as in the source, and Enter restarts after death. Damage
amounts are project diagnostics, not extracted enemy values.

The semantic registry can be validated without opening a window:

```sh
./build/fusion_room_runtime --check-animations \
  --samus-assets assets/extracted/metroid/sprites/samus/runtime
```

`scripts/mzm_samus_sprite.py` reconstructs the four Power Suit animations used
by the local asset preview. The addresses below come from exact symbols in the pinned
`mzm` source and a matching `mzm_us` build, not from scan heuristics.

| Preview state | MZM source animation | Frame address | Cannon animation | Frames | Durations at 60 Hz |
|---|---|---:|---:|---:|---|
| Idle | `sSamusAnim_PowerSuit_Right_Standing` | `0x08248744` | `0x08234430` | 4 | `16,16,16,16` |
| Run | `sSamusAnim_PowerSuit_Right_Running` | `0x08248034` | `0x08234120` | 10 | ten times `2` |
| Jump | `sSamusAnim_PowerSuit_Right_Spinning` | `0x0824FE58` | `0x08234F38` | 8 | `2,1,2,1,2,1,2,1` |
| Attack | `sSamusAnim_PowerSuit_Right_Shooting` | `0x08248884` | `0x082344B0` | 3 | `2,2,4` |

The normal Power Suit palette is `sSamusPal_PowerSuit_Default` at ROM offset
`0x2376A8`; normal rendering copies its first two 16-color rows. Idle uses the
forward-standing cannon graphics at `0x082337EC` and `0x082338AC`. Attack uses
the forward-right default graphics at `0x0823236C` and `0x082324AC`.

Generate every local animation from the repository root:

```sh
for animation in idle run jump attack; do
  python3 scripts/mzm_samus_sprite.py \
    --rom "roms/Metroid - Zero Mission (USA).gba" \
    --animation "$animation" \
    --output-dir assets/extracted/metroid/sprites/samus/animations/basic
done
```

Use `--frame N --output assets/extracted/metroid/sprites/samus/animations/diagnostic/frame.bmp`
to export one record for diagnostics. The GTK4 asset viewer displays the generated frames;
their source-defined durations are retained as metadata for the future runtime.

Batch output uses a shared transparent canvas centered on Samus's OAM X axis,
with common vertical bounds per animation. This preserves the game-space anchor
and prevents jitter between differently sized records. The idle sequence is
`0,1,2,1`; the duplicate image is intentional. Un-aimed run and spin records
use empty cannon OAM because their visible arm pixels are already in the body.

The jump state represents the repeating `SPOSE_SPINNING` section. The separate
one-record `SPOSE_STARTING_SPIN_JUMP` transition is not replayed on every loop.
This is verified visual extraction, not execution of the original MZM state
machine.

## Verified data formats

### Animation and graphics

Each 16-byte-aligned `SamusAnimationData` record contains top graphics, bottom
graphics and raw OAM pointers followed by an 8-bit duration. Each graphics
bundle begins with two tile counts, then two groups of uncompressed 4bpp tiles.
The compositor reproduces the documented 32 KiB OBJ VRAM layout:

| Part | VRAM offset |
|---|---:|
| Shoulders | `0x000` |
| Legs | `0x280` |
| Torso | `0x400` |
| Lower body | `0x680` |
| Arm cannon upper | `0x800` |
| Arm cannon lower | `0xC00` |

Samus uses 2D OBJ mapping: successive tile rows advance by 32 tile indices.
The earlier generic 1D hypothesis cannot connect the separated VRAM rows.

### Raw OAM and layering

Pinned `third_party/mzm/tools/oam.py` confirms a 16-bit header followed by
three 16-bit attributes per object, or six bytes per entry. The low 12 header
bits are the count; bits 12 and 13 place the cannon in front of or behind the
body. These are compact source records, not eight-byte hardware OAM records.

The final compositor handles signed coordinates, square/horizontal/vertical
sizes, flips, palette banks, transparent index zero, 2D tile addressing, body
and cannon VRAM staging, and front/behind order. Affine and 8bpp objects remain
unsupported because the verified animations do not require them.

### Palette

`scripts/mzm_samus_body.py` can map 1 to 16 consecutive BGR555 rows into an
explicit OBJ bank range. It rejects any OAM bank not loaded by the caller. A
body-only diagnostic using the verified standing frame is:

```sh
python3 scripts/mzm_samus_body.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --frame-pointer 0x08248744 --palette-offset 0x2376A8 \
  --palette-rows 2 --first-bank 0 \
  --output assets/extracted/previews/samus_body.bmp
```

## Active diagnostic workflow

Stage a source-verified record into a local OBJ VRAM dump:

```sh
python3 scripts/mzm_samus_frame.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --frame-pointer 0xVERIFIED \
  --output assets/extracted/mzm/samus_frame_vram.bin
```

Decode its confirmed compact OAM format:

```sh
python3 scripts/mzm_samus_oam_decode.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --frame-pointer 0xVERIFIED
```

Every active diagnostic validates pointers, bounds and slot capacities. The
authoritative format reference is
`third_party/mzm/docs/samus/graphics.md` plus the pinned implementation.

## Remaining work

- Add the one-frame spin-jump transition when the native engine gains explicit
  animation state transitions.
- Extend recipes to aim directions, crouch, morph ball, damage, suits and
  effect overlays as gameplay needs them.
- Keep MZM formats and assumptions out of the separate Aria engine.
- Keep extracted outputs local and ignored.
