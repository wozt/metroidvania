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

Each frame is rendered once into OBJ palette indices (bank * 16 + color) and
stored as an 8-bit indexed BMP per graphics family (PowerSuit, FullSuit,
Suitless), cropped to its visible pixels with its native top-left offset; the
five visual suits share those frames. `palettes.tsv`
(`metroidvania-samus-palettes-v1`) holds every `sSamusPal_*` array row by row
(default, flashing, speed boost, beam release, charging, unmorph, dying,
saving, map download, frozen). The runtime picks two rows per frame like
`SamusUpdatePalette`: damage flashing during invincibility, the Screw Attack
flash, four frames of beam release after each shot, the unmorph rows for 15
frames after leaving the Morph Ball (Suitless borrows the Power Suit rows),
and the dying sequence, whose row 0 is the Power Suit dying row and whose
`+16 * k` offsets read the directly following `sSamusPal_Generic_Dying`. The
jump echo uses the native palette bank 1 for every part. The
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

While missiles are highlighted the source swaps the arm cannon graphics for
the `*_Armed_*` tables; those variants are stored under the same key with an
`/armed` suffix (480 sequences). `cannon_offsets.tsv`
(`metroidvania-samus-cannon-offsets-v1`) lists, for every key and frame, the
arm cannon offset from Samus's pixel position computed exactly like
`SamusUpdateArmCannonPositionOffset`; projectiles spawn there.

The same command also writes the projectile sprite library below
`assets/extracted/metroid/sprites/projectiles/runtime/` with
`scripts/mzm_projectile_compose.py`: every non-particle `FrameData` table of
`projectile_data.c` (beams of all five types and their charged forms, the
pistol, missiles, super missiles, bombs and power bombs) in four flip states,
staged like `HudGenericLoadCommonSpriteGfx` and `ProjectileLoadGraphics`
(common graphics at OBJ 0x800, the beam set at 0x1000, common palette rows
2-7 with the beam row patched into row 2). Offsets are relative to the
projectile position; X/Y flips mirror around it as `ProjectileDraw` does.

Not yet used by the runtime: the saving, map download and frozen palette
rows (their mechanics are missing), and effect sprites such as the Screw Attack and Speed Booster
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

Weapons follow `src/runtime/mzm_projectiles.c`, a port of
`SamusSetHighlightedWeapon`, `SamusCheckFireBeamMissile` and the beam/missile
parts of `ProjectileUpdate`, `ProjectileInit`, `ProjectileMove`,
`ProjectileProcessNormalBeam`, `ProjectileProcessMissile`,
`ProjectileProcessSuperMissile` and `ProjectileUpdateAnimation`. Holding R
arms missiles (or super missiles after Select) while ammunition remains; B
fires when the cooldown allows (7, 9 and 11 frames), the controller reacts with
the native shooting or spin-break pose, and the projectile spawns at the arm
cannon offset of the pose and frame Samus has after her update. Power Beam
shots move 4 then 5 pixels per frame, die after 13 updates (short beam) or on
entering solid collision, and at most six exist. Missiles advance 12 pixels,
then accelerate by one subpixel per frame from 2 pixels (super missiles from
3), add Samus's horizontal velocity when moving the same way, and spend their
ammunition at launch; spending the last one disarms them. While missiles are
armed the runtime draws the `/armed` arm cannon sequences.

Collision uses every native Clipdata type of the room (exported by
the native room export from `sClipdataCollisionTypes` and its tilemap
twin) with the `ClipdataConvertToCollision` rules per actor: solid blocks,
doors, all six floor slopes at subpixel precision, enemy-only, stop-enemy and
tank blocks. Known gaps, kept explicit: Samus's box is swept against those
types instead of the original point probes, so slope speed changes
(`SamusChangeVelocityOnSlope`) and the partial-ceiling position nudges are
absent, and pass-through platforms (unused in the original rooms) are air. The pistol, aiming
while hanging, crawling and Morph Ball tunnel pulls are not implemented. Lethal damage enters the dying pose without the original
screen-centre drift and fade. F6 remains a separate raw-catalogue browser for
all 1,384 sequences.

The jump trail is the native `SamusEcho` behavior documented by the pinned
decompilation in `src/samus.c`, not a generic motion blur. The runtime keeps a
64-position ring at 60 Hz and activates on the source condition: MidAir, spin,
Space Jump, Screw Attack or midair Morph Ball with a Y velocity above 80. It
renders one past body position while cycling four distance-two offsets. The copy is drawn with OBJ palette bank 1 for every part, as `SamusDraw` does.

H applies a diagnostic 20-point hit through the native suit reduction rule.
The 48-frame invincibility interval prevents repeated damage, energy equal to
the damage is lethal as in the source, and Enter restarts after death. Damage
amounts are project diagnostics, not extracted enemy values.

The semantic registry can be validated without opening a window:

```sh
./build/fusion_room_runtime --check-animations \
  --samus-assets assets/extracted/metroid/sprites/samus/runtime
```

The early Power Suit preview tool (`scripts/mzm_samus_sprite.py`) and its
`animations/basic` BMPs are removed: the runtime library above covers every
pose, and nothing read those previews.

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

The runtime library exports every palette row and its native effects; see
`palettes.tsv` and the `SamusUpdatePalette` selection in `src/runtime/mzm_samus.c`.

## Active diagnostic workflow

Stage a source-verified record into a local OBJ VRAM dump:

```sh
python3 scripts/mzm_samus_frame.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --frame-pointer 0xVERIFIED \
  --output assets/extracted/mzm/samus_frame_vram.bin
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
