# Zero Mission local palette candidate scanner

The pinned MZM decompilation lists Power Suit default palettes as three
16-color BGR555 rows. `scripts/mzm_samus_palette_scan.py` searches for
palette-like regions in the user's **verified USA ROM**. The heuristic
does not identify which palette belongs to Samus; validate an offset
against the pinned `src/data/samus/samus_palette_data.c` and binary
references before using it for a final asset.

Run:

```sh
python3 scripts/mzm_samus_palette_scan.py --rom 'roms/Metroid - Zero Mission (USA).gba' --max-results 25
python3 scripts/mzm_samus_palette_scan.py --rom 'roms/Metroid - Zero Mission (USA).gba' --offset 0xVERIFIED --output assets/extracted/previews/palette.bmp
```

Use a real numeric offset instead of `0xVERIFIED`. All outputs remain
under ignored `assets/extracted`. No extracted assets are included in source.
