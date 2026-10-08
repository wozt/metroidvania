# Zero Mission Samus candidate table inspection

Use `scripts/mzm_samus_inspect.py` to examine animation records discovered by
`scripts/mzm_samus_scan.py` in your own SHA-1-verified Zero Mission USA ROM.

```sh
python3 scripts/mzm_samus_inspect.py \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --table 0x08238D7C --max-frames 24
```

The tool shows per-frame duration, four graphics subset tile counts, pointers,
and the first sixteen OAM bytes for **diagnostics only**. It does not dump game
assets to disk, identify the specific pose or assume raw Samus body OAM is a
standard 8-byte GBA OAM array. Those assumptions need source-level verification.

For the next step, correlate the first pointer table with upstream `mzm`
`src/data/samus/samus_animation_pointers.c` and inspect how Samus OAM data is
converted into hardware OAM. Only then implement authentic sprite composition.

The project's `assets/extracted/` output directory remains ignored by Git.
