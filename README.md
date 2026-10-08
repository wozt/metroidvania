# Metroidvania Fusion

Linux prototype for a **Castlevania: Aria of Sorrow x Metroid: Zero Mission**
crossover. The architecture requires two separate backends: Metroid rules are
not merged with Castlevania rules. Characters can be swapped instantly inside a
world; changing worlds suspends the active backend before activating the other.

## PC-first direction (patch 0034)

`./build/fusion_dev` now opens a **global SDL3 title menu**. Choose Metroid or
Castlevania with Left/Right + Enter, or click a card. The two initial rooms
remain **project-authored diagnostic rooms**, not decoded original maps.
`M` is no longer a global teleport in the PC prototype: it only works while
standing near the right-hand **SAVE PAD** in a demo room and when the
bidirectional link is enabled in `data/world_graph.mvg`. All other rooms/points
reject travel. The separate `--authentic-interactive` mode is a legacy **mGBA
research tool**, not gameplay, and its `M` key behavior is unchanged.

The project-authored `MVGRAPH 1` file defines room IDs, world ownership, save
flags, editor coordinates and bidirectional links. These are **sample graph
nodes**, not original Metroid/Aria save-room addresses. For the optional GTK4
first-stage graph editor install `libgtk-4-dev`, reconfigure CMake, then run:

```sh
sudo apt install libgtk-4-dev
cmake -S . -B build
cmake --build build -j"$(nproc)"
./build/fusion_map_editor
```

Drag the two nodes, toggle the save-room link and press **Save graph**. The
next game launch reads these settings. This first editor is **not a tilemap
editor** yet: ROM map import, collision painting and native PC renderer remain
separate future work. GTK4 is optional for building `fusion_dev`.

mGBA stays available strictly as a reference engine for ROM-backed diagnostic
probes. The target architecture is a PC-native renderer with separate world
mechanics and a common editable room graph.

## Patch 0035 - layered PC-only tilemaps

The GTK4 editor now has **World graph** and **Tile painter** tabs. The painter
edits three layers (`Background`, `Terrain`, `Foreground`) for each local demo
save room, with palette values `0` (erase) through `4`. Click/drag to paint,
then **Save current tilemap**. Files live under `data/maps/*.mvroom` and are
strictly validated `MVTILE 1` text; `fusion_dev` loads them on startup.
The demo game still uses its original procedural collision rectangles and
portals; terrain tiles are **visual only**, and these are project-authored
palette colors, **not** extracted original sprites or real game rooms.
Restart the game after saving to view edits. GTK4 remains optional at build.


## Patch 0037 - verified local ROM asset import (first usable visuals)

The two original ROMs contain many more resources than the original character
animations. Import **only your own verified USA ROMs** into ignored local files:

```sh
python3 scripts/import_game_assets.py --scope all
./build/fusion_map_editor
./build/fusion_dev
```

The importer uses the **pinned decompilation `database.json` offsets** to copy
known raw regions into `assets/extracted/raw/metroid/` and
`assets/extracted/raw/aria/` and writes a private JSON provenance catalog at
`assets/extracted/catalog.json`. It also calls the project's existing verified
sprite decoders for Samus (idle/run/jump/attack) and Soma (six animations,
including attack/knife). The GTK4 **ROM visuals** tab displays those
real, locally decoded frames. It can also show the two native engine frame
references created by `./build/fusion_dev --authentic-arrival-preview` (these
are 240x160 framebuffer previews, NOT imported or editable room data). The PC demo rooms automatically display the
Samus/Soma BMPs instead of placeholder character boxes where extraction
succeeds. The maps' colored tile placeholders remain for now.

**A raw binary block is not an editable visual game asset.** The current
Metroid decomp database describes many more raw regions than the sparse Aria
database. Neither database supplies an end-to-end visual exporter for **every
original room, enemy/object, stat, track, sound effect, world map or cutscene**.
`catalog.json` distinguishes decoded BMP frames from raw/unknown blocks, and
also indexes relevant decomp source paths. This patch does **not** claim to
have decoded all original content or created a completed game editor. Those
are the following milestones, using the catalog as the entry point.

**Do not stage** `assets/extracted/`, ROMs, generated sounds or textures. They
are git-ignored and must stay local. The code and documentation are safe to
commit; distribution of extracted copyrighted assets is not authorized here.

## Patch 0038 - Native room descriptor catalog

