# Native asset pipeline migration

The product target is a C11/SDL3 application that can verify user-provided
ROMs and prepare its private native-resource cache without requiring Python.
The migration is deliberately incremental: the existing Python extractors are
the reference implementation until a C replacement has a comparison fixture,
unit tests and an engine/editor integration check.

## Current foundation

`libfusion_assets` is the first native layer. Its public headers are
`include/fusion/assets/rom.h` and `include/fusion/assets/gba.h`; the
implementation is split into generic SHA-1, bounded ROM loading and GBA
primitives under `src/assets/`.
It currently:

- recognizes the pinned USA revisions of Zero Mission and Aria of Sorrow;
- requires the verified 8 MiB image size and the known SHA-1 fingerprint;
- reads into a private heap buffer without modifying the input file;
- rejects missing, oversized, truncated, incorrectly sized and mismatched
  inputs with a bounded diagnostic; and
- wipes and releases private ROM bytes through `fusion_rom_close`;
- provides a bounded little-endian binary reader; and
- decodes standard GBA 0x10 LZ77 and 0x30 RLE streams into caller-owned
  buffers without silently accepting output overflow.
- reads the fixed GBA title, game code, maker code and version fields with
  strict header bounds, and exposes the standard complement-checksum result;
- exposes that reader through verified `FusionRom` buffers, and reports the
  identity during `fusion_importer --verify`.

`fusion_importer` exposes the first orchestration contract:

```text
build/fusion_importer --list
build/fusion_importer --verify --rom-dir roms
build/fusion_importer --aria-directory "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-map "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-rooms "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-transitions "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-backgrounds "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-background-tiles "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-collisions "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-loads "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-video-memory "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-entities "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-bosses "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-summary "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-catalog "roms/Castlevania - Aria of Sorrow (USA).gba" WORLD_JSON ROOMS_TSV
build/fusion_importer --mzm-symbols "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-rom-bank "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf OUTPUT_DIR
build/fusion_importer --mzm-rooms "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-clipdata "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-backgrounds "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --dry-run --force
```

`--list` and `--verify` are functional. `--dry-run` reports the planned native
task graph and `--assets`/`--all` currently report that extraction remains
Python-authoritative; they do not claim that an extractor has been migrated.

## Migration matrix

| Python reference | Native target | Status | Equivalence gate |
|---|---|---|---|
| `scripts/verify_roms.py` | `fusion/assets/rom.h`, `src/assets/core/rom.c` | foundation complete | C SHA-1 vector, local ROM verification, malformed-file tests |
| `scripts/mzm_rom_bank.py` | `src/assets/mzm/rom_bank.c` | native extractor complete, orchestration pending | synthetic malformed-ELF, bounds and serialization fixtures; exact verified-input comparison of every filtered symbol plus byte-for-byte comparison of `MZMROM01` and the manifest |
| `scripts/mzm_room_render.py` (GBA LZ77/RLE inputs) | `src/assets/gba/decompress.c` | primitive only | compressed bytes, output length and malformed-stream comparison |
| GBA cartridge header fields (shared input contract) | `src/assets/gba/header.c`, `fusion_importer --header`, `fusion_rom_read_gba_header` | metadata reader complete | Python-generated synthetic header, native CLI field comparison, verified local ROM identity, fixed marker and complement checksum |
| `scripts/mzm_runtime_room.py`, `scripts/mzm_bg_layers.py` | `src/assets/mzm/rooms.c`, `src/assets/mzm/runtime_room.c` | room entries, Clipdata and BG0/BG1/BG2 payloads complete; other payloads pending | synthetic table, pointer, custom-RLE and LZ77 bounds; exact comparison of every descriptor field, all 331 Clipdata maps, 784 RLE background layers and 14 text tilemaps; BG3, tilesets, compositing and runtime-room serialization pending |
| `scripts/mzm_samus_pipeline.py`, `mzm_sprites.py` | `src/assets/mzm/sprites.c` | not started | frame, palette, OAM and duration comparison |
| `scripts/mzm_audio.py` | `src/assets/mzm/audio.c` | not started | table and sample comparison; audio remains outside this lot |
| `scripts/import_aos_world.py` | `src/assets/aos/world.c`, `src/assets/aos/catalog.c` | native extractor complete, orchestration pending | synthetic bounds and serialization fixtures; complete semantic comparison of `MV_AOS_WORLD_2`, byte comparison of `rooms.tsv`, deterministic atomic rewrites and all focused verified-ROM metadata comparisons |
| `scripts/aos_runtime_room.py`, `scripts/aos_room_render.py` | `src/assets/aos/runtime_room.c` | BG1 collision, graphics/palette loading and text-background tile expansion complete; rendering, compositing and room serialization pending | synthetic metadata, resource-encoding, bounds and flip fixtures; exact comparison of 342 collision maps, 343 complete VRAM/palette snapshots and 901 final background tilemaps totaling 2,358,272 entries; 104 absent and 24 affine layer slots remain explicit |
| `scripts/aos_soma_pipeline.py`, `aos_object_sprites.py`, `aos_weapons.py` | `src/assets/aos/sprites.c` | not started | animation, palette, box and weapon metadata comparison |
| `scripts/rebuild.py` | `src/assets/pipeline/rebuild.c` | not started | dependency, digest, atomic-output and dry-run comparison |

The matrix tracks technical migration only. It must not change the gameplay
parity percentages for either original reconstruction.

## Contracts and sequencing

The next migration layer should alternate to Zero Mission and connect its BG3
LZ77 tilemaps to the native room-background contract. Aria still needs native
pixel rendering, compositing and runtime-room serialization; Zero Mission
still needs tilesets, compositing, doors, sprites and runtime-room
serialization. The
completed Zero Mission ROM bank and Aria world catalog are ready for later
registration in the native task graph. The header extractor is
intentionally diagnostic: it reads a
caller-supplied file and now also runs on verified game imports, but does not
replace room or asset extraction. Each task must declare its ROM inputs,
generated private outputs, extractor version and dependencies. A C task becomes
authoritative only after its
Python output has been compared structurally and semantically (and byte-for-
byte where the format requires it). Until then the Python task remains the
production fallback and the native CLI must report that boundary clearly.

ROMs and generated files remain under ignored local paths. No ROM-derived
bytes, caches or extracted resources belong in GitHub.
