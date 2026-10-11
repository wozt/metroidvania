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
  through the reference ELF symbols (verified against the ROM).
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

Not ported yet: sounds (damage, death, pickups, Skree dive and crash),
the stun palette flash, freezing and frozen (solid) sprites, power bombs
and their drops, the multiple large energy pickup, tumbling missiles,
charge beams and the pseudo screw, speed booster and shinespark kills,
screen-shake falls, splashes in liquids, the hard-mode and event
spritesets, and every other sprite AI.
