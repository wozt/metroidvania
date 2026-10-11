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

Not ported yet: combat (Samus contact damage, projectile hits, death and
drops), freezing, screen-shake falls, the splash effect, the damaged sound,
the hard-mode spritesets and every other sprite AI.
