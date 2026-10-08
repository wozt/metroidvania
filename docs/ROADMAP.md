# Roadmap

## P0 - First authentic logic

1. **Reproduced:** the `ariaOfSorrow-recomp` Linux build and all 36 configured
   tests pass in an isolated workspace. A 300-frame run is fully static only
   after generating and recompiling its clean-room BIOS. Adoption remains
   limited to a project-owned adapter around licensed components; the
   unlicensed root host code is excluded. See `docs/ARIA_RECOMP_EVALUATION.md`.
2. **Integrated baseline:** mGBA boots both verified ROMs, advances frames,
   exposes authentic framebuffers, follows the exclusive backend lifecycle,
   and supports memory-only deterministic snapshots.
3. **First state view:** verified MZM mode, location, Samus, and equipment
   fields are observable through bounded read-only WRAM access.
4. **Second state view:** verified Aria mode, location, player entity,
   progression, animation, and movement fields are observable through bounded
   read-only EWRAM access with dynamic-pointer validation.
5. Define explicit field ownership and rollback before any cross-engine
   mutation.

Exit criterion: one authentic room from each ROM can be loaded independently
with verified timing, input, rendering, and a minimal read-only state view.
Both views are complete; transition ownership design is next.

## P1 - Two engines and transitions

- encapsulate memory, PPU, DMA, VBlank, audio, and save behavior per backend;
- complete an Aria -> MZM -> Aria round trip with restored state;
- compare deterministic traces and captures against a reference;
- ensure a suspended backend never affects rendering or simulation.

## P2 - Guest characters

- preserve native Soma in Aria and native Samus in MZM;
- implement Samus-in-Aria and Soma-in-MZM as local adaptations without starting
  the second engine;
- test ability, animation, damage, and collision tables.

## P3 - Progression and content

- stable cross-game save schema and migrations;
- explicit shared map, door, boss, and progression flags;
- configurable resurrection policy;
- first synergy hooks, each marked experimental.

## P4 - Distribution and GBA study

- asset-free local pipeline, license manifest, and legal review;
- performance measurements, reproducibility, and Linux packages;
- only then, a GBA ROM/RAM budget and linker prototype.