Run `python3 scripts/import_game_assets.py --scope all` to index the pinned
Zero Mission source `RoomEntryRom` metadata. The GTK4 **Native rooms** tab
then lists real room numbers, tileset references, music symbols, clipdata
symbols, and default spritesets. The generated catalog is private and ignored.
This does **not** decode room tile pixels, collision layers, enemies, or
sounds; Aria native room structure is still under research.

## Patch 0039 — real MZM room graphics (partial)

Run `python3 scripts/mzm_room_render.py --area Brinstar --room 33`
after `python3 scripts/import_game_assets.py --scope all`.
Open the **Native rooms** GTK4 tab or use `./build/fusion_mzm_room_viewer`
with the generated `brinstar_033_bg1.bmp`. This reconstructs genuine
BG1/BG2 metatile graphics, but not common tiles, BG0/BG3, or objects.
See `docs/RENDER_MZM_ROOMS.md`. All outputs are private.

## Patch 0040 — configurable GTK4 editor workspace

The GTK4 editor now uses a large, three-column workspace with draggable splitters,
reorderable/detachable notebook tabs, a project explorer and a dedicated asset and
inspector dock. Tabs can be dragged between dock zones or detached into windows.
The tile painter supports Pencil, Eraser, Flood fill, Eyedropper, Grid visibility,
50–200% zoom and per-world undo/redo (one operation per mouse stroke).
**Edits apply only to original project demo `.mvroom` data**, not native ROM rooms
or proprietary graphics. ROM previews remain read-only. Original music, maps,
items, enemies and cutscenes are not yet decoded as editable game data.

## Honest status

The default executable is an **SDL3 integration harness**, not yet a native
port of either game. Its rooms, collisions, and profiles use diagnostic
geometry. The opt-in `--authentic-video-test` mode executes the user's verified
ROMs through the mGBA library and displays their real 240x160 output, one
suspended/active runtime at a time. The simulated and authentic implementations
share the same backend lifecycle; no separate emulation loop exists in the
controller. This emulated proof is not the final native backend architecture.
Optional local-only pipelines can extract the verified
Samus and Soma animation sets. Both diagnostic backends display
`SIMULATED BACKEND` explicitly.

Working today:

- two rooms and two distinct backends;
- Samus and Soma in both worlds, with four physics profiles;
- separate HP, per-character KO, and continuation with the other character;
- target state preserved across character swaps and world round trips;
- deterministic collision correction or a rejected unsafe swap;
- versioned save data, dual HUD, placeholder synergy gauge, and debug overlay;
- strict local validation of both ROMs, with no network transfer.
- reproducible authentic ROM probes, read-only verified state views for both
  games, and an interactive mGBA video mode.
- read-only transition observations with explicit source-local and
  character-owned fields;
- versioned cross-world plans using engine-native MZM door and Aria staged-room
  descriptors, plus a tested preflight/checkpoint/apply/verify/rollback
  contract;
- a local checkpointed arrival preview that moves the native character to the
  selected coordinate, captures before/after frames, and rolls the runtime
  back. Authentic backends still expose no target-engine importer.

## Debian 13 prerequisites

```sh
sudo apt install build-essential cmake libsdl3-dev libmgba-dev python3 git
```

Ghidra 12.1.4 is installed on this machine under
`/opt/ghidra_12.1.4_PUBLIC`. The `ghidra` and `ghidra-analyze-headless`
commands are available on `PATH`.

## Personal ROMs

Place your two legally obtained files in `roms/`. Default names, fingerprints,
and validation commands are documented in [`roms/README.md`](roms/README.md).
ROMs and extracted data are ignored by Git.

```sh
python3 scripts/verify_roms.py \
  --aria "roms/Castlevania - Aria of Sorrow (USA).gba" \
  --metroid "roms/Metroid - Zero Mission (USA).gba"
```

## Build, test, and run

```sh
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/fusion_dev
```

