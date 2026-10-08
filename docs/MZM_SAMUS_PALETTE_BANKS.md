# Samus OBJ multi-bank palette composition

The sampler from patch 0020 assumed OAM bank zero. This patch adds optional
`--palette-rows` (1..16) and `--first-bank` (0..15), mapping a contiguous
ROM palette range to the requested 16-color OBJ banks. Defaults preserve the
old single-bank behavior. Missing banks trigger a hard error; no wrong colors
are silently substituted.

Example using the default Power Suit palette symbol from the matching build:

```sh
python3 scripts/mzm_samus_body.py --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --frame-pointer 0x08248744 --palette-offset 0x2376A8 \
  --palette-rows 2 --first-bank 0 \
  --output assets/extracted/previews/samus_body.bmp
```

The normal runtime path copies the first two rows. This command is still an
incomplete body-only preview; use `mzm_samus_sprite.py` for the complete idle
frame. Images are kept out of Git.
