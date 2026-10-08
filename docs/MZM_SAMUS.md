# Zero Mission Samus graphics

This document is the single reference for discovering, decoding, composing,
and displaying Samus graphics from a user-owned Metroid: Zero Mission USA ROM.
All tools verify SHA-1 before reading known addresses. Generated data stays
under ignored `assets/extracted/`; never commit ROM bytes or extracted images.

## Current result

`scripts/mzm_samus_sprite.py` reconstructs the four Power Suit animations used
by the SDL prototype. The addresses below come from exact symbols in the pinned
`mzm` source and a matching `mzm_us` build, not from scan heuristics.

| Prototype state | MZM source animation | Frame address | Cannon animation | Frames | Durations at 60 Hz |
|---|---|---:|---:|---:|---|
| Idle | `sSamusAnim_PowerSuit_Right_Standing` | `0x08248744` | `0x08234430` | 4 | `16,16,16,16` |
| Run | `sSamusAnim_PowerSuit_Right_Running` | `0x08248034` | `0x08234120` | 10 | ten times `2` |
| Jump | `sSamusAnim_PowerSuit_Right_Spinning` | `0x0824FE58` | `0x08234F38` | 8 | `2,1,2,1,2,1,2,1` |
| Attack | `sSamusAnim_PowerSuit_Right_Shooting` | `0x08248884` | `0x082344B0` | 3 | `2,2,4` |

The normal Power Suit palette is `sSamusPal_PowerSuit_Default` at ROM offset
`0x2376A8`; normal rendering copies its first two 16-color rows. Idle uses the
forward-standing cannon graphics at `0x082337EC` and `0x082338AC`. Attack uses
the forward-right default graphics at `0x0823236C` and `0x082324AC`.

Generate every local animation from the repository root:

```sh
for animation in idle run jump attack; do
  python3 scripts/mzm_samus_sprite.py \
    --rom "roms/Metroid - Zero Mission (USA).gba" \
    --animation "$animation" \
    --output-dir assets/extracted/sprites/samus
done
```

Use `--frame N --output assets/extracted/previews/frame.bmp` to export one
record for diagnostics. SDL loads the state files automatically, honors their
source-defined durations, resets timing on action or character changes, and
keeps the rectangle fallback when files are absent.

Batch output uses a shared transparent canvas centered on Samus's OAM X axis,
with common vertical bounds per animation. This preserves the game-space anchor
and prevents jitter between differently sized records. The idle sequence is
`0,1,2,1`; the duplicate image is intentional. Un-aimed run and spin records
use empty cannon OAM because their visible arm pixels are already in the body.

The jump state represents the repeating `SPOSE_SPINNING` section. The separate
one-record `SPOSE_STARTING_SPIN_JUMP` transition is not replayed on every loop.
This remains visual integration over prototype physics, not execution of the
original MZM state machine.

## Verified data formats

### Animation and graphics

Each 16-byte-aligned `SamusAnimationData` record contains top graphics, bottom
graphics and raw OAM pointers followed by an 8-bit duration. Each graphics
bundle begins with two tile counts, then two groups of uncompressed 4bpp tiles.
The compositor reproduces the documented 32 KiB OBJ VRAM layout:

| Part | VRAM offset |
|---|---:|
| Shoulders | `0x000` |
| Legs | `0x280` |
| Torso | `0x400` |
| Lower body | `0x680` |
| Arm cannon upper | `0x800` |
| Arm cannon lower | `0xC00` |

Samus uses 2D OBJ mapping: successive tile rows advance by 32 tile indices.
The earlier generic 1D hypothesis cannot connect the separated VRAM rows.

### Raw OAM and layering

Pinned `third_party/mzm/tools/oam.py` confirms a 16-bit header followed by
three 16-bit attributes per object, or six bytes per entry. The low 12 header
bits are the count; bits 12 and 13 place the cannon in front of or behind the
body. These are compact source records, not eight-byte hardware OAM records.

