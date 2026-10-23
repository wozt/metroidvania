# Native asset pipeline migration

The product target is a C11/SDL3 application that can verify user-provided
ROMs and prepare its private native-resource cache without requiring Python.
The migration is deliberately incremental: the existing Python extractors are
the reference implementation until a C replacement has a comparison fixture,
unit tests and an engine/editor integration check.

## Current foundation

`libfusion_assets` is the first native layer. Its public header is
`include/fusion/assets/rom.h`; the implementation is split into generic
SHA-1 and bounded ROM loading under `src/assets/core/`. It currently:

- recognizes the pinned USA revisions of Zero Mission and Aria of Sorrow;
- requires the verified 8 MiB image size and the known SHA-1 fingerprint;
- reads into a private heap buffer without modifying the input file;
- rejects missing, oversized, truncated, incorrectly sized and mismatched
  inputs with a bounded diagnostic; and
- wipes and releases private ROM bytes through `fusion_rom_close`.

`fusion_importer` exposes the first orchestration contract:

```text
build/fusion_importer --list
build/fusion_importer --verify --rom-dir roms
build/fusion_importer --dry-run --force
```

`--list` and `--verify` are functional. `--dry-run` reports the planned native
task graph and `--assets`/`--all` currently report that extraction remains
Python-authoritative; they do not claim that an extractor has been migrated.

## Migration matrix

| Python reference | Native target | Status | Equivalence gate |
|---|---|---|---|
| `scripts/verify_roms.py` | `fusion/assets/rom.h`, `src/assets/core/rom.c` | foundation complete | C SHA-1 vector, local ROM verification, malformed-file tests |
| `scripts/mzm_rom_bank.py` | `src/assets/mzm/rom_bank.c` | not started | symbol-table byte and bounds comparison |
| `scripts/mzm_runtime_room.py` | `src/assets/mzm/rooms.c` | not started | room structure and semantic render comparison |
| `scripts/mzm_samus_pipeline.py`, `mzm_sprites.py` | `src/assets/mzm/sprites.c` | not started | frame, palette, OAM and duration comparison |
| `scripts/mzm_audio.py` | `src/assets/mzm/audio.c` | not started | table and sample comparison; audio remains outside this lot |
| `scripts/import_aos_world.py` | `src/assets/aos/world.c` | not started | room/map/transition structure comparison |
| `scripts/aos_runtime_room.py` | `src/assets/aos/rooms.c` | not started | collision, entity and transition comparison |
| `scripts/aos_soma_pipeline.py`, `aos_object_sprites.py`, `aos_weapons.py` | `src/assets/aos/sprites.c` | not started | animation, palette, box and weapon metadata comparison |
| `scripts/rebuild.py` | `src/assets/pipeline/rebuild.c` | not started | dependency, digest, atomic-output and dry-run comparison |

The matrix tracks technical migration only. It must not change the gameplay
parity percentages for either original reconstruction.

## Contracts and sequencing

The next native layers should add generic bounded binary readers and GBA
decompression primitives, then port one small metadata extractor at a time.
Each task must declare its ROM inputs, generated private outputs, extractor
version and dependencies. A C task becomes authoritative only after its
Python output has been compared structurally and semantically (and byte-for-
byte where the format requires it). Until then the Python task remains the
production fallback and the native CLI must report that boundary clearly.

ROMs and generated files remain under ignored local paths. No ROM-derived
bytes, caches or extracted resources belong in GitHub.
