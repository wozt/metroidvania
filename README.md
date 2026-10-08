# Metroidvania Fusion

Linux prototype for a **Castlevania: Aria of Sorrow x Metroid: Zero Mission**
crossover. The architecture requires two separate backends: Metroid rules are
not merged with Castlevania rules. Characters can be swapped instantly inside a
world; changing worlds suspends the active backend before activating the other.

## Honest status

The current executable is an **SDL3 integration harness**, not yet a port of
either game. Its rooms, collisions, and profiles use diagnostic geometry. The
user's own ROMs are required at launch and verified with SHA-1, but their code,
maps, and assets are not executed yet. Both backends display
`SIMULATED BACKEND` explicitly.

Working today:

- two rooms and two distinct backends;
- Samus and Soma in both worlds, with four physics profiles;
- separate HP, per-character KO, and continuation with the other character;
- target state preserved across character swaps and world round trips;
- deterministic collision correction or a rejected unsafe swap;
- versioned save data, dual HUD, placeholder synergy gauge, and debug overlay;
- strict local validation of both ROMs, with no network transfer.

## Debian 13 prerequisites

```sh
sudo apt install build-essential cmake libsdl3-dev python3 git
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

Controls: arrow keys or `A`/`D` to move, `Space` to jump, `J` to attack,
`Tab` to swap characters, `M` to change worlds, `F3` for the debug overlay,
`K` for diagnostic damage, `F5`/`F9` to save/load, and `Escape` to quit.

## Research sources

`third_party/mzm` and `third_party/cvaos` are pinned submodules. Their code and
licenses remain separate from this project. See
[`docs/SOURCE_AUDIT.md`](docs/SOURCE_AUDIT.md) and the living status document
[`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md).

This repository does not contain and must not distribute ROMs, BIOS images,
save data, music, maps, sprites, or extracted proprietary data. The legal status
of any future patch or distributable package must be reviewed separately.
