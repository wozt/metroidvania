# Patch 0018 - bounded Zero Mission Samus OAM layout hypotheses

The previously added OAM probe explored possible header and entry lengths.
This patch adds `scripts/mzm_samus_oam_layout.py`, which respects the possible
object count from the first header halfword, rejects implausible counts and
out-of-bounds reads, and reports four **unverified** combinations of two- or
four-byte headers and six- or eight-byte OBJ records.

```sh
python3 scripts/mzm_samus_oam_layout.py \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --frame-pointer 0x08248B34 --max-entries 16
```

Only numerical diagnostics are printed. The code does not export sprites or
ROM bytes, nor does it claim any layout is authentic. Compare output with the
pinned `third_party/mzm` source before choosing a layout. Next: implement the
confirmed Samus-specific OAM layout and palette staging.
