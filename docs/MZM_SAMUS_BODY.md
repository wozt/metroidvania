# Samus body-only compositor

This tool uses a user-provided, hash-verified Zero Mission USA ROM. It stages
four graphics banks, decodes the confirmed 2-byte count + 6-byte body OAM,
and exports a diagnostic BMP to ignored `assets/extracted/`.

Run from repository root, after verifying the OBJ palette ROM offset:

```sh
python3 scripts/mzm_samus_body.py \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --frame-pointer 0x08248B34 --palette-offset 0xVERIFIED \
  --output assets/extracted/previews/samus_body.bmp
```

`0xVERIFIED` must be replaced with an established palette byte offset. A
single 16-color palette bank is supported. This is a body-only preview, not
an authentic complete Samus sprite: the arm cannon, effects, suit palette
selection and pose identification remain outstanding. Never commit outputs.
