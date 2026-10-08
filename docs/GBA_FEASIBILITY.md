# Future GBA feasibility

Linux remains the primary milestone. No current result proves that the complete
crossover can fit or run on physical GBA hardware.

Constraints to measure:

- 32 MiB addressable ROM and possible flash cartridge or mapper constraints;
- 256 KiB EWRAM, 32 KiB IWRAM, 96 KiB VRAM, 1 KiB OAM, and 1 KiB palette RAM;
- scanline, VBlank, DMA, and sprite-count budgets;
- overlays or banks for two code and data sets;
- coexistence of both m4a audio engines, voice tables, and mixer memory;
- SRAM/Flash save layout and space for two progression states;
- `agbcc`/devkitARM toolchains, linking, overlays, and relocation;
- map, tileset, sprite, music, and duplicated resource size.

A plausible design would keep only one engine resident while storing compact
shared state and the other world's data in ROM. The GBA does not provide general
dynamic loading, however, so overlays or banked calls would require a dedicated
linker strategy and strict relocation rules. The linker maps, ROM/RAM headroom,
and worst-frame cost of both original builds must be measured first. Status:
**not started; feasibility unknown**.
