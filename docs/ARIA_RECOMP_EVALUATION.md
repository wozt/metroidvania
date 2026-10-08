# Aria static recompilation evaluation

Observation date: **2026-10-08**.

## Scope

This evaluation reproduces the public `ariaOfSorrow-recomp` Linux build in an
isolated temporary checkout. It does not add that project or its generated code
to this repository, and it does not make the simulated Castlevania backend
authentic.

## Revisions and licenses

| Component | Revision | License |
|---|---|---|
| `sergiomanzur/ariaOfSorrow-recomp` | `f00abd91ee8338b4378e279e1aa4a5a3a8ab8525` | No root license file; component licenses apply |
| `sergiomanzur/gbarecomp` | `19ecf46aca5f306620e56f942e8e49a958320a14` | PolyForm Noncommercial 1.0.0 |
| `mstan/recomp-ui` | `028fa5c238265090a6596d1256168bb0b69b0e60` | MIT |
| `testyourmine/cvaos` | `bc23d849d578c35ae12a5cec4e66549c3021a5be` | MIT |
| `sergiomanzur/arm-recomp-core` | `763b922f4912708d2704e7833f18addfbf8ddf33` | MIT |

The `gbarecomp` license restricts use to noncommercial purposes. In addition,
the root `ariaOfSorrow-recomp` checkout has no license file covering its own
host code. These are product-level constraints, not build details. The project
owner has confirmed a noncommercial intent and selected PolyForm Noncommercial
1.0.0 for project-authored code. This matches the `gbarecomp` license. The root
host code remains unavailable for reuse, so integration must be implemented
independently around `gbarecomp`, the MIT-licensed `cvaos` data, and locally
generated ROM-derived output. The complete dependency tree and notices must
still be reviewed before distribution.

## Reproduction environment

- Debian Linux;
- CMake 3.31.6;
- Ninja 1.12.1;
- GCC/G++ 14.2.0;
- SDL2 2.32.4;
- the locally supplied, validated Aria USA ROM with SHA-1
  `abd71fe01ebb201bcc133074db1dd8c5253776c7`.

All source checkouts, generated C++ shards, BIOS output, cache files, save data,
and binaries remained outside this repository.

## Build procedure

The following sequence was reproduced successfully in the temporary checkout.
`ARIA_ROM` must name the user's local validated ROM.

```sh
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target gba_recompile -j4
./build/third_party/gbarecomp/gba_recompile \
  --rom "$ARIA_ROM" \
  --config game.toml \
  --entry 0x080000C0 \
  --codegen-shards 16 \
  --out recomp_out
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target aria_recomp -j4
```

The cartridge pass generated 12,736 functions, 47 formal jump tables, and
92,268 static interior resume aliases. The resulting Linux executable linked
successfully.

The first headless run creates a 16 KiB clean-room BIOS image under the build
directory. Recompiling that image is necessary for a fully static measurement:

```sh
(cd third_party/gbarecomp && \
  ../../build/third_party/gbarecomp/gba_recompile \
    --bios ../../build/bios/cleanroom_bios.bin)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target aria_recomp -j4
```

Without this step, the placeholder BIOS dispatch table caused three BIOS PCs
to fall back to the interpreter during a 300-frame run: `0x00000018`,
`0x00000030`, and `0x00000044`, for 1,031 interpreted instructions. After the
clean-room BIOS was recompiled, the same cold run reported zero dispatch misses
and zero interpreted instructions with runtime self-healing disabled:

```sh
GBARECOMP_SELFHEAL_RECOMPILE=0 \
  ./build/aria_recomp --rom "$ARIA_ROM" --no-window --frames 300
```

Observed final counters:

```text
ppu_frames=300
pal_nonzero=30/1024
vram_nonzero=6224/98304
oam_nonzero=256/1024
self_heal_coverage=FULLY_STATIC dispatch_misses=0 interpreted_insns=0
```

The headless path reported `frames_presented=0` even though the PPU advanced.
A separate TCP capture at frame 1,217 returned an authentic 240 x 160 RGB
framebuffer with 115,200 bytes, 91,924 nonzero bytes, and no dispatch miss or
interpreted instruction. Its observed SHA-256 was
`38b02805d22e21765a448241c408f2d13ef96c0ecf406045138bbda6fbd5446b`.
This hash records the observed run; it is not yet a stable golden test.

## Tests

All available configured tests were built explicitly, including the targets
excluded from the default build, then executed with:

```sh
ctest --test-dir build --output-on-failure
```

Result: **36/36 tests passed** in 3.00 seconds. This covers the six Aria host
tests and 30 configured runtime, decoder, bus, DMA, timer, IRQ, PPU, codegen,
and packaging checks. It does not replace a complete gameplay playthrough.

## Integration implications

The AOT route is technically viable enough for an isolated backend proof of
concept. Integration still requires all of the following:

1. a project-owned adapter that does not copy the unlicensed root host code;
2. a reproducible external generation pipeline that never commits ROM-derived
   `recomp_out` or BIOS files;
3. a narrow library API for initialization, one-frame stepping, input, and
   framebuffer access instead of embedding the upstream executable entry point;
4. deterministic reset, snapshot, shutdown, and save-path controls;
5. isolation between the runtime's SDL2 host layer and this project's SDL3
   frontend;
6. longer gameplay traces and stable framebuffer comparisons.

The upstream runtime writes a `.sav` beside the ROM path by default. Future
automation must use an isolated temporary ROM path or add an explicit save-path
override so a developer's personal save cannot be modified.
