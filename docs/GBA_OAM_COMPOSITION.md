# GBA OBJ/OAM composition — local research only

Run `python3 -m unittest tests.test_gba_oam` before attempting extraction.

`scripts/gba_oam.py` reads an exact-hash-verified personal ROM and combines
uncompressed 4bpp OBJ graphics, a 256-color (16-bank) BGR555 OBJ palette,
and consecutive eight-byte OAM entries using selectable **1D or 2D mapping**.
Use `--mapping 2d` for Zero Mission's Samus staging layout. It supports
normal square/horizontal/vertical OBJ sizes, flips and transparency. It rejects
affine OBJ and 8bpp mode. Neither the ROM offsets nor the animation pointer
chains are guessed. No proprietary bytes or outputs belong in Git.

Example (replace every placeholder with *verified* numeric ROM offsets):

```sh
python3 scripts/gba_oam.py --character samus \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --tiles-offset 0xVERIFIED --tile-count 256 \
  --palette-offset 0xVERIFIED \
  --oam-offset 0xVERIFIED --oam-count 3 \
  --origin-x -32 --origin-y -32 --width 64 --height 64 --mapping 2d \
  --output assets/extracted/sprites/samus/idle_0.bmp
```

**This command is an example, not a working sprite recipe**. Zero Mission's
Samus graphics are actually composed from multiple top/bottom data subsets,
pose pointers, and a separately animated arm cannon. The game's staging of
those data into OBJ VRAM must be reproduced before a proper image can be made.
Aria of Sorrow has different sprite and animation tables. This tool is a
hardware-format primitive shared by both games, not a complete extractor.

The result is a 32-bit BGRA BMP with explicit alpha masks; SDL3 supports this
format for sprite transparency. Do not commit generated images. Verified
ROM-specific sprite assembly is implemented separately for the first MZM idle
frame; broader animation timing remains a later milestone.
