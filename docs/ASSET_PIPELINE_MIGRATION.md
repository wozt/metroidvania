# Native asset pipeline migration

The product target is a C11/SDL3 application that can verify user-provided
ROMs and prepare its private native-resource cache without requiring Python.
The migration is deliberately incremental: the existing Python extractors are
the reference implementation until a C replacement has a comparison fixture,
unit tests and an engine/editor integration check.

## Migration accounting

| Measure | Current value | Boundary |
|---|---:|---|
| ROM extraction writers running in C11 | 10 / 11 | Zero Mission audio is the sole Python extractor |
| Native orchestrator milestones | 3 / 4 (75%) | registry/graph, cache/planner, action execution and checklist generation are validated; routine discovery is audited natively, while complete inventory generation remains |
| Registered tasks still backed by Python | 2 / 15 | `inventory`, `metroid_sound` |
| Maintained Python files | 118 | 40 under `scripts/`, 78 under `tests/`; third-party submodule tools excluded |

These migration measures are independent of both games' reconstruction and
fidelity percentages.

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
- atomically writes the deterministic private JSON manifest of either verified
  ROM through `fusion_rom_write_manifest`;
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
build/fusion_importer --rom-manifest aria "roms/Castlevania - Aria of Sorrow (USA).gba" OUTPUT_JSON
build/fusion_importer --aria-directory "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-map "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-rooms "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-transitions "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-backgrounds "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-background-tiles "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-background-renders "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-background-composites "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-collisions "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-runtime-rooms "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-loads "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-video-memory "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-entities "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-bosses "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-summary "roms/Castlevania - Aria of Sorrow (USA).gba"
build/fusion_importer --aria-catalog "roms/Castlevania - Aria of Sorrow (USA).gba" WORLD_JSON ROOMS_TSV
build/fusion_importer --aria-export-rooms "roms/Castlevania - Aria of Sorrow (USA).gba" OUTPUT_DIR
build/fusion_importer --aria-export-soma "roms/Castlevania - Aria of Sorrow (USA).gba" OUTPUT_DIR
build/fusion_importer --mzm-symbols "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-rom-bank "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf OUTPUT_DIR
build/fusion_importer --mzm-rooms "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-clipdata "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-runtime-room-cores "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-runtime-room-doors "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-runtime-room-hatches "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-runtime-room-metadata "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-runtime-room-sprites "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-bitmaps "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-runtime-tables "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf [blocks.tsv|power_bomb.tsv]
build/fusion_importer --mzm-export-backgrounds "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-export-rooms "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf OUTPUT_DIR
build/fusion_importer --mzm-backgrounds "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-tilesets "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-metatiles "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-background-renders "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --mzm-background-composites "roms/Metroid - Zero Mission (USA).gba" third_party/mzm/mzm_us.elf
build/fusion_importer --dry-run --force
```

`--list` and `--verify` are functional. `--dry-run` reports the planned native
task graph and `--assets`/`--all` currently report that extraction remains
Python-authoritative; they do not claim that an extractor has been migrated.

## Migration matrix

| Python reference | Native target | Status | Equivalence gate |
|---|---|---|---|
| removed: `scripts/verify_roms.py` | `fusion/assets/rom.h`, `src/assets/core/rom.c`, `fusion_importer --verify`, `fusion_importer --rom-manifest` | native verification and rebuild-manifest cutover complete; the unreferenced Python validator is removed | C SHA-1 vector, local ROM verification, malformed-file tests, exact JSON serialization, byte-identical Aria and Zero Mission manifests and native rebuild-task integration |
| removed: `scripts/mzm_rom_bank.py` | `src/assets/mzm/rom_bank.c` | native writer and rebuild cutover complete; the Python writer is removed | synthetic malformed-ELF, bounds and serialization fixtures; exact verified-input comparison of every filtered symbol plus byte-for-byte comparison of `MZMROM01` and the manifest; recorded native digests guard the command and both output files |
| `scripts/mzm_room_render.py` (GBA LZ77/RLE inputs) | `src/assets/gba/decompress.c` | primitive only | compressed bytes, output length and malformed-stream comparison |
| GBA cartridge header fields (shared input contract) | `src/assets/gba/header.c`, `fusion_importer --header`, `fusion_rom_read_gba_header` | metadata reader complete | Python-generated synthetic header, native CLI field comparison, verified local ROM identity, fixed marker and complement checksum |
| removed: `scripts/mzm_runtime_room.py`, `scripts/mzm_bg_layers.py`, the room helpers of `scripts/mzm_sprites.py` | `src/assets/mzm/rooms.c`, `src/assets/mzm/runtime_room.c`, `src/assets/mzm/primary_sprites.c`, `src/assets/mzm/export.c` | room entries, Clipdata, BG0-BG3 payloads, standard tileset video memory, metatile tables, RGBA layer rendering, diagnostic BG1/BG2 selection, BMP/mask serialization and complete cumulative `MVROOM-NATIVE` serialization including the `P/S/F/J/N` sprite records and the BMP32 tileset sheets and text layers complete; `blocks.tsv`, `power_bomb.tsv`, the exact export `background.bmp` renderer and the complete atomic private export with its SHA-256 index complete; the rebuild cutover complete; the Python exporter is removed | synthetic table, pointer, property, custom-RLE, LZ77, VRAM-placement, palette, source-size, zero-fill, common-metatile, block-selection, screen-order, flip, transparency, foreground selection, BG1-only fallback, BMP, visibility-mask, room-core, door-target, signed-offset, hatch-facing, hatch-table, event-lock, music, layout, effect, background-row, spriteset, sprite-slot, event-variant and test-area bounds; exact comparison of every descriptor field, all 331 Clipdata maps, 784 RLE background layers and 331 text tilemaps totaling 1,443,436 cells, all 79 complete 64-KiB VRAM and 512-byte palette snapshots, 97,328 complete metatile records, 1,115 complete RGBA layers totaling 253,914,112 pixels, 330 exact background selections totaling 88,427,520 pixels with byte-identical 265,300,380-byte BMP and 88,427,520-byte mask representations, 330 byte-identical metadata-inclusive room payloads totaling 5,450,692 bytes with 330 music records, 784 block layers, 22,426 block rows and 331 text layers, 330 byte-identical complete room payloads totaling 5,548,796 bytes with 1,160 `P`, 1,518 `S`, 118 `F`, 785 `J` and 608 `N` sprite records plus all earlier records, and byte-identical BMP32 output for all 79 tileset sheets and 331 exported text layers (255,148,900 bytes), for `blocks.tsv` (4,643 bytes) and `power_bomb.tsv` (201,326 bytes), and for the export's older-renderer `background.bmp` of all 330 rooms (265,300,380 bytes), which differs from the VRAM selection in 289 rooms by design (no common blocks), and a complete native export of the same 1,015 files as `mzm_runtime_room --all` (315 rooms, 16 skipped, index included); one room without supported BG1 remains skipped, while animated updates, Mother Ship common tiles and hardware composition remain pending; recorded SHA-256 digests now guard every native command output and all 1,015 export files |
| removed: `scripts/mzm_samus_pipeline.py`, `scripts/mzm_samus_compose.py` | `src/assets/mzm/samus.c`, `src/assets/mzm/samus_tables.c`, `src/assets/core/sprite_library.c` | complete native Samus and projectile libraries and `metroid_samus` rebuild cutover; the Python writer and composer are removed | pose-enum, selection, sign, mirror, cannon-offset and indexed-BMP unit tests; all 832 variants, 1,072 Samus sequences, 4,670 indexed frames, 2,640 unique BMPs, the 566-sequence projectile library and all 3,840 output files byte-identical; recorded digests guard the command and both libraries |
| removed: `scripts/mzm_sprites.py`, `scripts/mzm_projectile_compose.py`, `scripts/mzm_message_text.py`, `scripts/gba_oam.py` | `src/assets/mzm/sprites.c`, `src/assets/mzm/sprite_tables.c`, `src/assets/mzm/message_text.c`, `src/assets/core/sprite_library.c` | complete native enemy sprite library, OAM bounds and sprite tables and `metroid_sprites` rebuild cutover; the Python writer and its helpers are removed | generated-table, palette-variant, palette-index, bounds, affine-part and empty-frame unit tests; all 20,088 sequences, 117,235 frames, 27,806 unique BMPs, `oam_bounds.tsv` and `sprites.tsv` byte-identical, the 67 `MultiSpriteData` tables read from the ROM equal to the former source evaluation; recorded digests guard the command, the index, bounds, tables and every object name, each object checked against its digest |
| `scripts/mzm_audio.py` | `src/assets/mzm/audio.c` | not started | table and sample comparison; audio remains outside this lot |
| retained decoder library: `scripts/import_aos_world.py`; removed writer: `scripts/aos_room_index.py` | `src/assets/aos/world.c`, `src/assets/aos/catalog.c` | native extractor and rebuild cutover complete; the Python catalog CLI and index writer are removed, while editor and sprite tools still use the reference decoder | synthetic bounds and serialization fixtures; complete semantic comparison of `MV_AOS_WORLD_2`, byte comparison of `rooms.tsv`, deterministic atomic rewrites and all focused verified-ROM metadata comparisons; recorded native digests guard both catalog files |
| removed: `scripts/aos_runtime_room.py`; retained: `scripts/aos_room_render.py` for editor previews and sprite tooling | `src/assets/aos/runtime_room.c`, `src/assets/aos/export.c` | BG1 collision, graphics/palette loading, tile expansion, 4-bpp rendering, static-origin depth compositing, BMP and runtime-room TSV serialization complete; the atomic private export, SHA-256 index and rebuild cutover are complete; the Python room exporter is removed | synthetic metadata, resource-encoding, bounds, tile/pixel flip, palette, transparency, depth, repetition, BMP, runtime schema, signed-transition, entity and flag fixtures; exact comparison of 342 collision maps, 343 complete VRAM/palette snapshots, 901 final tilemaps and RGBA layers totaling 150,929,408 pixels, 342 complete RGB composites totaling 61,210,624 pixels, their byte-identical 183,650,340-byte BMP representation, 342 byte-identical `AOSROOM-NATIVE` payloads totaling 2,035,730 bytes and the complete 685-file private export; 104 absent and 24 affine layer slots remain explicit and one room has no supported text layer or runtime payload; recorded SHA-256 digests guard every native command, both catalog files and all export files |
| removed: `scripts/aos_soma_pipeline.py` and the `scripts/aos_soma_sprite.py` preview decoder | `src/assets/aos/soma.c`, `src/assets/core/sprite_library.c` | complete native Soma/knife library and rebuild cutover; the Python writer and the unconsumed preview decoder are removed | synthetic bitmap, offset, deduplication, index and pruning tests; all 83 Soma animations, the one supported knife animation, 382 indexed frames, 152 unique BMPs and all 155 output files byte-identical; recorded digests guard the command and complete library |
| removed writer: `scripts/aos_weapons.py` | `src/assets/aos/weapons.c`, `src/assets/core/sprite_library.c` | complete native weapon library and rebuild cutover; the Python writer is removed | all 59 records plus the unarmed record, 60 sequences, 416 indexed frames, 255 unique BMPs, both metadata files and all 258 output files byte-identical; recorded digests guard the command and complete library |
| removed writer: `scripts/aos_object_sprites.py` | `src/assets/aos/objects.c`, `src/assets/core/sprite_library.c` | complete native traced-object library and rebuild cutover; the Python writer and its duplicate tests are removed | 348 door/enemy/effect sequences, 1,344 indexed frames, 825 unique BMPs, all 113 enemy stat records, frame boxes, anchors, probes and volleys, and all 828 output files byte-identical; recorded digests guard the command and complete library |
| removed: the extraction of `scripts/import_game_assets.py` and the writers of `scripts/import_mzm_rooms.py`; retained: their safe-writer and descriptor-decoder helpers for the Python editor tools | `src/assets/core/raw_import.c`, `src/assets/core/json.c`, `src/assets/mzm/rooms.c`, `src/assets/mzm/music_names.c` | complete native raw import and `raw_import` rebuild cutover; the unread `catalog.json` and `world_atlas.json` are no longer written | JSON, path-safety, bounds-before-write, duplicate, symlink and manifest unit tests; all 3,888 raw blocks and `rooms/metroid/rooms.tsv` byte-identical, the manifest's entries and summary equal; recorded digests guard the console, manifest, descriptors and every block |
| `scripts/rebuild.py`, `scripts/native_parity.py`, `scripts/native_inventory.py` | `include/fusion/pipeline/rebuild.h`, `include/fusion/pipeline/inventory.h`, `src/pipeline/tasks.c`, `src/pipeline/graph.c`, `src/pipeline/cache.c`, `src/pipeline/plan.c`, `src/pipeline/actions.c`, `src/pipeline/checklist.c`, `src/pipeline/inventory.c`, `fusion_rebuild`, `fusion_checklist`, `fusion_inventory` | migration in progress: registry, graph, v1 state, fingerprints, validation, incremental planning, action dispatch, post-action cache updates and checklist generation are native; native routine discovery/auditing now covers both pinned trees, while the complete inventory writer and audio remain explicit temporary Python actions | native synthetic tests cover scanner masking, C/assembly discovery, stable deduplication, calls, planning, execution, failure/recovery and path safety; complete dry runs remain byte-identical to Python; fresh checklist and Aria-backlog outputs are byte-identical; `fusion_inventory --audit-routines` matches all 6,543 routine records plus direct/ambiguous calls; next gate is declarations, data, types, constants, relations, classification and deterministic inventory sharding |

The matrix tracks technical migration only. It must not change the gameplay
parity percentages for either original reconstruction.

## Contracts and sequencing

Zero Mission's complete runtime-room payload, sprite placements and event
variants included, and its BMP32 sheets and text layers now match byte for
byte, as do `blocks.tsv`, `power_bomb.tsv` and the export's
`background.bmp` (its older renderer ported exactly, since the runtime
displays it), and `--mzm-export-rooms` writes the whole private export and
its index atomically, file for file as the former Python exporter. The
`metroid_rooms` rebuild task now runs `build/fusion_importer
--mzm-export-rooms` (skipped like a missing ROM until the importer is built),
`scripts/mzm_runtime_room.py` and `scripts/mzm_bg_layers.py` were removed with
their tests and the room-only helpers of `mzm_sprites` / `mzm_c_tables`, and
`tests/test_fusion_assets_mzm_golden.py` checks recorded SHA-256 digests of
every native command output and export file instead of the Python
reference. Aria's composite image, `AOSROOM-NATIVE` v4 serialization and
complete 685-file private export are now native too. The `aria_rooms` rebuild
task calls `build/fusion_importer --aria-export-rooms`, the former Python room
exporter and its tests are removed, and a recorded digest fixture replaces the
temporary Python equivalence test. The retained Python renderer still serves
editor previews and sprite tooling. The Aria world catalog is also registered
in the native task graph through `fusion_importer --aria-catalog`; only its
Python decoding library remains for editor and sprite consumers. The Zero
Mission ROM bank is registered too through `fusion_importer --mzm-rom-bank`,
and its former Python writer is removed. Both verified-ROM manifests are
registered through `fusion_importer --rom-manifest`; their Python output path
and the now-unreferenced standalone Python validator are removed. The header extractor is
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
