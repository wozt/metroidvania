# Verified MZM Samus idle frame

`scripts/mzm_samus_sprite.py` reconstructs Power Suit right-standing frame 0
from a user-owned, SHA-1-verified Zero Mission USA ROM. The recipe uses exact
symbols from the pinned `mzm` source and a matching `mzm_us` build, not scan
heuristics:

- `sSamusAnim_PowerSuit_Right_Standing` frame 0 at `0x08248744`;
- `sSamusPal_PowerSuit_Default` at ROM offset `0x2376A8`, first two rows;
- `sArmCannonAnim_Suit_Right_Standing` at `0x08234430`;
- forward-standing upper/lower cannon graphics at `0x082337EC` and
  `0x082338AC`.

The compositor reproduces the game's four body VRAM slots, two arm-cannon
slots, 2D OBJ tile mapping, six-byte raw OAM entries, transparent palette index
zero, and the cannon front/behind flags. It crops only fully transparent outer
pixels and emits a 32-bit alpha BMP:

```sh
python3 scripts/mzm_samus_sprite.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --output assets/extracted/sprites/samus/idle_0.bmp
```

The output directory is ignored. SDL3 loads the file automatically and falls
back to the diagnostic rectangle when it is absent. The repository contains
the extraction recipe and synthetic tests only, never the extracted image.

This milestone covers one complete frame. Additional idle, run, jump, and
attack records still need verified pose tables and per-frame cannon selection.
