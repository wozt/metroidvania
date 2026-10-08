# Metroidvania Fusion

Linux prototype for a **Castlevania: Aria of Sorrow x Metroid: Zero Mission**
crossover. The architecture requires two separate backends: Metroid rules are
not merged with Castlevania rules. Characters can be swapped instantly inside a
world; changing worlds suspends the active backend before activating the other.

## Honest status

The current executable is an **SDL3 integration harness**, not yet a port of
either game. Its rooms, collisions, and profiles use diagnostic geometry. The
user's own ROMs are required at launch and verified with SHA-1, but their code
and maps are not executed yet. An optional local-only pipeline can extract one
verified Samus frame from the user's ROM. Both backends display
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

To generate the first verified Power Suit idle frame locally:

```sh
python3 scripts/mzm_samus_sprite.py \
  --rom "roms/Metroid - Zero Mission (USA).gba" \
  --output assets/extracted/sprites/samus/idle_0.bmp
```

The SDL3 room renderer discovers that ignored BMP automatically and keeps its
colored rectangle fallback when it is absent. No extracted pixels are tracked.

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

## License

Original project-authored code and documentation are licensed under
**GNU GPL v3.0 only** (`GPL-3.0-only`); see [`LICENSE`](LICENSE).
GPLv3 permits commercial use subject to its conditions. Third-party code retains
its own license terms. In particular, PolyForm-licensed `gbarecomp` is a
**research-only reference**, not a permitted GPLv3 runtime dependency at
present. See [`LICENSES.md`](LICENSES.md) for integration restrictions.
No license here grants rights to ROMs or proprietary game content.
