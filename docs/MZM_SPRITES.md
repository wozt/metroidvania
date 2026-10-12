# Zero Mission sprites

The Zero Mission runtime's sprite engine follows the pinned decompilation
(`third_party/mzm`). Everything game-specific is parsed from that source or
read from the user's ROM at export time; nothing is stored in the repository.

## Data (`scripts/mzm_sprites.py`, rebuild task `metroid_sprites`)

- Room placements (`SpriteLoadRoomSprites` / `SpriteInitPrimary`): a room
  entry's `.pDefaultSpriteData` lists `(y block, x block, slot)` triples up to
  `0xFF`. A slot above 16 (`MAX_AMOUNT_OF_SPRITE_TYPES + 1`) selects entry
  `(slot - 1) % 16` of the room's default spriteset (`sSpritesetPointers`:
  sprite id and graphics slot, `SpriteLoadSpriteset`); a lower slot is sprite
  `slot - 1` in graphics slot 0. `mzm_runtime_room` writes them as `P` lines
  (block y, block x, sprite, graphics slot, room slot). The hard-mode and
  event spritesets are not exported.
- Graphics (`SpriteLoadSpriteset`, `SpriteDraw`): the LZ77 graphics of
  `sSpritesGraphicsPointers` at OBJ VRAM 0x4000 + slot * 0x800 (tile 0x200 +
  slot * 64), the palette rows of `sSpritesPalettePointers` at OBJ rows
  8 + slot, every `FrameData` table of the sprite's data file drawn relative to
  the sprite position and mirrored around it for the X / Y flip statuses.
  Only sprites with a ported AI are exported, with the palette row their AI
  selects (the red Zoomer uses row 1). Keys: `Sprite/<NAME>/<Table>/<flip>`.
- `metadata/sprites.tsv`: `sSpriteRandomNumberTable`, `sSpritesFallingSpeed`
  and the `sPrimarySpriteStats` row of every primary sprite, read from the ROM
  through the reference ELF symbols (verified against the ROM), the
  `sSecondarySpriteStats` rows and the signed tables the ported AIs read
  (`table` lines: falling, jump, idle and movement tables, including the four
  Space Pirate jump tables).
- Enemy drops draw from the common sprite graphics
  (`HudGenericLoadCommonSpriteGfx`: `sCommonSpritesGfx` at OBJ VRAM 0x800,
  `sCommonSpritesPal` at OBJ palette rows 2-7) with their
  `sEnemyDropOam_*` table, unflipped.

## Engine (`src/runtime/mzm_sprites.c`)

- `SpriteUpdate` order per sprite: `gSpriteRng =
  sSpriteRandomNumberTable[(frame8 + slot + frame16 / 16 + x + y) % 32]`, the
  stun timer, the AI, `SpriteUpdateAnimation` (a frame lasts its timer + 1
  ticks of the counter; a zero timer loops), `SpriteCheckOnScreen` (half a
  screen of 8 blocks plus the draw distances, GBA screen size).
- Collision as sprites see it: `ClipdataConvertToCollision` (original
  version) for the sprite actor, `SpriteUtilGetCollisionAtPosition`,
  `SpriteUtilCheckVerticalCollisionAtPosition` and its slope variant (block
  tops on slopes and `gPreviousVerticalCollisionCheck`) and
  `SpriteUtilAlignYPositionOnSlopeAtOrigin`.
- Zoomer (`zoomer.c`): initialisation onto a floor, ceiling or wall (or
  removal), crawling at 0.5 pixels per frame (red: 0.75) with the eight edge
  and corner turns, slope animations (two thirds speed uphill with the
  animation slowed every other frame), falling with `sSpritesFallingSpeed`
  and landing.
- Zeela (`zeela.c`, yellow and red): the Zoomer's crawler with one subpixel
  per frame, its own hitbox (legs 4, eyes 56, sides 24 subpixels), the X
  flip following the facing on the ground, a single slope table flipped
  when it starts, and doubled (falling) or tripled (landing) animation
  speed. A red Zeela uses palette row 1 and, when it dies, throws two
  `ZeelaEyes` (secondary sprites drawn with its graphics): they fall with
  `sZeelaEyesFallingFrom*Speed`, drift 1 to 3 subpixels a frame and spin
  20 rotation units a frame (`ROTATION_SCALING_SINGLE`, drawn by turning
  the composed frame around its centre), stop on solid collision or when
  they touch Samus (`SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT`, 3 damage), then
  explode harmlessly until their table ends.
- Waver (`waver.c`): starts toward Samus and bounces diagonally, a pixel a
  frame across and half a pixel vertically, reversing on any non-air
  collision at its hitbox edge.
