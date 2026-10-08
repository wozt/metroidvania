# Zero Mission Samus raw OAM decoding

The pinned `third_party/mzm/tools/oam.py` reads Samus raw OAM as a 16-bit
header followed by **three 16-bit attributes per object (6 bytes)**. The
low 12 header bits contain the object count. Bits 12 and 13 encode the
arm-cannon front/behind ordering flags. These are **not** standard 8-byte
hardware OAM records; previous probing of 2/4-byte headers and 6/8-byte
strides was provisional. The specific 2+6 interpretation is now backed by
that upstream decoder.

Inspect a frame located by the local scanner:

```sh
python3 scripts/mzm_samus_oam_decode.py \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --frame-pointer 0x08248B34
```

The command verifies the original USA ROM hash and prints *metadata only*.
No graphics, ROM data or extracted assets are written. This milestone does
not identify the pose or produce a sprite: palette staging, body OAM offset
mapping, arm cannon graphics and effect layering are still pending.