Use `--aria` and `--metroid` to supply different local paths.
`--validate-only` validates both files without opening a window.
`--authentic-probe` executes each ROM for 300 frames in sequence, captures an
in-memory runtime snapshot, advances 30 frames, restores it, replays those
frames, and prints the framebuffer hashes and replay result. It then advances
each game to its first gameplay-ready state and prints verified mode, room,
position, and character fields read directly from WRAM. The Aria path supplies
deterministic A/Start pulses to begin a new game without loading a save.
The probe also projects each verified engine state into a common Q16.16
observation, maps it to the native arrival descriptor in the opposite world,
and prints the planned room, derived position, character health, and engine
entry data. Planning does not authorize or perform a transition write. Run
`--authentic-arrival-preview` to exercise the selected coordinates inside the
already-loaded native rooms. It writes five ignored BMPs under
`captures/arrival-preview/`, verifies the position after one engine frame, and
adds a later Aria context frame before restoring the pre-write in-memory
snapshot. This is a diagnostic position preview, not a room-loader transition
or a guest-character implementation.
To test the strictly exclusive, memory-only MZM -> Aria -> MZM
round-trip, run `./build/fusion_dev --authentic-roundtrip-probe`.
It verifies suspended-source byte equality and restores the
original Aria checkpoint after the diagnostic arrival. This is
not a playable guest-character transfer or cross-room loader.

Run `--authentic-video-test`
for interactive authentic ROM output; `M` suspends the current runtime and
switches to the other one. The mode does not load or write save files. GBA
controls are arrows, `Space`/`X` for A, `J`/`Z` for B, `Enter` for Start, right
Shift for Select, and `U`/`I` for L/R.

Snapshots are currently process-local only. They are not written by F5/F9 and
are not a supported save format. Both engine state views are also read-only:
they do not update `SessionState` or write back to emulated memory. The arrival
preview is the only write path: it accepts at most 16 WRAM bytes per operation,
requires a matching in-memory checkpoint, and always verifies rollback before
exit.

To generate all four verified Power Suit animation states locally:

```sh
for animation in idle run jump attack; do
  python3 scripts/mzm_samus_sprite.py \
    --rom "roms/Metroid - Zero Mission (USA).gba" \
    --animation "$animation" \
    --output-dir assets/extracted/sprites/samus
done
```

To generate the four primary Soma states and both movement transitions locally:

```sh
for animation in idle run jump attack run_start run_stop; do
  python3 scripts/aos_soma_sprite.py \
    --rom "roms/Castlevania - Aria of Sorrow (USA).gba" \
    --animation "$animation" \
    --output-dir assets/extracted/sprites/soma
done
```

The SDL3 room renderer discovers the ignored BMPs automatically and uses the
source-defined per-frame durations for all four primary states of both
characters. Soma additionally uses the verified start/stop transitions around
its run loop. It keeps its colored rectangle fallback when files are absent.
No extracted pixels are tracked.

Controls: arrow keys or `A`/`D` to move, `Space` to jump, `J` to attack,
`Tab` to swap characters, `M` to change worlds, `F3` for the debug overlay,
`K` for diagnostic damage, `F5`/`F9` to save/load, and `Escape` to quit.

## Research sources

`third_party/mzm` and `third_party/cvaos` are pinned submodules. Their code and
licenses remain separate from this project. See
[`docs/SOURCE_AUDIT.md`](docs/SOURCE_AUDIT.md) and the living status document
[`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md). Graphics documentation is
consolidated in [`docs/MZM_SAMUS.md`](docs/MZM_SAMUS.md) and
[`docs/GBA_GRAPHICS_TOOLS.md`](docs/GBA_GRAPHICS_TOOLS.md).

This repository does not contain and must not distribute ROMs, BIOS images,
save data, music, maps, sprites, or extracted proprietary data. The legal status
of any future patch or distributable package must be reviewed separately.

## License

Original project-authored code and documentation are licensed under
**GNU GPL v3.0 only** (`GPL-3.0-only`); see [`LICENSE`](LICENSE).
GPLv3 permits commercial use subject to its conditions. Third-party code retains
its own license terms. mGBA is linked as an MPL-2.0 system library; its source
and license remain upstream. In particular, PolyForm-licensed `gbarecomp` is a
**research-only reference**, not a permitted GPLv3 runtime dependency at
present. See [`LICENSES.md`](LICENSES.md) for integration restrictions.
No license here grants rights to ROMs or proprietary game content.

### Interactive native-world switching (experimental)

With both locally validated USA ROMs available, launch:

```sh
./build/fusion_dev --authentic-interactive
```

`M` switches worlds; `Esc` exits. Arrow keys move, `Space` is GBA A, `J`
is GBA B, `Enter` is Start, `Right Shift` is Select, `U` is L and `I` is R.
The first switch into Aria uses the verified Entrance same-room arrival;
returning to a previously visited world resumes its untouched native state.
Only one mGBA runtime runs at a time. The character is still Samus in Metroid
and Soma in Castlevania; **this is not yet a guest-character crossover**.
The normal `--authentic-video-test` remains available unchanged.