- Sidehopper (`sidehopper.c`) and Dessgeega (`dessgeega.c`): one ported
  state machine, as the two routines are parallel. On the ground or, when a
  block is above them at spawn, on the ceiling (Y flip): idle or head shake /
  scream (RNG below 8) for `work1 + 1` loops (`work1` stays 0 at spawn
  without BUGFIX), a jump warning when Samus's middle is within five blocks
  (`SpriteUtilCheckSamusNearSpriteLeftRight`, facing her), then a high or low
  jump (odd RNG) from the ROM tables, one entry per four frames: 1.25 pixels
  a frame across while rising, one while descending, pushed back 1.5 pixels
  and turned by walls, falling when its head meets solid, landing on its
  centre or either side. Unsupported on both sides, a ground one falls with
  `sSpritesFallingSpeed` (ceiling: `sSpritesFallingCeilingSpeed`). The
  Sidehopper's head hitbox grows to 0x70 while jumping and returns to 0x5C on
  landing; the Dessgeega keeps 88. Deaths use the single-then-big explosion.
  Every ground Dessgeega death sets `EVENT_LONG_BEAM_DESSGEEGA_KILLED` and
  unlocks the doors, a bug of the game that is kept.
- The Long Beam Dessgeega (`PSPRITE_DESSGEEGA_AFTER_LONG_BEAM`) exists only
  after `EVENT_VIEWED_STATUE_ROOM` and before its kill event; it locks the
  doors, waits hidden from Samus and projectiles until she is on screen
  within 12 blocks vertically and 3.75 across, drops with four debris at 24
  subpixels a frame, removes the three floor blocks it meets
  (`CAA_REMOVE_SOLID`) with dust and debris, and lands on the next floor.
  Events, door locks and block removal reach the game through
  `MzmSpriteHooks`; the runtime keeps session event flags (nothing sets the
  statue room event yet), reports door lock requests and turns removed blocks
  into air without changing the static background.
- The runtime loads each family's OAM frame durations separately: a family
  missing from the library is reported by name and its sprites are not
  spawned, while the others keep working.
- Viola (`viola.c`, blue and orange): half a block up at spawn, then onto
  the floor, ceiling, left or right wall found around it (or removed); it
  crawls half a pixel a frame (orange: one) with its centre 24 subpixels from
  the surface, following floor slopes, turning around outer and inner
  corners as the native probes decide, snapping to the block on ceilings and
  walls, falling with `sSpritesFallingSpeed` when unsupported and landing on
  a floor in a random direction.
- Sova (`sova.c`, purple and orange): a separate crawler port, as its
  ceiling state is `work2` with its own table (no Y flip while crawling), its
  speed follows the animation frame (0, 0, 1, 2, 3, 1 subpixels; orange one
  more while moving), its forward probes reach 28 subpixels, its turn tables
  and offsets differ from the Zoomer's, it has two slope tables (full speed
  going down, two thirds up) and it lands in a random direction. The orange
  Sova uses palette row 1.
- Zeb and Geega (`zeb.c`, `geega.c`, one ported state machine): hidden in
  a pipe (not drawn, ignoring projectiles, harmless) until Samus is above it
  (her feet at least 30 subpixels higher), more than 36 subpixels beside it
  and within five blocks of her middle (`SpriteUtilCheckSamusNearSpriteAboveBelow`);
  it rises an eighth of a block a frame, harmful after two frames, until
  100 subpixels above her saved height, waits 10 frames and flies toward her
  at 12 subpixels a frame through walls; 16 blocks past her it returns to its
  pipe and waits a second. A death leaves a drop that follows the pipe's slot
  (`DEATH_RESPAWNING`, `SpriteSpawnDropFollowers`) and the pipe waits while
  it exists (`SpriteUtilCheckHasDrops`). A Geega leader spawns a follower two
  and a half blocks below it when it rises and waits while it exists; a
  follower is removed instead of respawning. Zebbos (`zebbo.c`) are Geegas
  with a 24-subpixel hitbox whose green leader spawns two followers (1.5 and
  2.5 blocks below, harmful after 10 and 18 frames).
- Reo (`reo.c`, green and purple wings): hovers with `sReoIdleYMovement` /
  `sReoIdleXMovement` from an offset of four times the RNG, until Samus's
  middle is within 8 blocks vertically and 7 across; then, facing her and
  diving if above her head, it homes on a point 72 subpixels above her feet,
  each axis accelerating one unit (a quarter subpixel) a frame up to its cap
  (green 24 across and 16 vertically, purple 64 and 32), braking once past
  the point and turning, and turning at walls. It pushes the next overlapping
  Reo of its kind a pixel away (when that one is not frozen), and dies with
  the big explosion.
