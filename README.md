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
python3 -m scripts.check_generated         # stale checklist or dashboard data? (no tests run)
```

`check_generated` writes nothing: it compares `docs/NATIVE_PARITY_CHECKLIST.md`
with the parity annotations and the registry-derived part of the dashboard
data (declared fields, missing sources, test registration, status counts)
with both registries, ignoring the generation time, git state, history and
test outcomes.

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

The *Monsters & items* tab lists every native monster and item of both games
from tracked sources only (the pinned Zero Mission sprite enum and stats, the
`mzm_sprite_family` switch and the `AOS_ENEMY` enum), marked done when the
runtime has a ported AI for them; unidentified Aria enemies keep their native
id, and "done" never means complete (sounds and unported states remain).

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

Started without arguments (or with `--intro`), the runtime boots on the
native Zero Mission intro (`src/runtime/mzm_intro.c`; A, B or START skips
it), then the title screen (`src/runtime/mzm_title_screen.c`: comets,
scrolling, title fade, sparkles, PRESS START; Return or the gamepad's Start
is START; `--title` starts there), and START opens the file select with its
three save files (`src/runtime/mzm_file_select.c`); a new game lands the
gunship in Crateria (`src/runtime/mzm_gunship.c`) and plays the "Planet
Zebes..." story text cutscene (`src/runtime/mzm_cutscene.c`) before Brinstar.
The demos and the other cutscenes are not ported yet.

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
transliteration of the pinned decompilation's `src/samus.c`: every pose
handler and its graphics loop, the pose changes, the physics, the native
point-probe collisions (corners, slopes, ledges) and the animation and arm
cannon selection of `SamusUpdateGraphicsOam`, with the liquids and hazards
(water slows her, lava, acid, heat and cold hurt); the triggers of the
elevator, save, map, zipline and escape poses are not ported yet. Controls follow the GBA layout:
arrows/WASD are the D-pad, Space/Z is A (jump), F/X is B (fire), E/Q hold L
for diagonal aim up/down. Gamepads work in both runtimes (see "Gamepads"
below). Down crouches and Down again morphs; Up unmorphs and
stands. Running jumps spin; touch a wall during a spin, press away and then A
to wall-jump. With Power Grip, hold toward a ledge while falling to hang; Up (or the
diagonal toward the ledge) turns to aim and climbs after ten frames, A with
the direction toward it climbs at once, and B fires up (Down: down) from the
hanging arm cannon. Running with Up or Down aims diagonally. Fast upward jumps show Zero
Mission's position-history echo. B fires the Power Beam from the native arm
cannon position; holding V or Left Shift (GBA R, gamepad right shoulder) arms
missiles and Tab (Select, gamepad Back) toggles super missiles. In the Morph
Ball, B places a bomb (four at most); its explosion breaks bomb blocks,
damages enemies and bounces Samus when she is on it. Holding R in the Morph
Ball arms power bombs: B places one, whose growing explosion breaks blocks,
opens power bomb hatches and damages every enemy it reaches (its look is an
approximate overlay). With the Charge Beam (a debug menu item, like the Long
Beam), holding B charges a shot released with B, and a charged spin hits
enemies as the pseudo screw. The Ice Beam (also a debug item) freezes
enemies for a few seconds, flashing before they thaw; Samus can stand
on a frozen enemy. Energy, missile, super missile and power bomb tanks are
picked up with their native message banner (the text drawn with the game's
font), raising the capacities; collected tanks stay gone. Item Chozo statues
hold their item in a ball: shoot it, then touch it to get the Long, Ice or
Wave Beam, Bombs, Speed Booster, High Jump, Screw Attack or Varia Suit (the
Plasma Beam, Space Jump and Gravity Suit statues give an "Unknown Item" that
stays inactive, as in the game before the fully powered suit). The Morph
Ball, the Power Grip and the Charge Beam are their own item sprites, touched
to be collected with their banner (the runtime grants the Morph Ball, Bombs
and Power Grip at start; `--without morph-ball`, `bombs` or `power-grip`
removes one). Rest
in a statue's hand in the Morph Ball to be refilled; a hint statue grabs
Samus, fades the music, flashes and sits down to its hint music, then
replays the room's track (its map screen is not ported). The Wave Beam crosses walls and the Plasma Beam goes
through enemies (both debug items, combinable with the other beams). With the Speed Booster
(an item of the debug menu), a long run starts the speed boost: it
breaks speed blocks and enemies, and releasing the direction skids (Down
while skidding stores a shinespark: A then launches it up, sideways with a
direction or diagonally with L or Up; in the Morph Ball with High Jump, A
launches a ballspark). Diagnostic
keys: R cycles suits (gamepad Guide), T cycles Space Jump/Screw Attack, G
toggles High Jump, M refills ammunition, H applies 20 damage, Enter restarts
after death, F6 opens the raw animation catalogue and F7 outlines the active
hitbox.

Rooms show their native Zoomers, Zeelas, Rippers, Skrees, Wavers,
Sidehoppers, Dessgeegas, Violas, Sovas, Zebs, Geegas, Zebbos, Reos, Mellas, Holtzes,
Multiviolas, Rinkas, Gerutas, Ripper IIs, Squeepts, Gamets, Skulteras, Dragons and Space Pirates (with their lasers and the Chozodia alarm,
rung by the searchlights, security lasers and searchlight eyes)
(`python3 -m scripts.mzm_sprites`, see [`docs/MZM_SPRITES.md`](docs/MZM_SPRITES.md)):
they move with their native AIs, hurt Samus on contact, take beam and missile
hits (flashing their stun colours), freeze under the Ice Beam, explode into particles and debris and leave the native enemy drops.
Other enemies and bosses are not ported yet. `--spawn X Y` places Samus's feet at
a room pixel position for tests.

The HUD and minimap are the native ones (`src/runtime/mzm_hud.c`, a port of
`hud.c` and `minimap.c`): energy digits and tanks, missile, super missile
and power bomb counters with their selection and refill graphics, the
suitless charge bar and the 3x3 minimap that marks the screens Samus
explores. They are drawn by a GBA picture processing unit emulation
(`src/runtime/gba_ppu.c`: text and affine backgrounds, objects, windows,
blending, mosaic) from a private ROM data bank exported by
`python3 -m scripts.mzm_rom_bank` (rebuild task `metroid_rom_bank`), whose
symbol table comes from the decompilation's reference ELF. The view is wider
than the GBA screen, so the minimap sits at the view's right edge. Tanks are
only collected on explored minimap tiles, as in the game.

START (Return or the gamepad's Start) opens the native pause screen
(`src/runtime/mzm_pause_screen.c`): the area map with Samus's position and
the boss icons, D-pad scrolling, the world map with A or B, the other
explored areas with SELECT, and START or B to resume; the music is lowered
meanwhile. R opens the status screen: the equipment rows, the energy and
ammunition amounts, Samus's wireframe and the item descriptions written with
the native font. L opens easy sleep: YES blanks the screen and silences the
game until SELECT, L and R are held together. Map stations grab Samus and
download the area's map: the pause screen shows the line revealing it, then
the map message appears and the station retracts. Save platforms ask to save
and write the game to the chosen file (`file_a.sav` to `file_c.sav` in SDL's
preference folder, or in `--save-dir DIR`). START on the title opens the
native file select screen: the three files with their energy, timer,
difficulty and area, a new game with its difficulty, or the continuation of
a saved file, Samus stepping out of the save platform's tube; its copy and
erase menus copy or delete the save files. `--load` resumes the most recent
file directly.

With the ROM bank, room backgrounds are rendered natively as well
(`src/runtime/mzm_room_gfx.c`): animated tiles and palettes, liquids (lava,
acid, water with its wave), tank and hatch animations.

Rooms play their native music through the ported Zero Mission sound engine
(`src/runtime/mzm_audio.c`: the sequencer, the DirectSound mixer of
`asm/soundcode.s` and the four PSG channels, emulated in
`src/runtime/gba_psg.c`), from a private sound bank exported by
`python3 -m scripts.mzm_audio` (rebuild task `metroid_sound`). A door fades
the current track over 30 frames and starts the destination room's track.
Samus, her weapons, hatches, blocks, enemies and drops request their sound
effects at the game's own `SoundPlay` points (`src/runtime/mzm_sfx.c`, with
identifiers generated by `python3 -m scripts.mzm_sound_ids`). Message
banners play the tank jingle, or insert the item jingle in front of the
room's music, which resumes when the jingle ends
(`InsertMusicAndQueueCurrent`). As on the GBA, the sound runs on its own
clock: the audio device pulls it frame by frame (UpdateAudio, then the mix
at the hardware's 65.536 kHz rate, resampled by SDL), so a slow game frame
cannot starve the device. The output
has not been compared with hardware yet. `--mute` disables sound; captures are
silent.

For headless verification, `--capture out.bmp FRAMES BUTTONS REPEAT` runs
FRAMES fixed 60 Hz updates with the given GBA button mask (right 0x1, left 0x2,
up 0x4, down 0x8, A 0x10, B 0x20, L 0x40, R 0x80, Select 0x100), re-pressing A
and B every REPEAT frames, and saves the rendered frame, for example
`SDL_VIDEO_DRIVER=dummy ./build/fusion_room_runtime --room brinstar_033
--capture shot.bmp 40 0x20 6`. `--input-script MASK:FRAMES,...` replaces the held mask
frame by frame (then the `--capture` mask applies), for example
`--input-script 0:3,8:1,0:1,8:1,0:20,0x20:1` crouches, morphs and places a
bomb. `--item NAME` enables a debug menu item at start (for example
`--item speed-booster`), and `--energy N` starts with N energy.

When Samus dies, the native game over screen offers to try again (from the
most recent save file, or from the start without one) or to return to the
title screen.

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
