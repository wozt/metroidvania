# Side project - Nintendo 3DS port: Dual World / Dual Screen

> **Status: concept to investigate; it must not alter the PC Linux project roadmap.**
> This document is a standalone specification for Codex. Do not claim a working 3DS port until it has been built and tested on real hardware.

## 1. Vision

Create a future Nintendo 3DS homebrew port of the crossover project with:

- both original worlds;
- both characters freely selectable in either world;
- two distinct logical engines that retain their own gameplay feel;
- four character/world gameplay adaptations;
- separate health bars and character states, shared progression, separate inventories, and future synergies;
- primary gameplay on the upper screen;
- map, menus, status, and touch controls on the lower screen.

The signature feature is a world-transfer sequence across both screens. At a compatible save or transfer room:

1. the player activates the transfer;
2. the current character, room, and shared progression state are saved;
3. the character appears to leave the upper screen downward through the hinge;
4. the lower screen temporarily becomes a transition stage, where the character lands or crosses a portal;
5. in the background, the program suspends the old gameplay engine, releases temporary resources, prepares the other engine, and loads the target room without imposing an immediate forced loading screen;
6. the character returns to the upper screen, reorients, and gameplay resumes in the other world;
7. the lower screen returns to its map or menu role.

Important: the two displays are not one continuous surface. Their resolutions and aspect ratios differ, and the bezel creates a physical gap. The effect must therefore be designed as an illusion using synchronized views, fades, masks, and camera changes. It needs storyboards and tests, with an option to shorten or disable it.

## 2. Controls and UX

- Gameplay takes place on the upper screen; the lower screen hosts the touch interface.
- Instant Soma/Samus character switching uses a configurable button and must not change the active engine while the player remains in the same world.
- World travel is available only at selected save or transfer points, with an optional confirmation prompt.
- Touch menus and map navigation must also have physical-button alternatives.
- Old 3DS must be supported without requiring a C-stick, ZL, or ZR.
- Gameplay input is locked during the transition sequence, with a safe skip option.
- The UX must not require both gameplay engines and all resources to remain active at once.

## 3. Target architecture

### 3.1 Platform-independent shared core

Extract or preserve reusable interfaces from the Linux project:

- `shared_state`: progression, flags, active character, selected target world, save version;
- `world_castlevania`: adapter for the castle gameplay backend;
- `world_metroid`: adapter for the Zebes gameplay backend;
- `character_switch`: instant character swap within the active world;
- `world_transition`: transactional sequence `save -> leave -> load -> restore -> resume`, with safe failure recovery;
- `ui_model`: map, menus, status, and notifications without platform rendering code.

The shared layer must not depend on SDL windowing, desktop file paths, or a 3DS graphics API.

### 3.2 3DS-specific frontend

Create an isolated 3DS frontend using an appropriate homebrew toolchain, such as devkitARM/libctru, and a graphics backend selected after evaluation. Do not assume that the desktop SDL3 frontend can be ported unchanged.

The 3DS frontend is responsible for:

- upper-screen gameplay rendering;
- lower-screen UI rendering;
- the synchronized dual-screen transition;
- physical buttons and touch input;
- audio output;
- SD-card save data;
- application suspend and resume behavior;
- resource lifetime and memory budgets for each world;
- timing measurements on real hardware.

Do not assume that the original GBA engines can be ported directly. Introduce hardware-facing adapters, and evaluate recompilation or porting tools without making unverified claims.

## 4. Technical goals and constraints

- Treat Old 3DS and 2DS as the primary feasibility target. New 3DS optimizations may be added later but must not hide budget overruns.
- Measure RAM, VRAM, CPU time, texture bandwidth, and world-switch costs.
- Keep only one gameplay backend active at a time unless measurements justify otherwise.
- Outside the transition cinematic, the lower screen should not render a full gameplay scene unless required.
- Define a virtual resolution and framing policy for 240 x 160 source content.
- Define an audio policy for mixing, resampling, streaming, and transition fades.
- Use versioned saves, validate them, and recover to a known state after an interrupted transfer.
- Target `.3dsx` first. Consider `.cia` only later if appropriate and legal; never claim success from an uncompiled artifact.