- Mella and Holtz (`mella.c`, `holtz.c`): ceiling swoopers sharing their
  native movement probes (down until a floor is a block below their centre
  or either side, up until both blocks on one side of their top are solid,
  sideways until solid a block ahead). Both hover with ROM idle tables and
  swoop when Samus is low (72 subpixels below them) and within 8 blocks
  vertically (Mella: 3 across, Holtz: 5); `SAMUS_COLLIDING` is their going
  down flag, as in the game. The Mella waits 7 frames, then moves with
  `sMellaGoingDownYMovement`, `sMellaGoingUpYMovement` and
  `sMellaMovingXMovement` (one entry per 8 frames) and its animation is
  `sMellowOam_Moving`; the Holtz warns for its table, dives 3 pixels a frame
  while its horizontal speed grows from 2 to 12 subpixels every other frame,
  slides for its table, climbs back slowing down and bonks on its ceiling.
- Multiviola (`multiviola.c`): bounces diagonally off every surface at 3
  subpixels a frame on both axes, starting toward Samus, its 56-frame table
  starting at twice its slot (the frame limit per table is now 64).
- Rinka (`rinka.c`, orange and green; not the Mother Brain ones): born at
  its spawn block (the first spawn is visible at once, as `SpriteInitPrimary`
  sets no `NOT_DRAWN`); a respawn stays hidden and harmless for 60 frames
  (green 20), shows once on screen and turns vulnerable 11 frames later. When
  its spawning table ends it aims at 60 subpixels above Samus's feet and is
  placed along the unit vector (10-bit fixed point, BIOS `Sqrt`) at 4
  subpixels (green 5) times its frame count from the spawn point; 16 blocks
  away from Samus or past the room origin it respawns. Deaths respawn it and
  leave a drop only when none of its own exists
  (`DEATH_NO_DEATH_OR_RESPAWNING_ALREADY_HAS_DROP`). The green Rinka's
  `MOSAIC` status is kept as its flag; the mosaic effect is not drawn.
