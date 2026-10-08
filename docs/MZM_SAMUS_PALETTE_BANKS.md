# Samus OBJ multi-bank palette composition

The sampler from patch 0020 assumed OAM bank zero. This patch adds optional
`--palette-rows` (1..16) and `--first-bank` (0..15), mapping a contiguous
ROM palette range to the requested 16-color OBJ banks. Defaults preserve the
old single-bank behavior. Missing banks trigger a hard error; no wrong colors
are silently substituted.

Example (replace palette offset with an independently verified value):

```sh
python3 scripts/mzm_samus_body.py --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --frame-pointer 0x08248B34 --palette-offset 0xVERIFIED \
  --palette-rows 3 --first-bank 0 \
  --output assets/extracted/previews/samus_body.bmp
```

This is still an incomplete body-only preview: missing arm cannon, effects,
and confirmed animation/pose labels. Images are kept out of Git.
