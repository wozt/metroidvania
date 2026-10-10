# Metroid Vania

Metroid Vania is a GPL-3.0-only research and development project for a native
PC crossover of *Metroid: Zero Mission* and *Castlevania: Aria of Sorrow*.
The target is C11, SDL3 and two distinct gameplay engines, with a shared GTK4
content editor and project-owned progression/story data.

This repository is not a playable reconstruction yet. Its active deliverables
are verified local-ROM importers, native room research tools, a Zero Mission
metatile workspace, world metadata, and the campaign design baseline. Removed
simulation and emulation prototypes are not part of the active architecture.

The current priority is to reconstruct both original games independently from
their title screens to their credits before crossover systems resume. The
long-term goal remains one game rebuilt automatically from the two ROMs, with
the project's editor overlays and new mechanics applied as separate layers. The
principle, its layers and the target pipeline are defined in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md#reconstruction-principle-permanent-architectural-constraint);
the first dependency-aware orchestration tasks are now implemented.

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

Import private assets and structural metadata in one incremental pass:

```sh
python3 -m scripts.rebuild --assets          # or --all, with the native tasks
python3 -m scripts.rebuild --list            # tasks, dependencies, required files
python3 -m scripts.audit_extracted_assets --write
```

`--assets` verifies each ROM, then runs the existing extractors in dependency
order: the raw import (`import_game_assets.py`, both ROMs), the Aria world
metadata, the Aria runtime rooms, Soma, object/enemy sprites and weapons, and
the Zero Mission Samus/projectile libraries (they also need the reference ELF
`third_party/mzm/mzm_us.elf`) and runtime rooms. A full pass takes about four
minutes; afterwards unchanged tasks are skipped in about two seconds. A task
reruns when its ROM, extractor modules or pinned decompilation revision
change, or when one of its private outputs is missing or was modified (room
exports carry an `export_index.tsv` of per-file SHA-256, sprite libraries are
content-addressed). Tasks whose ROM is absent are reported as skipped. Each
extractor can still be run directly, as described below.

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

Build the distributable native-source inventory and its parity checklist
without reading either ROM:

```sh
python3 -m scripts.rebuild --all
python3 -m scripts.rebuild --all --dry-run
```

The command currently registers the inventory and checklist tasks. It hashes
their real inputs and pinned-source revisions, rebuilds dependencies in order,
and skips unchanged outputs. The machine-readable inventory manifest and its
checksummed fragments are tracked under `data/native_parity/`; human validation
stays separate in `annotations.tsv` (the canonical, human-edited source),
and [`docs/NATIVE_PARITY_CHECKLIST.md`](docs/NATIVE_PARITY_CHECKLIST.md) is
generated from both. `--native` runs only these ROM-free tasks; `--all` adds
the private asset tasks above. Extractors not yet registered (room previews,
audits, editor caches) still run on their own.

## Gamepads

Both runtimes read SDL3 gamepads through `src/runtime/gba_input.c`, together
with the keyboard. The first connected gamepad is used, a gamepad plugged in
later is picked up, and an unplugged one is replaced by the next connected
one. Default layout: south button A, east and west B, shoulders L and R,
Back Select, Start Start, the D-pad and the left stick past half travel for
the GBA D-pad; opposite directions cancel out as on the GBA. `--input-map
FILE` replaces bindings, one GBA key per line (`a`, `b`, `select`, `start`,
`right`, `left`, `up`, `down`, `r`, `l`) followed by up to three gamepad
buttons (`south`, `east`, `west`, `north`, or SDL names such as
`leftshoulder`, `dpup`, `back`), plus an optional `deadzone 0..32767`;
`#` starts a comment and a bad line rejects the whole file:

```text
a south
b west east
l rightshoulder
r leftshoulder
deadzone 12000
```

The Zero Mission runtime keeps its own diagnostic pad buttons (Guide suits,
North Space Jump/Screw Attack, Start animation catalogue).

F1 (or the gamepad chord, Back + Start by default, `debug ...` in an input
map) opens the debug menu of either runtime. It freezes the game and edits
the running engine; both offer pause (then F2 steps one frame), frame step,
hitboxes and teleporting to any exported room (at the default spawn
search). `fusion_aria_runtime` adds HP and max HP, the diagnostic ATK/DEF,
each ported ability move, spawning a ported enemy 48 pixels ahead and
removing every enemy. `fusion_room_runtime` adds the suit preset, the items
whose native effect is ported (High Jump, Space Jump, Screw Attack), energy,
missiles and super missiles with their maxima, a refill and 20 damage. Up/Down
select, Left/Right change a value (L/R by ten), A toggles or runs, B
closes. Only engine features that exist are listed; nothing it does is
saved. For headless checks, `--debug-menu` opens it at start and
`--debug-input MASK,...` feeds one menu key mask per captured frame (in
captures the game keeps running under the menu).

## Development dashboard

`dashboard/index.html` is a static status page (HTML, CSS, JavaScript; no
server, no network, no ROM data). Open it directly in Firefox, Chromium or
Edge, or publish the `dashboard/` folder on GitHub Pages. Refresh its data
with:

```sh
python3 -m scripts.dashboard --run-tests   # also runs CTest (build/) and Python tests
python3 -m scripts.dashboard               # registries and repository facts only
```

The generator only reads the project. Native features come from
`data/native_parity/annotations.tsv`; Metroidvania, story, tools, assets and
platform features from `data/dashboard/features.tsv` (one row per feature:
area, stable id, category, status, fidelity, sources, tests, dependencies,
limitations, last declared validation; `@all-tests` in the tests column means
the whole suite of the generating host). Statuses are not started, in
progress, partially functional, functional, faithful, validated, blocked and
unknown. Each feature shows its declared status apart from what the run
verified: sources found, tests registered in CTest or present as Python
modules, and their results. Percentages are shares of registered features
("functional or better", "with passing tests") with their denominators;
unregistered work is not counted, and the git timeline is shown as history,
not as evidence. `data.js` holds the same data as `data.json` so the page
loads from `file://`.

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
for diagonal aim up/down. Gamepads work in both runtimes (see "Gamepads"
below). Down crouches and Down again morphs; Up unmorphs and
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
souls and the game-over screen are not implemented; bats, zombies (with their
spawners), blue crows, zombie soldiers (with their grenades) axe armors (with their returning axes) and skull archers (with their arrows) attack Soma (he flinches, is knocked back, or dies at 0 HP) and
die to his weapon with native rules (other enemies are not ported
yet; `--atk/--def/--hp` set diagnostic stats, the new-game stats are not
traced); wooden doors open natively with
their native sprite and palette cycle (`python3 -m scripts.aos_object_sprites`).
Leaving a room
through its edge loads the native neighbour at the native arrival position;
`--audit-transitions` checks every exported transition. The spawn
point is a test placement (a floor near the room centre, or `--spawn X Y`).
Arrows (or a gamepad, see "Gamepads") move, Down crouches, X/F attacks (B; `--weapon none|INDEX` after
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