- Space Pirate (`space_pirate.c`, ported in `mzm_space_pirate.c` with the
  shared AI interface `mzm_sprite_ai.h`): the normal, waiting (three) and
  second pirate sprites and their laser (`SSPRITE_SPACE_PIRATE_LASER`).
  `SpacePirateSamusDetection` scans the clipdata grid (three rows ahead
  when Samus is within 3 blocks vertically, then a diagonal up or down, one
  row while crawling, nothing toward the wall while climbing) and leaves its
  results in `gSpriteDrawOrder[0..4]`, kept as `MzmSpriteGlobals.draw_order`
  and cleared at the end of the frame, as the drawing does. Unalerted
  pirates walk (speed growing by 2 subpixels a frame to 32, a quarter of it
  per frame, two thirds up steep slopes), idle on stop-enemy blocks and turn
  around, jump over obstacles with the four `sSpacePirate*` velocity tables,
  crawl under low ceilings, climb walls, pause, wall-jump (also at
  `SPACE_PIRATE_WALLJUMP_POINT` blocks) and leave at
  `SPACE_PIRATE_ZONELINE` blocks once they reacted (`MOSAIC`, the native
  flag) while the alarm is off. A pirate that sees Samus is alerted, rings
  the alarm (`gAlarmTimer`, 480 frames, counted down at the end of
  `SpriteUpdate` and re-armed after Mecha Ridley's death), charges for 24
  frames and fires two lasers (forward, diagonally up or down, or from a
  wall) on the third frame of its shooting table. Lasers move 20 subpixels a
  frame (15 per axis diagonally), apply `CAA_BOMB_PISTOL` to the block they
  reach (hatches in the runtime), explode on solid blocks and on Samus
  (`SSC_SPACE_PIRATE_LASER` stops them) and, after their first 10 frames,
  on pirates, which freeze, flash and die after 33 frames. The last
  pirate's death stops the alarm. Waiting pirates placed in a room hide at
  their door until the alarm, then shoot through it and come out.
  `SpawnWaitingPirates` runs after each door transition: with the alarm on
  (forced on after Mecha Ridley's death) it spawns the third waiting pirate
  when the room's spriteset has it, else the first one when the spriteset
  has any pirate, at Samus's arrival point half a block back toward the
  door (`SpriteSpawnPrimary`, room slot `0x80`, `0x81` for a door without
  hatch, graphics slot 5 in spritesets 88 and 103), facing down; a
  spriteset without pirates stops the alarm. Rooms export their default
  spriteset as `S` lines (index, sprite, graphics slot); event spritesets
  are not resolved. Rooms
  export the clipdata movement of each block (`M` lines,
  `sMovementClipdataValues`) for the zonelines, wall jump points and
  stop-enemy blocks.
- Chozodia security (`searchlight.c`, `security_laser.c`,
  `searchlight_eye.c`, ported in `mzm_chozodia_security.c`): searchlights
  sweep diagonally half a pixel a frame (their direction from the sprite
  id), turning at stop-enemy blocks, flicker every frame and ring the alarm
  when Samus touches their light (`SSC_CHECK_COLLIDING`, which only sets
  `SAMUS_COLLIDING`), then hold it; a searchlight is removed when the alarm
  already rings. Security lasers (vertical up to the first block above,
  horizontal to the first on the right, 2 to 5 blocks) ring the alarm and
  mark every Space Pirate (`MOSAIC`) when tripped, play their disappearing
  table and hold the alarm hidden. Searchlight eyes move a subpixel a frame
  along their wall between stop-enemy blocks and cast a seven-block beam
  (`SSPRITE_SEARCHLIGHT_EYE_BEAM2`) that rings the alarm unless the first
  wall in it shields Samus; alerted, they shoot a projectile (12 subpixels a
  frame, 16 damage, a medium explosion on walls) every 1.5 seconds while on
  screen. They are immune to projectiles (`SP_IMMUNE_TO_PROJECTILES`: the
  invincible impact and a 2-frame stun) until the Fully Powered Suit.
  Searchlights are drawn opaque above the backgrounds (natively they sit
  behind BG1); the BG0 transparency fades of these sprites and of
  `PrimarySpriteB3` are not drawn.
- Every primary sprite records the slot it owns (`primarySpriteRamSlot`),
  passed on to its secondary sprites and its drops.
- Ripper (`ripper.c`, brown and purple): raised an eighth of a block at
  init, a random X flip from the sprite RNG, half a pixel per frame (purple:
  one), a wall probe a quarter block above its feet and 36 subpixels ahead,
  the two-part turn, and its death point 24 subpixels above its feet. Only
  super missiles and the screw attack hurt it (ROM weaknesses).
- Skree (`skree.c`, green and blue): raised a block at init, it waits
  until Samus is below it (less than 10 blocks) and within 150 subpixels
  horizontally while on screen, spins, then dives with `sSkreeFallingSpeed`
  while drifting toward Samus (a quarter of its frame count per frame, up to
  a quarter block; a wall stops the drift), lands with its hitbox bottom,
  shows its crash table after 40 frames and after 60 spawns four
  `SkreeExplosion` parts (`SpriteSpawnSecondary` in the first free slots)
  and disappears. Parts fly diagonally an eighth of a block per frame,
  rotate an eighth of a turn per frame (whole-sprite rotation, drawn by
  rotating the composed frame clockwise), ignore projectiles, hurt Samus with
  the secondary stats damage until their third frame and are removed when
  their table ends or far off screen (`SP_KILL_OFF_SCREEN`). They draw with
  the parent Skree's graphics.
- Particles (`src/runtime/mzm_effects.c`, `particle.c`): `ParticleSet` takes
  the first free of 16 slots or replaces the particle with the highest frame
  counter; the one-shot effects (sprite explosions, the screw attack kill,
  the beam, invincible, missile and super missile impacts) play their table
  once (`ParticleUpdateAnimation`: a frame ends when its counter reaches
  the timer) and are removed outside the native screen window. Explosions
  move to the high OAM priority. Deaths set their explosion at the death
  point (`SpriteUtilSpriteDeath`; the screw attack kill adds three random
  debris), projectiles set their impact on walls and sprites, and the Skree
  its huge explosion. Particles are processed after `ProjectileUpdate` and
  drawn above the sprites.
- Sprite debris (`sprite_debris.c`): 8 slots, eight arc types from the
  `sDebris*YVelocity` tables (each value used for two frames) with their
  start delays, a bounce on landing (half arcs only after 20 frames), the
  bouncing table afterwards, a 255-frame life, the vertical draw range and
  `sSpriteDebrisOam`. Processed at the start of `SpriteUpdate`. The Skree
  throws four pieces on impact.
- Sprite families (`MzmSpriteFamily`) group the sprites that share an AI and
  OAM tables; `MzmSpriteFrame.kinds` holds each family's frame durations.

- Combat. `SpriteUpdate` starts with `SpriteUtilSamusAndSpriteCollision`
  and runs between `SamusUpdate` and `ProjectileUpdate`, which ends with
  `ProjectileCheckHittingSprite`; the runtime keeps that order (the arm
  cannon offset is the one `SamusUpdate` left).
  - Samus contact (sprites with `SSC_HURTS_SAMUS` or a drop collision,
    existing and on screen,
    ignore timer zero; `SpriteUtilCheckObjectsTouching` against the pose
    hitbox): the screw attack destroys sprites weak to it
    (`SPRITE_POSE_SCREW_ATTACK_DESTROYED`, stun 17) and otherwise stuns them
    for 3 frames; then, unless Samus is invincible,
    `SpriteUtilTakeDamageFromSprite` (stats damage, suit reductions, normal
    difficulty, hurt pose) and a knockback of `SUB_PIXEL_TO_VELOCITY(8)` away
    from the sprite hitbox centre compared with `gPreviousXPosition` (none on
    a lethal hit); the sprite then ignores Samus for 15 frames. The first
    hurting or collecting contact ends the loop
    (`gIgnoreSamusAndSpriteCollision`), and every ignore timer then runs
    down once.
  - Projectiles (`ProjectileHitSprite`, `ProjectileMissileHitSprite`,
    `ProjectileSuperMissileHitSprite`, `ProjectileDealDamage`): boxes of
    +-8 subpixels (beam, missile) or +-12 (super missile); damage 2, 20 and
    100 for sprites weak to them (`WEAKNESS_BEAM_BOMBS`, `WEAKNESS_MISSILES`,
    missiles or super missiles), a 17-frame stun, `SPRITE_POSE_DESTROYED` at
    zero health; otherwise a 3-frame stun (`ProjectileHitSolidSprite`). The
    projectile is consumed. Stuns above 9 frames freeze the AI
    (`SpriteUtilIsSpriteStunned`).
  - Death (`ZoomerDeath`, `SpriteUtilSpriteDeath` with `DEATH_NORMAL`): the
    sprite becomes the drop chosen by `SpriteUtilDetermineEnemyDrop` at the
    middle of its body, hidden, or is removed. The roll is
    `(gSpriteRng * 256 + frame8 + frame16) % 1024` (0 counts as 1) against
    the stats probabilities stacked from 1024 down: power bomb, super
    missile, missile, large energy, small energy; full ammunition becomes a
    large energy drop unless Samus is at full energy, and energy drops at
    full energy become missiles when missiles are not full.
- Enemy drops (`enemy_drop.c`): 20 frames ignoring Samus and projectiles
  (shown when the timer falls below 2), the drop hitbox, collection on
  contact (small energy 5, large energy 20, missiles 2, super missiles 2,
  capped), a 200-tick timer counted every other frame that blinks the drop
  for its last 80 ticks and then removes it.
  - The weakness is the low byte of the stats' weakness column
    (`C_16_2_8(freeze chance, weaknesses)`).
  - `SSC_SPACE_PIRATE` hurts like `SSC_HURTS_SAMUS`; `SSC_SPACE_PIRATE_LASER`
    first sets the laser's stopped pose (`SPRITE_POSE_STOPPED`, its exploding
    pose).

Freezing: the Ice Beam hits follow `ProjectileIceBeamHittingSprite` and its
charged twin (the freeze flag is the high byte of the weakness column);
`ProjectileFreezeSprite` sets 240 ticks and the frozen row (15 - graphics
slot), and every ported AI then runs `SpriteUtilUpdateFreezeTimer` instead
of its states. `SpriteUtilUpdateStunTimer` flashes the stun row (14 - slot).
The exporter bakes `/frozen` and `/stun` variants of every sprite frame (a
part of row b drawn with OBJ row (b + 15) % 16 or (b + 14) % 16, taking
`frozenPaletteRowOffset` as 0); the runtime picks them from the sprite's
palette row. Frozen sprites are solid (the frozen branch of
`SpriteUtilSamusAndSpriteCollision`): after the contact damage, a falling
Samus whose feet are within 6 pixels of the top is set on it
(`SAMUS_ON_TOP`), rising into its bottom pushes her under it, and walking
into a side stops her (`SpriteUtilCheckStopSamusAgainstSolidSpriteLeft` /
`Right`). `SpriteUtilSamusStandingOnSprite` then runs after each AI: the
landing request and `STANDING_ENEMY`, the release over two frames and the
drop to `STANDING_MIDAIR` (also when the sprite is killed under her).

Not ported yet: the multiple large energy pickup,
screen-shake falls, splashes in liquids, the hard-mode and event
spritesets (where the Long Beam Dessgeega is placed), the door lock
lifecycle, screen shakes, the Chozodia alarm's music and palette, and
every other sprite AI.
