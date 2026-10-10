# Metroid Vania

Metroid Vania is a GPL-3.0-only research and development project for a native
PC crossover of *Metroid: Zero Mission* and *Castlevania: Aria of Sorrow*.
The target is C11, SDL3 and two distinct gameplay engines, with a shared GTK4
content editor and project-owned progression/story data.

This repository is not a playable reconstruction yet. Its active deliverables
are verified local-ROM importers, native room research tools, a Zero Mission
metatile workspace, world metadata, and the campaign design baseline. Removed
simulation and emulation prototypes are not part of the active architecture.

## Repository rules

- Supply legally obtained USA ROMs locally under `roms/`.
- Never commit or redistribute ROMs, saves, extracted graphics, audio or maps.
- Generated material stays under ignored `assets/extracted/`.
- Original ROMs and upstream submodules remain unchanged.
- Room edits are separate local overrides, never ROM mutations.

Supported fingerprints and filenames are documented in
[`docs/ROM_REQUIREMENTS.md`](docs/ROM_REQUIREMENTS.md).

## Setup and validation

On Debian 13:

```sh
sudo apt install build-essential cmake libsdl3-dev libgtk-4-dev python3 git
git submodule update --init
python3 scripts/verify_roms.py \
  --aria "roms/Castlevania - Aria of Sorrow (USA).gba" \
  --metroid "roms/Metroid - Zero Mission (USA).gba"
```

Import private assets and structural metadata:

```sh
python3 scripts/import_game_assets.py --scope all
python3 scripts/import_aos_world.py
python3 -m scripts.mzm_samus_pipeline
python3 -m scripts.audit_extracted_assets --write
```

The Aria world importer records metadata only. The general asset importer may
write proprietary decoded output, so everything it produces remains ignored.
New imports are grouped below `assets/extracted/metroid/`,
`assets/extracted/aria/`, and `assets/extracted/shared/`. Some older room and
editor caches remain at their historical paths until their consumers can be
migrated and tested independently.

The Samus command is the complete reproducible preparation path: it verifies
the ROM, reads the pinned decompilation tables and reference ELF, rebuilds body,
cannon-composed and special-pose sources, deduplicates runtime BMPs, and emits
the semantic animation map. Use `--bundle-only` only when intentionally reusing
already prepared private sources.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Run the active editor:

```sh
./build/fusion_map_editor
```

Run the shared backend without a display server:

```sh
./build/fusion_editor_cli --command=project-info --format=json
./build/fusion_map_editor --headless --command=capabilities --format=json
```

The stable command, batch, JSON and exit-code contracts are documented in
[`docs/EDITOR_CLI.md`](docs/EDITOR_CLI.md).

The editor currently supports Zero Mission native room browsing, BG1/BG2
metatile editing in private overrides, the decoded MZM world atlas, and local
Samus/Soma sprite previews. Both room browsers can render one native room or
audit an area/world through the shared headless backend. Aria reconstruction
remains partial; complete rendering and gameplay are not claimed.

Run the experimental SDL3 Zero Mission room/animation integration with:

```sh
./build/fusion_room_runtime --room brinstar_033
./build/fusion_room_runtime --check-animations \
  --samus-assets assets/extracted/metroid/sprites/samus/runtime
```

This shortcut consumes only local ignored assets. It demonstrates native room
collision diagnostics, an indexed Samus animation library, keyboard input and
SDL3 gamepad input; it is not a complete Zero Mission gameplay engine.

## Current verified data

- nine requested Zero Mission encounters;
- eleven Aria campaign bosses, with technical ROM fields left empty where not
  yet decoded;
- 29 Zero Mission save-platform rooms;
- 17 Aria save rooms decoded from the global map and room-pointer tables;
- a versioned concurrent story timeline and full campaign story bible.

See [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) for the current boundary,
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for active components, and
[`docs/ROADMAP.md`](docs/ROADMAP.md) for the next milestones.

## Local legacy archive

Superseded documentation and prototype code may exist in the local `legacy/`
directory after a migration. The entire directory is ignored by Git and is not
required to build or test the active project.