## 5. Non-negotiable user-ROM requirement

The final game must require the user to provide their own exact ROMs locally.

- Never download, embed, commit, publish, or redistribute ROMs, BIOS files, game assets, music, sprites, extracted data, or reconstructed proprietary content.
- Use only public source code and tools whose licenses permit the intended use.
- The user supplies the required ROM files on their own machine.
- A local preparation tool validates accepted hashes, extracts or converts only what is required, and writes generated data to ignored local directories.
- ROM paths, extracted outputs, build artifacts, save files, and caches must remain ignored by Git.
- Commit only an example configuration with no private paths or ROM data.
- Distribute project code, patches, and tools separately from game content, subject to license and legal review.
- A checksum proves file identity only; it does not establish distribution rights.

## 6. Three mandatory test rooms

### 6.1 Castle test room

- simple geometry, platforms, a door, a save or transfer point, and one dummy enemy;
- playable Soma and Samus using the Castlevania-world backend;
- movement, jump, collision, and a basic attack;
- instant character switching with separate health and state;
- lower-screen status display;
- exit through the transfer point.

### 6.2 Zebes test room

- simple geometry, platforms, a door, a save or transfer point, and one dummy enemy;
- playable Soma and Samus using the Metroid-world backend;
- movement, jump, collision, and a basic attack;
- instant character switching with separate health and state;
- lower-screen status display;
- return transfer to the castle room.

### 6.3 Transition test room

The first version must work with geometric placeholders only, without ROMs or real gameplay engines:

- animate a character leaving the upper screen;
- continue the animation on the lower screen;
- simulate unloading one backend and loading the other;
- return the character to the upper screen;
- record transition timing, peak memory use, and failures.

Replace placeholders progressively with resources prepared locally from the user's ROMs.

## 7. Work requested from Codex

Follow this order:

1. Audit the existing Linux skeleton without destructive changes.
2. Study the public `mzm`, `cvaos`, and relevant recompilation or porting projects; document licenses, build assumptions, portability, and risks.
3. Produce a 3DS feasibility report covering toolchain, graphics, audio, input, filesystem access, memory budgets, Old 3DS constraints, and hardware-testing needs.
4. Create an isolated, reproducible 3DS frontend skeleton without breaking the Linux build.
5. Create the three placeholder test-room prototypes.
6. Implement the minimal shared state and simulated backend `suspend -> switch -> resume` flow.
7. Add a local ROM import interface while keeping placeholder prototypes buildable and runnable without ROMs.
8. Propose the staged integration plan for both worlds and all four character/world combinations.
9. Test first in an emulator, then on real hardware. Clearly label any unperformed test and any missing measurement.
10. Update documentation, build and test everything available, then commit in English and push if the repository is configured.

## 8. Tracking documents

Maintain the following documents for this side project:

- `docs/3ds/PROJECT_STATUS.md`;
- `docs/3ds/ARCHITECTURE.md`;
- `docs/3ds/SCREEN_TRANSITION.md`;
- `docs/3ds/FEASIBILITY.md`;
- `docs/3ds/ROM_SETUP.md`;
- `docs/3ds/TEST_PLAN.md`.

## 9. First milestone acceptance criteria

The first milestone is complete only when:

- the 3DS subproject is isolated from the Linux build;
- setup and build steps are documented;
- no proprietary file is required for the placeholder build;
- a dummy scene renders on the upper screen and a dummy map or menu renders on the lower screen;
- a full upper-to-lower-to-upper placeholder transition preserves character and progression state;
- Soma and Samus can be selected with separate health values;
- the design clearly represents two gameplay backends, even if both are simulated;
- limitations, measurements, and tests are documented honestly.

## 10. Conduct rules

- Prefer small, measurable, honest prototypes over broad speculative integrations.
- Do not modify the Linux frontend unless a shared interface requires a justified, non-destructive change.
- Preserve upstream attribution and comply with every dependency license.
- Keep all proprietary content outside the repository and outside distributed artifacts.
