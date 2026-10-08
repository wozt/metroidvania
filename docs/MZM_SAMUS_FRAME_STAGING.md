# Zero Mission Samus frame staging (local research)

The pinned `third_party/mzm/docs/samus/graphics.md` describes a 13-byte
`SamusAnimationData` record: upper graphics pointer (4), lower graphics
pointer (4), OAM pointer (4), and frame duration (1).

`scripts/mzm_samus_frame.py` resolves a **verified** GBA frame address,
decodes both graphics bundles (two tile-count bytes followed by uncompressed
4bpp tile groups), and reproduces four documented OBJ VRAM slots:

- shoulders: 0x000; torso: 0x400
- legs: 0x280; lower body: 0x680

It rejects invalid pointers, out-of-bounds data and oversized slot uploads.
The 32 KiB output contains game graphics and is **local only**.

```sh
python3 scripts/mzm_samus_frame.py \
  --rom 'roms/Metroid - Zero Mission (USA).gba' \
  --frame-pointer 0xVERIFIED_GBA_POINTER \
  --output assets/extracted/mzm/samus_frame_vram.bin
```

This is a diagnostic VRAM staging dump, **not a playable Samus sprite**.
The exact starting frame pointer must come from a verified upstream table
or symbol map; this patch does not fabricate it. Next steps are decoding
the game's raw Samus OAM layout, verifying palette staging and composing
OAM tiles onto a transparent framebuffer. Arm cannon and effects are
independent overlays requiring additional frame data.
