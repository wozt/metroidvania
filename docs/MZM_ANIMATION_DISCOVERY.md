# MZM Samus animation pointer discovery (local research)

`scripts/mzm_samus_scan.py` checks the USA ROM SHA-1 and scans ROM-aligned
pointer arrays for valid-looking `SamusAnimationData` records (two graphics
pointers, one OAM pointer, one duration). It also checks both graphics bundles
and capacity constraints. Results are **candidates**, not identified poses.

```sh
python3 scripts/mzm_samus_scan.py \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --min-frames 3 --max-results 30
```

For the candidate's **first frame pointer**, stage the graphics via the existing
`mzm_samus_frame.py` utility. Confirm the relevant pointers against the pinned
`third_party/mzm/src/data/samus/samus_animation_pointers.c` tables and, if
necessary, an ELF/map generated from the decompilation. The OAM format, suit
palette, gun barrel and animation names are not inferred by the scanner.

Scanner output contains numeric addresses only. Do not commit extracted assets.
