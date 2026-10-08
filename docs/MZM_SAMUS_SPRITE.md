# Verified MZM Samus idle frame

`scripts/mzm_samus_sprite.py` reconstructs the four-frame Power Suit
right-standing cycle
from a user-owned, SHA-1-verified Zero Mission USA ROM. The recipe uses exact
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

This milestone covers the complete basic idle cycle. Run, jump, and attack
records still need verified pose tables and per-frame cannon selection. Use
`--frame N --output path.bmp` to export a single idle record for diagnostics.
