# Samus body compositor

This tool uses a user-provided, hash-verified Zero Mission USA ROM. It stages
four graphics banks with the game's 2D OBJ mapping, decodes the confirmed
2-byte count + 6-byte body OAM,
and exports a diagnostic BMP to ignored `assets/extracted/`.

Run from repository root, after verifying the OBJ palette ROM offset:

```sh
python3 scripts/mzm_samus_body.py \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --frame-pointer 0x08248744 --palette-offset 0x2376A8 \
  --output assets/extracted/previews/samus_body.bmp
```

The example addresses are exact symbols from the pinned matching USA build:
Power Suit right-standing frame 0 and its default palette. This remains a
body-only diagnostic. Use `mzm_samus_sprite.py` for the complete verified idle
frame with the arm cannon. Never commit outputs.