The final compositor handles signed coordinates, square/horizontal/vertical
sizes, flips, palette banks, transparent index zero, 2D tile addressing, body
and cannon VRAM staging, and front/behind order. Affine and 8bpp objects remain
unsupported because the verified animations do not require them.

### Palette

`scripts/mzm_samus_body.py` can map 1 to 16 consecutive BGR555 rows into an
explicit OBJ bank range. It rejects any OAM bank not loaded by the caller. A
body-only diagnostic using the verified standing frame is:

```sh
python3 scripts/mzm_samus_body.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --frame-pointer 0x08248744 --palette-offset 0x2376A8 \
  --palette-rows 2 --first-bank 0 \
  --output assets/extracted/previews/samus_body.bmp
```

## Diagnostic workflow

The final recipes above should be used for normal development. The following
tools remain useful when investigating additional poses; their candidate output
is not proof until matched to pinned source symbols or a matching ELF/map.

1. Discover plausible pointer tables:

   ```sh
   python3 scripts/mzm_samus_scan.py \
     --rom "roms/Metroid - Zero Mission (USA).gba" \
     --min-frames 3 --max-results 30
   ```

2. Inspect a candidate table without exporting graphics:

   ```sh
   python3 scripts/mzm_samus_inspect.py \
     --rom "roms/Metroid - Zero Mission (USA).gba" \
     --table 0xVERIFIED --max-frames 24
   ```

3. Stage a verified record into a local OBJ VRAM dump:

   ```sh
   python3 scripts/mzm_samus_frame.py \
     --rom "roms/Metroid - Zero Mission (USA).gba" \
     --frame-pointer 0xVERIFIED \
     --output assets/extracted/mzm/samus_frame_vram.bin
   ```

4. Probe historical OAM hypotheses or decode the now-confirmed format:

   ```sh
   python3 scripts/mzm_samus_oam_probe.py \
     --rom "roms/Metroid - Zero Mission (USA).gba" \
     --frame-pointer 0xVERIFIED --limit 6

   python3 scripts/mzm_samus_oam_layout.py \
     --rom "roms/Metroid - Zero Mission (USA).gba" \
     --frame-pointer 0xVERIFIED --max-entries 16

   python3 scripts/mzm_samus_oam_decode.py \
     --rom "roms/Metroid - Zero Mission (USA).gba" \
     --frame-pointer 0xVERIFIED
   ```

5. Search or preview palette candidates, then verify them against
   `src/data/samus/samus_palette_data.c` and matching binary symbols:

   ```sh
   python3 scripts/mzm_samus_palette_scan.py \
     --rom "roms/Metroid - Zero Mission (USA).gba" --max-results 25

   python3 scripts/mzm_samus_palette_scan.py \
     --rom "roms/Metroid - Zero Mission (USA).gba" \
     --offset 0xVERIFIED \
     --output assets/extracted/previews/palette.bmp
   ```

Every diagnostic validates pointers, bounds and slot capacities. Scanner and
probe output contains metadata only. The authoritative format reference is
`third_party/mzm/docs/samus/graphics.md` plus the pinned implementation.

## Runtime state anchors

The authentic mGBA backend also exposes a read-only Samus state view. The
matching MZM ELF places `gSamusData` at IWRAM `0x030013D4`; the decoder reads
the verified 32-byte structure and exposes pose, standing status, arm-cannon
direction, facing direction, quarter-pixel position, and signed velocity. It
combines those fields with verified mode, location, difficulty, and equipment
globals to reject startup transients before reporting a gameplay-ready state.

Run `./build/fusion_dev --authentic-probe` to exercise the live decoder after
the deterministic snapshot check. The probe only reads active WRAM. It does
not write memory, persist a savestate, or import values into `SessionState`.

## Remaining work

- Add the one-frame spin-jump transition when the prototype gains explicit
  animation state transitions.
- Extend recipes to aim directions, crouch, morph ball, damage, suits and
  effect overlays as gameplay needs them.
- Keep MZM state addresses and assumptions out of the separate Aria adapter.
- Keep extracted outputs local and ignored.
