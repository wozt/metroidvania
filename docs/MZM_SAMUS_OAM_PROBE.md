# Samus OAM layout probe

This diagnostic examines raw OAM records referenced by verified Samus frame
pointers in the personally supplied Zero Mission USA ROM. It does not extract
sprites or select a winning interpretation automatically.

```sh
python3 scripts/mzm_samus_oam_probe.py \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --frame-pointer 0x08248B34 --limit 6
```

The program prints the first OAM header and compares possible 2-/4-byte
header sizes and 6-/8-byte object record strides. Some candidates can appear
plausible by chance; confirm against the pinned `mzm` implementation before
using the structure to reconstruct poses. No ROM-derived output is saved.
