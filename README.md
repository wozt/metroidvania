# Metroid Vania

Metroid Vania is a GPL-3.0-only research and development project for a native
PC crossover of *Metroid: Zero Mission* and *Castlevania: Aria of Sorrow*.
The target is C11, SDL3 and two distinct gameplay engines, with a shared GTK4
content editor and project-owned progression/story data.

This repository is not a playable reconstruction yet. Its active deliverables
are verified local-ROM importers, native room research tools, a Zero Mission
metatile workspace, world metadata, and the campaign design baseline. Removed
simulation and emulation prototypes are not part of the active architecture.

The long-term goal is a game rebuilt automatically from the two ROMs, with the
project's editor overlays and new mechanics applied as separate layers. The
principle, its layers and the target pipeline are defined in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md#reconstruction-principle-permanent-architectural-constraint);
most of that pipeline is not implemented yet.

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
python3 -m scripts.aos_soma_pipeline
python3 -m scripts.audit_extracted_assets --write
```

The Aria world importer records metadata only. The general asset importer may
write proprietary decoded output, so everything it produces remains ignored.
New imports are grouped below `assets/extracted/metroid/`,
`assets/extracted/aria/`, and `assets/extracted/shared/`. Some older room and
editor caches remain at their historical paths until their consumers can be
migrated and tested independently.

The Samus command is the complete reproducible preparation path: it verifies
the ROM, reads the native Samus pointer tables from the pinned decompilation
and the matching reference ELF, composes every body, arm cannon and suit
palette combination exactly as the game selects them, deduplicates the runtime
BMPs and emits the semantic animation map.

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
python3 -m scripts.mzm_runtime_room --area Brinstar --room 33
./build/fusion_room_runtime --room brinstar_033
./build/fusion_room_runtime --check-animations \
  --samus-assets assets/extracted/metroid/sprites/samus/runtime
```

Any of the 330 Zero Mission rooms can be exported the same way and opened as
`--room <area>_<NNN>` (for example `--room brinstar_001`). The export holds the
partial BG1-over-BG2 render and every Clipdata cell resolved to its native
collision type; the runtime applies `ClipdataConvertToCollision` for solid
blocks, all six floor slopes, doors, tanks and enemy-only blocks. Rooms are
linked through their native door tables: shoot a hatch to open it (beams open
blue hatches, missiles red ones, super missiles green ones) and walk through
the door transition to load the destination room at its native exit. Hatch
shells are tinted rectangles until the common hatch tiles are rendered.

This shortcut consumes only local ignored assets. Samus is driven by a
pose controller ported from the pinned Zero Mission decompilation (native
physics constants, per-pose hitboxes and pose transitions); it is not yet a
complete Zero Mission gameplay engine. Controls follow the GBA layout:
arrows/WASD are the D-pad, Space/Z is A (jump), F/X is B (fire), E/Q hold L
for diagonal aim up/down. On a gamepad, South is A, East/West is B and the
left shoulder is L. Down crouches and Down again morphs; Up unmorphs and
stands. Running jumps spin; touch a wall during a spin, press away and then A
to wall-jump. With Power Grip, hold toward a ledge while falling to hang, then
press A while holding toward it to climb. Fast upward jumps show Zero
Mission's position-history echo. B fires the Power Beam from the native arm
cannon position; holding V or Left Shift (GBA R, gamepad right shoulder) arms
missiles and Tab (Select, gamepad Back) toggles super missiles. Diagnostic
keys: R cycles suits (gamepad Guide), T cycles Space Jump/Screw Attack, G
toggles High Jump, M refills ammunition, H applies 20 damage, Enter restarts
after death, F6 opens the raw animation catalogue and F7 outlines the active
hitbox.

For headless verification, `--capture out.bmp FRAMES BUTTONS REPEAT` runs
FRAMES fixed 60 Hz updates with the given GBA button mask (right 0x1, left 0x2,
up 0x4, down 0x8, A 0x10, B 0x20, L 0x40, R 0x80, Select 0x100), re-pressing A
and B every REPEAT frames, and saves the rendered frame, for example
`SDL_VIDEO_DRIVER=dummy ./build/fusion_room_runtime --room brinstar_033
--capture shot.bmp 40 0x20 6`.

Run the experimental SDL3 Aria of Sorrow runtime (Soma in one native room)
with:

```sh
python3 -m scripts.aos_soma_pipeline
python3 -m scripts.aos_runtime_room --all
python3 -m scripts.aos_object_sprites
python3 -m scripts.aos_weapons
./build/fusion_aria_runtime --area 0 --room 6
```

Soma uses the movement, collision, jump, crouch, landing, ability-move and
animation rules ported from the cvaos assembly (`docs/AOS_SOMA.md`); attacks,
souls and the game-over screen are not implemented; bats hang, swoop, hurt
Soma (he flinches, is knocked back, or dies at 0 HP) and die to his weapon
with native rules (other enemies are not ported
yet; `--atk/--def/--hp` set diagnostic stats, the new-game stats are not
traced); wooden doors open natively with
their native sprite and palette cycle (`python3 -m scripts.aos_object_sprites`).
Leaving a room
through its edge loads the native neighbour at the native arrival position;
`--audit-transitions` checks every exported transition. The spawn
point is a test placement (a floor near the room centre, or `--spawn X Y`).
Arrows move, Down crouches, X/F attacks (B; `--weapon none|INDEX` after
`python3 -m scripts.aos_weapons`, unarmed by default; blades of weapon
classes 0, 2 and 3 are drawn, `--hitboxes` outlines their hitbox),
Space/Z jumps (A), Down + jump drops through
one-way platforms or slides, Q/Left Shift is L (backdash; Up + L high jump),
jump in the air jumps again and Down + jump after it dive-kicks. All five
ability moves are enabled by default (no soul inventory yet); `--moves MASK`
selects them (backdash 0x1, slide 0x2, mid-air jump 0x4, dive kick 0x8, high
jump 0x10). `--check` validates the room and library without a window, and
`--capture out.bmp FRAMES BUTTONS` runs FRAMES updates with a held GBA mask
(`--repeat N` releases it one frame in N to repeat presses)
(right 0x10, left 0x20, up 0x40, down 0x80, A 0x01, B 0x02, R 0x100,
L 0x200) and saves the frame.

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
