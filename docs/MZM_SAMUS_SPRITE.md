# Verified MZM Samus animations

`scripts/mzm_samus_sprite.py` reconstructs Power Suit animations from a
user-owned, SHA-1-verified Zero Mission USA ROM. The recipe uses exact
symbols from the pinned `mzm` source and a matching `mzm_us` build, not scan
heuristics:

- `sSamusAnim_PowerSuit_Right_Standing` frame 0 at `0x08248744`;
- `sSamusPal_PowerSuit_Default` at ROM offset `0x2376A8`, first two rows;
- `sArmCannonAnim_Suit_Right_Standing` at `0x08234430`;
- forward-standing upper/lower cannon graphics at `0x082337EC` and
  `0x082338AC`.

The source table contains the sequence `0, 1, 2, 1`; each record lasts 16 game
updates. The compositor reproduces the game's four body VRAM slots, two arm-cannon
slots, 2D OBJ tile mapping, six-byte raw OAM entries, transparent palette index
zero, and the cannon front/behind flags. It crops only fully transparent outer
pixels and emits a 32-bit alpha BMP:

```sh
python3 scripts/mzm_samus_sprite.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --output-dir assets/extracted/sprites/samus
```

The output directory is ignored. SDL3 loads the four files automatically at
the verified 3.75 frames/s cadence and falls
back to the diagnostic rectangle when it is absent. The repository contains
the extraction recipe and synthetic tests only, never the extracted image.

Use `--frame N --output path.bmp` to export a single selected record for
diagnostics.

## Running

The normal un-aimed right-running cycle is also verified from exact symbols:

- `sSamusAnim_PowerSuit_Right_Running` at `0x08248034`, ten live records;
- `sArmCannonAnim_Suit_Right_None_Running` at `0x08234120`;
- two game updates per frame, or 30 frames/s at the GBA update rate.

Every selected cannon record points to `sArmCannonOam_Empty`. This is expected:
the un-aimed running body records already contain all visible arm pixels, while
the cannon animation retains muzzle offsets for gameplay. The extractor checks
the empty OAM header instead of silently dropping a layer.

```sh
python3 scripts/mzm_samus_sprite.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --animation run \
  --output-dir assets/extracted/sprites/samus
```

Batch generation places every frame on a shared transparent canvas centered on
Samus's OAM X axis, with common top and bottom bounds. SDL can therefore center
and ground differently sized poses without introducing animation jitter.

Idle and run are complete. Jump and attack still require a canonical source
pose selection and verification of their cannon/effect layers.
