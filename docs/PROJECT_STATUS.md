# Project status

Last updated: **2026-10-25**.

## Patch 0521 - Native Zero Mission room export in production

- The `metroid_rooms` rebuild task now runs `build/fusion_importer
  --mzm-export-rooms ROM ELF assets/extracted/metroid/rooms/runtime` instead
  of the Python exporter. Its inputs are the ROM, the ELF and the importer
  binary (a rebuilt importer reruns it); it depends on `metroid_rom` only, no
  longer on the raw import, and it is skipped like a missing ROM until the
  importer is built. The export index validator is unchanged.
- Checked in production: the task rewrites the private export in about
  7 seconds, a second run is up to date, a modified `room.tsv` is detected and
  rebuilt, an identical rebuilt binary keeps it up to date, and
  `fusion_room_runtime --room` loads and draws the exported rooms (Brinstar,
  Kraid, Crateria, Norfair and Chozodia captures).
- Removed the Python production path the C writer replaces:
  `scripts/mzm_runtime_room.py`, `scripts/mzm_bg_layers.py`, their tests, the
  room-sprite helpers of `scripts/mzm_sprites.py` and
  `block_table_lines` / `power_bomb_window_lines` of `scripts/mzm_c_tables.py`
  with their tests, and the equivalence tests that compared the native
  outputs with them.
- `tests/test_fusion_assets_mzm_golden.py` replaces those comparisons: it
  checks the SHA-256 of the whole output of the 16 native inspection commands
  and of each of the 1,015 export files against
  `tests/fixtures/mzm_native_golden.tsv`, recorded from the native binary right
  after its byte parity with Python was proven. The fixture holds digests and
  counts only. The test takes about 35 seconds instead of several minutes.
- The runtime's export hints, the README and the room documents now name the
  native export; the parity and dashboard registries point at the native
  sources and the digest test. No status changes.

## Patch 0520 - One test run per work cycle

- `scripts/dashboard.py --run-tests` no longer runs the Python suite twice:
  its CTest run skips `python_tooling_and_data`, whose outcome now comes from
  the suite's own recorded run. It also prints the pass/fail totals and the
  failed tests and exits non-zero when one failed, so it serves as the single
  full test run of a cycle (covered by a new dashboard test).
- `AGENTS.md` now asks for targeted tests while working and one ordered end
  of cycle run (build, `native_parity`, `dashboard --run-tests`,
  `check_generated`) instead of separate `unittest`, `ctest` and leak-check
  runs, which ran the slow Python suite up to four times per cycle.

## Patch 0519 - Native Zero Mission room export writer

- Added `fusion_mzm_export_runtime_rooms` and `--mzm-export-rooms ROM ELF
  OUTPUT_DIR`: the complete private export of `scripts/mzm_runtime_room.py
  --all` from the ROM and ELF alone. In the reference's order it writes each
  exportable room's `<area>_<NNN>/bg0.bmp` / `bg3.bmp`, `room.tsv` and
  `background.bmp`, `blocks.tsv` and `power_bomb.tsv` with the first room,
  each used `tilesets/tileset_NN.bmp` once, then `export_index.tsv` (files
  sorted by path with their SHA-256, skipped rooms sorted, reasons
  whitespace-collapsed). Every file goes through the atomic writer, and
  symlinked outputs, temporaries or directory components are refused, as in
  `private_path` / `write_atomic`.
- Measured and reproduced the production skip rules: no RLE BG1, no BG1 tile
  in the tileset graphics, and an area name that is not purely alphabetic,
  because `produce()` reads the hatch lock events without the equivalence
  tests' `isalpha()` guard and `parse_hatch_lock_events` rejects the Test1 to
  Test3 areas. Production therefore exports 315 rooms and skips 16; the
  console summary matches the reference's line for line.
- Added a native SHA-256 (FIPS 180-4, `fusion_sha256_hex`) with the standard
  vectors and the 55/56/64-byte padding boundaries.
- A real-ROM test runs both exporters into temporary roots and requires the
  same 1,015 files byte for byte, index included, the same console output,
  an identical rerun without leftover temporaries, and the refusal of a
  symlinked or non-directory output. The native export takes about 9 seconds
  against about 2 minutes 40 for the Python one on this machine.
- The rebuild task still runs the Python exporter; switching
  `metroid_rooms` to the native writer, checking runtime/editor consumption
  and incremental rebuilds, then removing the Python production path come
  next. Gameplay status does not change.

## Patch 0518 - Native Zero Mission export tables and background image

- Ported the export's `background.bmp` renderer exactly
  (`fusion_mzm_render_export_background`): the older diagnostic path of
  `scripts/mzm_room_render.py` that `produce()` uses, not the VRAM
  selection. It draws only the tileset's own metatiles (common blocks such
  as hatches stay black, since the runtime animates them from the
  `H/V/A` records), takes tiles from the decompressed tileset graphics at
  the character base inferred from BG1 (`graphic_base`: 192, or 704 with
  more coverage), maps BG rows 3 to 15 to source palette rows 1 to 13, paints
  color 0 without making it visible, and composes BG1 over BG2 only when
  BG2's property is exactly `BG_PROP_RLE_COMPRESSED` (`BG_PROP_MOVING`
  layers are not composed). A room whose BG1 reaches no tile graphics is
  reported as skipped, like the Python `ValueError`. The new
  `--mzm-export-backgrounds` command and a real-ROM equivalence test match
  all 330 exportable rooms' bitmaps byte for byte (265,300,380 bytes), with
  synthetic coverage of composition, the `BG_PROP_MOVING` rule, painted
  color 0, common blocks and the skip.

- Added `fusion_mzm_serialize_block_tables` for `blocks.tsv`: the action
  damage types, block behaviors (with the `isSpeedboost` / `isBombChain` bit
  fields), weaknesses, reforming tiles, broken-block timers, tank behaviors
  and the start/total/increase tank lists, the bomb-chain tables and every
  raw Clipdata value's collision and behavior, read from the bounded
  `block_data.c` ELF objects. Element counts come from the object sizes;
  the `makesolid` / `tankclip` Clipdata values and `DIFF_NORMAL` are named
  constants of the decompilation, and `MESSAGE_NONE` (0xFF) prints as -1
  like its enum value.
- Added `fusion_mzm_serialize_power_bomb_windows` for `power_bomb.tsv`: the
  s16 window values of every `sHaze_PowerBomb_WindowValuesPointers` entry.
- The new `--mzm-runtime-tables` command and a real-ROM equivalence test
  match `block_table_lines` (4,643 bytes) and `power_bomb_window_lines`
  (201,326 bytes) byte for byte, with synthetic coverage of the bit fields,
  the signed message and offsets, the difficulty rows and malformed sizes.
- Every private file of the Zero Mission room export except the export
  index now has a byte-identical native serializer: `room.tsv`,
  `background.bmp`, `bg0.bmp` / `bg3.bmp`, the tileset sheets, `blocks.tsv`
  and `power_bomb.tsv`. Production authority and gameplay status do not
  change; the native writer, the export index and the rebuild cutover come
  next.

## Patch 0517 - Native Zero Mission runtime bitmaps

- Added `fusion_mzm_render_metatile_sheet` (every block value of a tileset,
  32 a row, the common blocks from block 1024, through the existing tile
  renderer) and `fusion_mzm_serialize_bmp32` (the top-down BITMAPV4HEADER
  32-bit BMP with an alpha mask of `scripts/mzm_bg_layers.py::bmp32`).
- The new `--mzm-bitmaps` command and real-ROM equivalence test match the
  room export's `tilesets/tileset_NN.bmp` sheets and `bg0.bmp` / `bg3.bmp`
  text layers byte for byte: all 79 sheets and the 331 text layers of the
  rooms with an RLE BG1, 255,148,900 bytes. Synthetic coverage checks the
  header, BGRA order, alpha, the size bound and sheet placement.
- Measured before the writer: the export's `background.bmp`, which the
  runtime loads as the room texture, comes from the older
  `render_layer` / `compose_partial_bg12` path with a guessed graphics base,
  not from the RoomLoadTileset VRAM selection the native code reproduces
  (`select_bg12`). The two differ in 289 of the 330 exportable rooms (0.4 to
  4.6 % of the pixels in the first rooms checked), so the native writer cannot
  replace that file byte for byte yet; porting the legacy path or switching
  the export to the VRAM selection is an open decision with a visible runtime
  effect.
- Production authority and gameplay status do not change.

## Patch 0516 - Native Zero Mission room sprite serialization

- Completed the cumulative C11 room serializer with the sprite records: `P`
  default placements, `S` default spriteset members, `F` event-variant
  headers, `J` event-variant placements and `N` event-variant members,
  emitted between the hatches and the `U` music record exactly as
  `scripts/mzm_runtime_room.py::encode_room` orders them.
- `fusion_mzm_read_spritesets` reads `sSpritesetPointers` and each
  spriteset array from bounded ELF symbols, stopping at `SPRITESET_EMPTY` or
  the array end; `fusion_mzm_read_room_sprites` reads the room entry's
  default and event sprite data up to `ROOM_SPRITE_DATA_TERMINATOR`, drops
  event variants whose event is 0, clamps spriteset indices to the table end
  and gives no variant for the test areas' `UCHAR_MAX` spritesets, matching
  the Python reference's behavior there. Placement slots above 16 resolve to
  spriteset members (missing members skipped but still numbered), others to
  PrimarySprite ids; names come from a C table of the decompilation's
  PrimarySprite enumerators (`src/assets/mzm/primary_sprites.c`), since the
  reference ELF carries no debug information.
- Added synthetic coverage of member/direct slots, the 0x80 slot bit, a
  missing member, graphics-slot masking, a terminator hiding later data, an
  array ending a spriteset, a zero-size label sharing an array address,
  spriteset clamping, both event variants, an event variant without
  placements, a dropped event-0 variant, the `UCHAR_MAX` rule and missing or
  malformed symbols. The new `--mzm-runtime-room-sprites` command and a new
  real-ROM equivalence test compare the complete payload with `produce()`'s
  encoding byte for byte.
- All 330 exportable rooms match across 5,548,796 bytes with 1,160 `P`,
  1,518 `S`, 118 `F`, 785 `J` and 608 `N` records plus every earlier record;
  the reference set covers rooms without sprites, with default sprites only,
  with one and with two event variants, several graphics slots and the 16
  test-area rooms without sprite records. The room without a supported BG1
  stays skipped by both implementations.
- The native serializer now covers the complete room TSV payload. Private
  output writes and the rebuild task remain Python-owned, so production
  authority and gameplay status do not change.
- Next: an atomic native writer for the complete private room outputs
  (room TSV, background BMP and masks), wired into the `metroid_rooms` task of
  `scripts/rebuild.py`, validated for runtime/editor consumption and
  incremental rebuilds before the Python production path is removed.

## Patch 0515 - Native Zero Mission room metadata serialization

- Extended the cumulative C11 room serializer with the room entry's `U` music,
  `L` layout/effect, `E` effect-height, `G/R` block-layer and `T` text-layer
  records. This reads the already bounded native room/background structures;
  it does not modify or migrate the audio runtime.
- Preserved symbolic room-effect names, common-metatile sheet remapping,
  invalid-block fallback, layer/row order, text-background size semantics and
  the production exporter's exact BG1 eligibility rule.
- Added synthetic music, layout, effect, BG1/BG2 row, common-block, text-layer,
  ordering and invalid-effect coverage. The new
  `--mzm-runtime-room-metadata` command compares cumulative payloads with the
  Python reference byte for byte.
- All 330 exportable rooms match across 5,450,692 bytes: 330 `U` music
  records, 784 `G` block layers, 22,426 `R` rows and 331 `T` text layers, plus
  one `L` and `E` record per room and every previously validated room record.
  The one room without a supported BG1 remains explicitly skipped by both
  implementations.
- Sprite placements and event variants plus private-output writes remain
  Python-owned, so production authority and gameplay status do not change.
- Next: port the `P/S/F/J/N` sprite records. That is the final payload block
  before adding atomic native writes, switching the rebuild task to C and
  removing the covered Python production path.

## Patch 0514 - Native Zero Mission hatch serialization

- Added bounded C11 decoding of the native hatch type, animation-duration,
  tilemap-value and weakness/health tables plus all 19 hatch-lock event
  records from their verified ROM/ELF objects.
- Reproduced `ConnectionLoadDoors` hatch derivation from each room's door and
  Clipdata records, including facing detection, behavior-to-type mapping,
  source-door identity and the native 16-slot capacity.
- Extended `MVROOM-NATIVE` serialization with ordered `H`, `V`, `A`, `W`, `K`
  and `Q` records. Event masks remain hatch-slot based and are resolved back
  to global door indices without confusing the two index spaces.
- Added synthetic facing, type mapping, weakness, animation, collision-table,
  event timing/class, ordering, ownership and malformed-table coverage. The
  new `--mzm-runtime-room-hatches` command compares every cumulative payload
  with Python byte for byte.
- All 331 payloads match across 3,085,641 bytes. They contain 506 `H` hatch
  records and 22 `Q` lock conditions in addition to the previously validated
  224,307 `C`, 2,707 `B`, 1,141 `M`, 761 `D`, 761 `X` and 77 `Y` records;
  every room also carries its exact `V/A/W/K` runtime tables.
- Sprite placements/variants, music, background records and private writes
  remain Python-owned, so gameplay status and production authority do not
  change. Audio was not modified.
- Next: serialize room-entry music and background records, then port sprite
  variants. Once those complete the payload, add native private writes, switch
  the rebuild task to C and delete only the Python paths proven redundant.

## Patch 0513 - Native Zero Mission room-door serialization

- Added bounded C11 decoding of the eight production door tables and the
  cross-area connection table directly from verified ROM/ELF objects. Door
  kinds, source rectangles, destination indices and signed entry offsets retain
  their native representation; the unused Test1/Test2/Test3 table remains
  outside the production room mapping, matching the Python exporter.
- Extended the native `MVROOM-NATIVE` v1 serializer with ordered `D` door,
  `X` entry-offset and location-banner `Y` records. Same-area targets resolve
  through their local door table, cross-area targets through
  `sAreaConnections`, and none or unresolved targets keep the schema's
  explicit zero destination.
- Added synthetic same-area, cross-area, location-flag, signed-offset,
  terminal-none, ownership and malformed-table coverage. The new
  `--mzm-runtime-room-doors` command retains the core-only command and compares
  each door-inclusive payload with Python byte for byte.
- All 331 room payloads match: 345,686 cells produce 224,307 `C`, 2,707 `B`,
  1,141 `M`, 761 `D`, 761 `X` and 77 `Y` records across 2,879,138 serialized
  bytes.
- Hatches, sprite variants, music and background records remain Python-owned;
  private writes and complete room serialization therefore remain pending,
  with no gameplay-status change. Audio was not modified.
- Next: decode native hatch type/behavior/animation tables and derive each
  room's `H` records while preserving the current door serialization.

## Patch 0512 - Native Zero Mission runtime-room core serialization

- Added bounded C11 loading of the five native Clipdata lookup objects from
  the verified ROM through the pinned ELF: main/tilemap collision types,
  main/tilemap behaviors and movement values. No generated table is tracked.
- Added serialization of the `MVROOM-NATIVE` v1 header and the complete
  Clipdata-derived core in native row order: non-air `C` records, door and
  vertical-transition `B` records, movement-affecting `M` records, then `END`.
- Preserved the native cleared-RAM behavior for raw values between
  `CLIPDATA_COUNT` and `CLIPDATA_TILEMAP_FLAG`, rejected values beyond the
  tilemap table, and derived pixel dimensions from the decoded 16x16 cells.
- Added synthetic table bounds, main/tilemap lookup, transition, movement,
  ownership, invalid-area and out-of-range-value coverage. The new
  `--mzm-runtime-room-cores` command compares every complete core payload.
- All 331 room cores match Python byte for byte: 345,686 cells produce 224,307
  `C` records, 2,707 `B` records, 1,141 `M` records and 2,837,039 serialized
  bytes.
- Doors, exits, hatches, sprite variants, music and background records remain
  outside this core payload. Private writes and complete room serialization
  therefore remain Python-owned, with no gameplay-status change.
- Next: read and serialize native Zero Mission door records, including `D`,
  `X` and location-banner `Y` lines, without touching audio.

## Patch 0511 - Native Zero Mission background serialization

- Added bounded C11 selection of the validated Zero Mission BG1-over-BG2
  diagnostic composite with an explicit BG1-only fallback when BG2 is absent
  or has incompatible dimensions.
- Added exact private-output serialization: a bottom-up 24-bit BMP with BGR
  channels and aligned rows, plus an owned raw one-byte-per-pixel visibility
  mask that preserves native black separately from unresolved pixels.
- Added synthetic BG1-only selection, BMP header/channel/padding, mask-copy,
  ownership and malformed-mask coverage. `--mzm-background-composites` now
  compares the complete RGB, BMP and visibility bytes for every selected room.
- All 330 rooms with a supported BG1 match Python byte for byte: 301 select
  BG1 over BG2 and 29 use BG1 alone. Their 88,427,520 pixels produce
  265,300,380 BMP bytes and 88,427,520 mask bytes; 55,361,565 pixels come from
  BG1, 3,646,119 from BG2 and 29,419,836 remain unresolved. One room has no
  supported BG1, and no decoded BG2 required a dimension fallback.
- This remains a static diagnostic background, not hardware composition.
  Animated replacements, the Mother Ship common-tile variant, private file
  writes and runtime-room serialization remain Python-owned, so gameplay and
  migration statuses do not change.
- Next: begin native Zero Mission runtime-room package serialization with the
  room header and Clipdata rows, then add doors, hatches, background records
  and sprites without touching audio.

## Patch 0510 - Native Zero Mission BG1/BG2 composites

- Added bounded C11 composition of the already validated Zero Mission BG1 and
  BG2 RGBA layers when both are decoded and have identical dimensions.
- Preserved the existing diagnostic contract: known opaque BG1 pixels are in
  front of known BG2 pixels, pixels absent from both layers remain black, and
  a separate byte mask distinguishes visible native black from unresolved
  output. This does not claim GBA priority, window or blending parity.
- Added synthetic foreground, background, unresolved, visibility, ownership
  and dimension-mismatch coverage plus complete RGB/mask SHA-1 comparison
  through `--mzm-background-composites`.
- All 301 eligible room composites match Python across 83,362,816 pixels:
  51,786,183 are selected from BG1, 3,646,119 from BG2 and 27,930,514 remain
  unresolved. Twenty-nine rooms have BG1 but no BG2, one has neither layer,
  and no decoded pair has mismatched dimensions.
- BG0/BG3, hardware priority, scrolling, blending, animated replacements,
  bitmap output and runtime-room serialization remain outside this contract,
  so migration and gameplay status do not change.
- Next: serialize the native Zero Mission background selection to the private
  bitmap/mask format, including the 29 valid BG1-only rooms, before porting the
  complete room package; audio remains untouched.

## Patch 0509 - Native Aria runtime-room serialization

- Added bounded C11 serialization of the `AOSROOM-NATIVE` v4 TSV consumed by
  the Aria runtime, using the already validated final BG1 collision cells,
  transition targets, entity records and global-map room flags.
- Preserved lowercase two-digit collision rows, signed transition X
  adjustments, source-screen coordinates, target room identities, entity
  ordering and the final `END` record.
- Added synthetic schema, collision-row, signed-transition, entity, flag,
  ownership and invalid-input coverage plus complete byte-level SHA-1
  comparison through `--aria-runtime-rooms`.
- All 342 exportable rooms match Python byte for byte: 956,416 collision cells,
  725 transitions and 2,335 entities serialize to 2,035,730 bytes. The output
  carries 17 save-room flags and 8 warp-room flags. Area 11 room 40 remains
  explicitly skipped because its BG1 is absent in both implementations.
- Private directory/index writes and global task orchestration remain
  Python-owned, so technical migration remains partial and gameplay status
  does not change.
- Next: alternate to Zero Mission and compose its validated native background
  layers before tackling its runtime-room serialization; audio remains
  untouched.

## Patch 0508 - Native Aria composite bitmap serialization

- Added bounded C11 serialization of native Aria RGB room composites to the
  exact 24-bit BMP representation consumed by the runtime and editor.
- Preserved the 54-byte little-endian BMP/DIB headers, aligned row pitch,
  bottom-up row order, RGB-to-BGR conversion and zero-filled row padding.
- Added synthetic header, channel-order, padding, ownership and invalid-input
  coverage. `--aria-background-composites` now reports both the RGB and BMP
  byte digests for each supported room.
- All 342 supported composites produce byte-identical Python and C BMP files:
  61,210,624 pixels serialize to 183,650,340 bytes. Area 11 room 40 remains
  explicitly skipped because neither path finds a supported text layer.
- `AOSROOM-NATIVE` TSV serialization, private output orchestration, parallax,
  affine/8-bpp layers, blending and animation remain outside this contract, so
  migration and gameplay status do not change.
- Next: serialize every supported `AOSROOM-NATIVE` v4 room payload in C and
  compare it byte for byte with Python; audio remains untouched.

## Patch 0507 - Native Zero Mission room-layer rendering

- Added bounded C11 RGBA rendering for both Zero Mission room-background
  paths: 16x16 RLE block layers through the validated tileset/common metatile
  tables and LZ77 text tilemaps through character base 2.
- Preserved block-to-common-table selection, invalid-block fallback, four-cell
  metatile placement, screen-block ordering, 4-bpp palette banks, horizontal
  and vertical tile flips, transparent color index zero and exact GBA
  BGR555-to-RGB conversion.
- Added synthetic tileset/common block colors, ownership and text-layer
  coverage. The Python reference now assembles block layers from cached
  tileset sheets so complete SHA-1 equivalence remains practical.
- `--mzm-background-renders` verifies all 1,115 present layers: 784 RLE and
  331 text layers totaling 253,914,112 pixels, with zero unresolved tile or
  palette references. The remaining 209 layer slots are explicitly absent.
- Animated tile/palette replacement, the Mother Ship common-tile variant,
  inter-layer composition, effects and runtime-room serialization remain
  Python-owned, so migration and gameplay status do not change.
- Next: alternate to Aria and serialize its validated native composites and
  runtime-room payloads in C; audio remains untouched.

## Patch 0506 - Native Zero Mission metatile tables

- Added bounded C11 reconstruction of the 79 Zero Mission tileset metatile
  tables from their ROM pointers and exact pinned-ELF object sizes.
- Preserved the loader's effective layout without reading past source objects:
  available words after each two-byte resource header are copied into 1,024
  four-cell tileset blocks, the remainder is zero-filled, and the 208 common
  four-cell blocks are appended from `sCommonTilemap`.
- Added synthetic header, short-table, zero-fill, common-table, pointer and
  malformed-size coverage plus complete little-endian SHA-1 comparison through
  `--mzm-metatiles`.
- All 79 tables match Python: 157,198 source tileset words and 65,728 repeated
  common words produce 97,328 complete block records. Room-layer pixel
  rendering, composition, animated replacements and the Mother Ship common
  variant remain outside this contract, so migration and gameplay status do
  not change.
- Next: render Zero Mission RLE metatile and LZ77 text room layers natively
  from the validated tables and video-memory snapshots; audio remains
  untouched.

## Patch 0505 - Native Aria background depth composites

- Added bounded C11 composition of the already validated Aria RGBA layers,
  using BG1 dimensions when available, the first supported layer otherwise,
  native `field_0` depth ordering, transparent-pixel rejection and modulo
  repetition for differently sized layers.
- Added synthetic depth, transparency, repeated-source, BG1-primary,
  fallback-primary, ownership and empty-input coverage plus complete RGB SHA-1
  comparison through `--aria-background-composites`.
- All 342 supported room composites match Python across 61,210,624 output
  pixels and all 901 rendered source layers. Area 11 room 40 has no supported
  text layer in either implementation and is reported explicitly as skipped.
- This remains a static-origin diagnostic composite: parallax, affine/8-bpp
  layers, hardware priority/blending, animation, entities, BMP output and
  `AOSROOM-NATIVE` serialization remain outside the native contract. Migration
  status therefore stays partial and gameplay parity does not change.
- Next: alternate to Zero Mission and expand its tileset and common metatile
  tables natively before rendering room layers; audio remains untouched.

## Patch 0504 - Native Zero Mission tileset video memory

- Added bounded C11 decoding of all 79 `sTilesetEntries` records directly from
  the verified ROM and pinned ELF symbol table.
- Reproduced the standard `RoomLoadTileset` memory layout: common graphics at
  VRAM `0x4800`, LZ77 tileset graphics at `0x5800`, LZ77 background graphics
  ending at `0xFDE0`, the final 32-byte clear at `0xFFE0`, common palette
  colors 1-47 and tileset palette rows 3-15.
- Added synthetic pointer, table, LZ77, placement, palette and malformed-input
  coverage plus `--mzm-tilesets` complete SHA-1 comparison against the Python
  reference's raw video-memory snapshots.
- All 79 64-KiB VRAM and 512-byte palette snapshots match exactly. They cover
  1,229,216 bytes of expanded tileset graphics and 789,728 bytes of expanded
  background graphics. Animated updates, the Mother Ship common-tile variant,
  metatile expansion, compositing and room serialization remain Python-owned,
  so migration status stays partial and gameplay parity does not change.
- Next: alternate to Aria and reproduce native room background depth
  compositing from the already validated layer renders; audio remains
  untouched.

## Patch 0503 - Native Aria 4-bpp background rendering

- Added bounded C11 rendering of decoded Aria text backgrounds from the native
  tilemaps, 64 KiB VRAM snapshot and 512-byte palette snapshot.
- Preserved character-base selection, tile index and palette bank lookup,
  per-tile horizontal/vertical flips, transparent color index zero and exact
  GBA BGR555-to-RGB conversion; unsupported 8-bpp layers and invalid output
  dimensions fail explicitly.
- Added a synthetic flipped-tile/palette fixture, transparent output ownership
  and 8-bpp rejection, then compared complete RGBA SHA-1 digests with Python
  through `--aria-background-renders`.
- All 901 decoded layers produce identical output across 150,929,408 pixels,
  with zero unresolved tile or palette references; 128 absent/affine layers
  remain intentionally skipped. Depth compositing, BMP serialization and
  `AOSROOM-NATIVE` serialization remain Python-owned, so migration status stays
  partial and gameplay parity does not change.
- Next: alternate to Zero Mission and load its tileset graphics and palettes
  into bounded native VRAM/palette snapshots; audio remains untouched.

## Patch 0502 - Native Zero Mission BG3 tilemaps

- Extended the native room-background contract from BG0/BG1/BG2 to all four
  layers and connected each room entry's BG3 property and resource pointer.
- BG3 now uses the existing bounded BIOS-LZ77 text-tilemap decoder, including
  the `BG_PROP_STARTS_FROM_BOTTOM` property combination and its preserved size
  flag, dimensions and final 16-bit cells.
- Extended the synthetic ownership/property fixture and the complete Python
  comparison to all four layer slots through `--mzm-backgrounds`.
- All 317 present BG3 tilemaps match Python across 576,512 cells; the other 14
  BG3 layers are absent. Across BG0-BG3, the contract now covers 784 RLE maps,
  331 text tilemaps and 209 absent layers, totaling 1,443,436 cells with no
  unsupported property. Tileset graphics/palettes, compositing and room
  serialization remain Python-owned, so migration status stays partial and
  gameplay parity does not change.
- Next: alternate to Aria and port bounded 4-bpp pixel rendering from native
  tilemaps, VRAM and palettes; audio remains untouched.

## Patch 0501 - Native Aria background tile expansion

- Added bounded C11 expansion of Aria text-background block maps into their
  final 8x8 tile entries, including direct or LZ77 block tables, empty block
  zero, horizontal/vertical block flips and the corresponding tile flip-bit
  toggles.
- Exposed absent and affine layers explicitly and retained the native metadata
  pointers, dimensions, flags, maximum block and final tile dimensions for
  every layer.
- Added synthetic flip, ownership, absent, affine, dimension and truncated
  block-table coverage, then compared complete little-endian tilemap SHA-1
  digests with Python through `--aria-background-tiles`.
- Across all 1,029 layer slots in 343 rooms, 901 text backgrounds now expand
  to 2,358,272 identical tile entries; 104 layers are absent and 24 are affine.
  Pixel rendering, depth compositing and `AOSROOM-NATIVE` serialization remain
  Python-owned, so migration status stays partial and gameplay parity does not
  change.
- Next: alternate to Zero Mission and connect its BG3 LZ77 tilemaps to the
  native room-background contract; audio remains untouched.

## Patch 0500 - Native Zero Mission room backgrounds

- Added a native BG0/BG1/BG2 preparation contract that classifies each room
  layer from its property bits and decodes both custom two-pass RLE block maps
  and BIOS LZ77 text tilemaps.
- Text backgrounds preserve the native size flag and expose bounded 32x32,
  64x32 or 32x64 final 16-bit tile cells; malformed pointers, dimensions,
  expanded sizes and compressed streams fail without retaining partial data.
- Added synthetic ownership, classification, direct RLE, LZ77 size and payload
  coverage, then compared every final map digest and dimension with Python
  through `--mzm-backgrounds`.
- Across all 331 rooms, the native reader now covers 784 RLE layers and 14
  LZ77 BG0 layers, totaling 866,924 cells; 195 layers are absent and none use
  an unsupported property. BG3, tileset graphics/palettes, compositing and
  runtime-room serialization remain Python-owned, so migration status stays
  partial and gameplay parity does not change.
- Next: alternate to Aria and port static text-background tile expansion from
  its native VRAM and room metadata; audio remains untouched.

## Patch 0499 - Native Aria graphics and palette loading

- Added bounded C11 execution of Aria's native graphics and palette load lists,
  including direct resources, BIOS LZ77 resources, the four-byte expanded-data
  prefix and the palette header's inclusive final-row field.
- Each room now produces complete 64 KiB VRAM and 512-byte palette snapshots;
  source ranges, destination blocks, palette rows, resource pointers,
  encodings, expanded sizes and compressed streams are rejected on overflow or
  truncation.
- Added synthetic direct/LZ77 resource, copy-placement, encoding and bounds
  coverage, then compared SHA-1 digests of both complete output buffers with
  Python for all 343 rooms through `--aria-video-memory`.
- The verified rooms apply 887 graphics loads and 736 palette loads. Static
  background compositing and `AOSROOM-NATIVE` serialization remain
  Python-owned, so the migration stays partial and gameplay parity does not
  change.
- Next: alternate to Zero Mission and decode its remaining BG0/BG1/BG2 room
  background payloads from the native descriptors; audio remains untouched.

## Patch 0498 - Native Zero Mission Clipdata payloads

- Added a bounded C11 decoder for Zero Mission's custom two-pass room RLE,
  including one- and two-byte big-endian counts, repeated and literal spans,
  independent low/high-byte passes and room-specific trailing metadata.
- Connected every native `RoomEntryRom::pClipData` pointer to that decoder and
  exposed dimensions, consumed input size and final 16-bit Clipdata cells
  through `--mzm-clipdata`.
- Added synthetic dimension, pass-width, overflow, truncation and ownership
  coverage, then compared dimensions and a canonical little-endian SHA-1 of
  every decoded map with Python.
- All 331 descriptors decode to 345,686 identical Clipdata cells. Background
  payloads, doors, sprites and runtime-room serialization remain Python-owned,
  so the migration stays partial and gameplay parity does not change.
- Next: alternate to Aria and port graphics/palette resource loading into its
  native runtime-room preparation; audio remains untouched.

## Patch 0497 - Native Aria BG1 collision maps

- Added bounded C11 decoding for Aria text-background metadata, compressed
  block and collision payloads, 4x4 block maps and the final per-cell BG1
  collision bytes consumed by the runtime.
- Preserved horizontal and vertical block flips and the native slope-direction
  toggle; fixed the Python reference so a vertical flip starts from the source
  row for every cell instead of alternately toggling the row within a block.
- Added synthetic bounds, absent/affine/no-collision status, flip and slope
  coverage, then compared dimensions, pointers, maximum block and a SHA-1 of
  every final collision map for all 343 rooms.
- The verified ROM contains 342 decoded BG1 collision maps totaling 956,416
  cells and one room without BG1. Rendering, graphics, palettes and runtime
  room serialization remain Python-owned, so the migration stays partial.
- Next: alternate to Zero Mission and decode its bounded Clipdata room payload
  from the newly native room descriptors; audio remains untouched.

## Patch 0496 - Native Zero Mission room descriptors

- Added a bounded C11 reader for the 60-byte `RoomEntryRom` structure and all
  eleven named room tables in the pinned Zero Mission ELF, including the four
  test-area tables.
- The native contract exposes every layer, Clipdata and spriteset pointer plus
  the room properties, map position, effect and music fields for all 331
  source descriptors.
- Added malformed table-size, index, ROM-bound and resource-pointer coverage,
  then compared every native field and table address with the pinned
  decompilation declarations and ELF symbols through `--mzm-rooms`.
- This is structural metadata only: room resources, doors, collision payloads,
  sprites and rendered output remain Python-owned, so the room migration stays
  partial and no gameplay parity status changes.
- Next: alternate to Aria and begin the native runtime-room payload contract,
  starting with its bounded collision metadata; audio remains untouched.

## Patch 0495 - Native Aria world-catalog serialization

- Added deterministic C11 serialization for the complete `MV_AOS_WORLD_2`
  catalog: rooms, variants, backgrounds, load lists, entities, resolved
  transitions, map cells, boss annotations, savepoints and warps.
- Added native generation of the derived verified `rooms.tsv` browser index,
  including primary dimensions, entity/transition counts and save/boss flags.
- Factored atomic output replacement into the shared native asset core and kept
  the existing Zero Mission ROM-bank writer on that common implementation.
- Compared the complete native JSON structure with `decode_world`, compared the
  room index byte for byte, and verified deterministic rewrites in an isolated
  directory through `fusion_importer --aria-catalog`.
- The Aria world extractor is now native; registration in the global `--assets`
  task graph and resource-payload extraction remain open.
- Next: alternate to Zero Mission and begin its native room-metadata contract
  from `mzm_runtime_room.py`; audio remains untouched.

## Patch 0494 - Native Zero Mission ROM-bank serialization

- Added deterministic C11 serialization of the private `MZMROM01` bank and the
  `metroidvania-mzm-rom-v1` symbol manifest.
- Both outputs use sibling temporary files followed by atomic replacement; the
  caller supplies an existing private output directory, keeping path ownership
  separate from serialization.
- Added synthetic byte-level coverage for the binary header, ROM payload and
  manifest, then compared both complete native files byte for byte with
  `mzm_rom_bank.py` in isolated temporary roots.
- `fusion_importer --mzm-rom-bank ROM ELF OUTPUT_DIR` now reproduces this Python
  extractor, but the global native `--assets` task graph is not connected yet.
- Next: alternate back to Aria and add deterministic native world-catalog
  serialization from its already migrated metadata; audio remains untouched.

## Patch 0493 - Native Zero Mission ROM symbols

- Started the Zero Mission side of the C11 import migration with a bounded
  little-endian ELF32 symbol-table reader.
- Preserved the Python contract for sized `STT_OBJECT` records: ROM-range
  filtering, address/name ordering, first-definition-wins deduplication and
  rejection of objects that extend past the ROM.
- Added a synthetic malformed-ELF and ROM-bounds suite, then compared every
  emitted symbol line and the duplicate-name count against `mzm_rom_bank.py`
  through `fusion_importer --mzm-symbols` on the verified local inputs.
- This slice does not yet write `MZMROM01` or `rom_symbols.tsv`; the Python bank
  producer remains authoritative and audio remains outside the active scope.
- Next: add deterministic native serialization for the Zero Mission ROM bank
  and symbol manifest, with byte-for-byte comparison in an isolated output.

## Patch 0492 - Native Aria world summary

- Added a bounded C11 world-summary reader that composes the migrated Aria
  directory, map, room, transition, entity and audited boss readers.
- The native summary now reports the room, mapped-cell, transition, entity,
  boss-room, savepoint and warp totals used by the Python world catalog.
- Expanded the synthetic world fixture to cover every audited boss location and
  added exact verified-ROM comparison through `fusion_importer --aria-summary`.
- Native catalog serialization and resource payload extraction remain open;
  Python therefore remains the production serializer.
- Next: begin the Zero Mission side of the native migration with the bounded
  `mzm_rom_bank` symbol-table contract; audio remains outside the active scope.

## Patch 0491 - Native Aria boss-room metadata

- Ported the audited Aria boss-room identity table to C11, preserving the
  distinct first and second Chaos phase records.
- The native reader resolves each identity through the room directory, requires
  exactly one matching enemy placement and reads its health from the bounded
  enemy table entry.
- Added synthetic success, missing-entity and ambiguous-entity coverage, then
  compared every audited boss record against the Python reference through
  `fusion_importer --aria-bosses` on the verified local ROM.
- This slice does not serialize the native world catalog or extract resource
  payloads, and no ROM-derived data is tracked.
- Next: assemble the native Aria world-summary and catalog contract while the
  Python serializer remains authoritative.

## Patch 0490 - Native Aria room entities

- Ported Aria's 12-byte room entity entries to C11: signed position,
  persistent index, kind, entity ID, flags and both parameters.
- Preserved null-list semantics and the native `x == 0x7FFF` terminator with
  ROM bounds and a 512-entry safety cap.
- Added synthetic negative-coordinate, empty-list and unterminated-list coverage,
  then compared every room placement against Python through
  `fusion_importer --aria-entities`.
- The structural room inputs are now decoded natively, but boss annotations,
  catalog serialization and resource payload decoding remain Python-owned.
- Next: port the audited boss-room identities and enemy-table health metadata,
  then assemble a native world-summary contract.

## Patch 0489 - Native Aria resource loading lists

- Ported Aria's zero-terminated eight-byte graphics and palette loading-list
  entries to C11, preserving each resource pointer and its three parameters.
- Added ROM bounds, resource-pointer validation, mandatory terminators and the
  native 64-entry safety cap.
- Added synthetic graphics/palette lists plus an unterminated-list failure and
  compared every room list against Python through `fusion_importer --aria-loads`.
- Resource payloads are not read, decompressed or written by this slice; entity
  placements and resource decoding remain Python-authoritative.
- Next: port the bounded room entity lists and their terminator semantics.

## Patch 0488 - Native Aria background descriptors

- Ported each room's three 12-byte background descriptors to C11, including
  layer identity, native control fields and metadata pointers.
- Added bounded optional width/height metadata reads while preserving the
  distinction between a null metadata pointer and zero-valued dimensions.
- Added a synthetic mixed metadata/no-metadata fixture and compared every room
  background header and dimension against Python through
  `fusion_importer --aria-backgrounds`.
- Tile graphics, palettes, block maps and collision payloads remain untouched
  and Python-authoritative; no ROM-derived output is tracked.
- Next: port the zero-terminated graphics and palette loading-list entries.

## Patch 0487 - Native Aria room transitions

- Ported Aria's 16-byte room transition entries to C11, including signed source
  screen coordinates, load coordinates and the five native control fields.
- Resolved every transition target pointer to one unique native area/room pair;
  unknown and ambiguous room pointers are rejected.
- Added synthetic coverage for signed coordinates, field decoding, two resolved
  destinations and an unknown target, then compared every local-ROM transition
  and destination against the Python reference.
- Background descriptors, graphics/palette loading lists and entity placements
  remain Python-authoritative; no transition output is tracked in the repository.
- Next: port the three background descriptors and their bounded dimension
  metadata, without decoding tile graphics yet.

## Patch 0486 - Native Aria room descriptor headers

- Ported Aria's bounded room descriptor headers and up-to-16-entry variant
  chains to C11, including cycle detection and the original terminal selector.
- Exposed display/blend control, linked resource pointers, map coordinates,
  coordinate flags and transition-table bounds through `FusionAosRoomDescriptor`.
- Added synthetic coverage for a resolved two-stage chain, coordinate flags,
  transition counts and a rejected cycle, then compared every room header and
  variant chain against Python through `fusion_importer --aria-rooms`.
- Transition entries, backgrounds, loading lists and entities are still
  Python-authoritative; this patch does not copy or emit their payloads.
- Next: port transition entries and resolve their target room identities using
  the native area directory.

## Patch 0485 - Native Aria global map metadata

- Ported Aria's 64x35 global map table to C11, including occupied coordinates,
  area/room identities, room descriptor pointers, save flags and warp flags.
- Added strict map bounds, area/room range, room-table pointer and duplicate
  save-room validation to the native reader.
- Refactored the focused Python decoder and compared every native cell and
  summary count against it through `fusion_importer --aria-map` on the verified
  local ROM; the synthetic CTest also covers invalid rooms and duplicate saves.
- No map output or ROM-derived bytes are tracked. Room descriptors, backgrounds,
  entities and transitions remain Python-authoritative.
- Next: port the bounded Aria room descriptor and variant-chain header, without
  decoding graphics or entity payloads yet.

## Patch 0484 - Native Aria area directory

- Added the first game-specific C11 metadata reader: Aria's 12-entry area
  directory and the bounded room-table count derived for each area.
- Added strict game, ROM-size, GBA-pointer ordering, table-end and alignment
  validation plus a fully synthetic ROM-free CTest.
- Refactored the Python reference to expose the same focused operation and
  compared all native counts against the verified local Aria ROM through
  `fusion_importer --aria-directory`.
- No extracted counts or ROM bytes are tracked; full room descriptors,
  entities, transitions and rendered resources remain Python-authoritative.
- Next: port Aria's map-cell table so the native layer can associate area/room
  identifiers with map coordinates, save rooms and warp rooms.

## Patch 0483 - Connect verified ROMs to GBA metadata

- Added `fusion_rom_read_gba_header`, which exposes the bounded GBA header
  reader through an already SHA-1-verified `FusionRom` buffer.
- `fusion_importer --verify` now reports each verified ROM's native title, game
  code, software version and header checksum state.
- Extended the ROM-free CTest to cover the verified-buffer wrapper and checked
  both installed local USA ROMs without committing them.
- This remains identity/diagnostic metadata; Python still owns all extraction
  and no room, sprite, audio or gameplay parity status changed.
- Next: use the verified game identity to select one small game-specific native
  metadata table and compare it against a Python fixture.

## Patch 0482 - C11 GBA header metadata reader

- Added a bounded native reader for the standard GBA title, game code, maker
  code, unit/device and software-version fields.
- Added complement-checksum calculation and a fixed-header-marker check while
  preserving the raw metadata bytes in caller-owned, NUL-terminated fields.
- Added a ROM-free synthetic-header CTest and a Python fixture that compares
  the native `fusion_importer --header` output field by field.
- This is an input-contract foundation only; Python remains authoritative for
  extraction and no gameplay or resource-equivalence status changed.
- Next: connect verified ROM identity to this metadata reader, then port one
  small game-specific metadata table before touching room extraction.

## Patch 0481 - C11 GBA binary and compression primitives

- Added a reusable bounds-checked little-endian binary reader to
  `fusion_assets`.
- Added strict-output GBA BIOS 0x10 LZ77 and 0x30 RLE decoders with malformed
  stream, backreference, truncation and destination-bound checks.
- Added ROM-free CTest coverage for literals, overlapping LZ77 copies, RLE,
  endianness and invalid input.
- These primitives are not connected to a production extractor yet; Python
  remains authoritative and no resource-equivalence claim is made.
- Next: port one small metadata reader using the binary reader and compare its
  output structurally against the existing Python extractor.

## Patch 0480 - C11 asset pipeline foundation

- Added the `fusion_assets` C11 library with bounded ROM loading, exact 8 MiB
  size checks, SHA-1 verification for the pinned USA Zero Mission and Aria
  revisions, clear errors and zeroing on release.
- Added `fusion_importer`: `--list`, `--verify`, and an explicit `--dry-run`
  task-plan interface. `--assets` and `--all` report the current boundary
  instead of pretending that extraction has already migrated.
- Added the first migration matrix in `docs/ASSET_PIPELINE_MIGRATION.md` and
  documented the public contract in `docs/ARCHITECTURE.md`.
- Python extractors, generated caches and ROM-derived resources remain
  unchanged and authoritative; no gameplay parity status was raised.
- Next: add generic bounded GBA binary/decompression primitives, then port one
  small metadata extractor with a Python/C equivalence fixture.

## Patch 0479 - Aria save-room checkpoints

- Added `aos_save_room`, a device-free controller for the native Object1C
  activation rectangle (`x=104..136`, `y=128..183`) and its Up edge.
- Runtime room exports now use schema version 4 with global-map save and warp
  flags. Existing version-3 private exports remain readable and their Object1C
  records still identify save rooms.
- In a detected save room, Up restores Soma's HP and atomically writes the
  selected portable file's room, spawn, HP, movement mask and play time. A
  small procedural marker makes the host checkpoint visible without importing
  ROM-derived graphics.
- Native healing animation, save Yes/No prompt, object art, audio, inventory,
  event/map flags and the original SRAM format remain intentionally partial.
- Next: port the first pause/status inventory state and then extend the save
  record only as its native owners become available.

## Patch 0478 - Aria file selection and runtime launch bridge

- Replaced the New Game and Continue placeholders in the SDL title host with
  three selectable files, empty/occupied summaries and overwrite confirmation.
- Added a checksummed, versioned and ROM-free host save format. New Game
  creates area 0, room 0 state, while Continue loads the selected file's room,
  spawn, HP and currently exposed movement abilities into `fusion_aria_runtime`.
- The title host launches the actual sibling runtime process after selection;
  `--runtime` and `--save-dir` keep this testable with explicit local paths.
- This is a playable host bridge, not a claim that native Aria SRAM or
  `GameModeMainMenuUpdate` is complete. Inventory, equipment, souls, flags,
  play-time updates and writes from native save rooms remain unported.
- Next: connect native save-room detection and interaction to this save model,
  then add the first pause/status inventory state without touching audio.

## Patch 0477 - Aria Bone Pillar and Buer linked components

- Ported Bone Pillar's constructor-time tower assembly: the original record
  moves to the top and emits one linked animation-0 segment per clamped tower
  unit, spaced by the native 22 pixels.
- Ported Buer's constructor-time linked second direct frame. It follows the
  rolling parent while active, without inventing the separate fire collision
  behavior that still belongs to its attack-state reconstruction.
- The Bone Pillar per-segment collision and hit callbacks, and Buer's fire
  collision, bounce sequence, palette callbacks and sounds remain unported.
- Next: continue the ten-enemy batch with Giant Ghost's three native linked
  components, then Winged Skeleton's producer and dive state.

## Patch 0476 - Aria Peeping Eye random target steering

- Ported the target choice following the timed entry: its horizontal point is
  randomly 64 to 112 pixels to either side, while the vertical point is 32 to
  80 pixels above the Eye.
- Added the traced state-3 steering core: both axes damp by 31/32, accelerate
  outside the four-pixel target tolerance, cap at two pixels per frame, and
  choose another point upon arrival or after the native timeout.
- The state-3 visual variant sequence, ceiling probes, damage callbacks,
  sounds and rewards remain unported. This remains a partial controller pass.
- Next: trace the Peeping Eye state-3 visual variant sequence and ceiling
  probe, then trace White Dragon damage and attack transitions.

## Patch 0475 - Aria Peeping Eye timed entry sequence

- Replaced the temporary 30/20/30-frame polar envelope with the ten-stage,
  40-frame entry sequence traced from `sub_08088168`. Its extension levels now
  drive the already-linked polar tendrils exactly through the observed stage
  order and durations.
- The ROM's following random-target selection, state-3 variant branch table,
  ceiling probes, damage callbacks, sounds and rewards remain unported at this
  earlier checkpoint.
- ROM-free enemy tests cover the trigger, early stage transition, final
  20-frame hold and state-2 handoff.
- Next: trace the Peeping Eye random target and state-3 variant selection,
  then trace White Dragon damage and attack transitions.

## Patch 0474 - Aria Peeping Eye attack entry pass

- Ported the traced `sub_08088168` proximity gate: the Peeping Eye begins its
  controller pass when Soma enters the native 180 by 200 pixel window.
- Added the first visible tendril attack envelope: wind-up, full polar
  extension and retraction now drive the already-linked child radii. ROM-free
  tests cover the trigger and the wind-up-to-extension transition.
- The variant table, exact branch timings, ceiling probes, damage callbacks,
  sounds and rewards are still unported. The temporary 30/20/30 frame envelope
  is explicitly not presented as the native attack-state table.
- Next: decode the remaining Peeping Eye branch table and attack callbacks,
  then trace White Dragon damage and attack transitions.

## Patch 0473 - Aria White Dragon central affine slot

- Connected the White Dragon's native shared affine slot to the middle linked
  body record (variant 5): it is rendered at unit scale with its tracked body
  angle, while the other eight records stay regular OAM sprites.
- Added a ROM-free assertion for the affine record and kept the existing
  curved chain, two-batch emission and parent lifetime behavior unchanged.
- This is a narrow graphics/controller slice, not full White Dragon parity;
  the complete scale/matrix choreography, damage callbacks, attack states,
  sounds and rewards remain partial, as do the Peeping Eye attack states.
- Next: trace the Peeping Eye attack state table and finish the White Dragon
  affine damage/attack callbacks before moving to the next linked family.

## Patch 0472 - Aria polar and articulated early-enemy motion

- Connected the Peeping Eye's target-oriented polar segment pass: the parent
  tracks Soma with the native angle convention and each linked segment keeps a
  fixed-point radius while following the parent's phase.
- Connected the White Dragon's articulated body pass: every linked animation-1
  part now follows a curved fixed-point chain and carries its per-part affine
  angle into the Aria renderer. The existing two-batch emission and parent
  lifetime contract are unchanged.
- Added ROM-free assertions for the polar angle and articulated child angle.
  These are motion/controller improvements only; the Peeping Eye attack cycle,
  ceiling probes, White Dragon's complete affine matrix/scale choreography,
  damage callbacks, sounds and rewards remain partial.
- Next: trace the remaining Peeping Eye attack states and White Dragon affine
  scale/damage callbacks, then address the next linked early enemy family.

## Patch 0471 - Aria early-enemy motion controller wiring

- Connected the dedicated early-enemy motion controllers for Peeping Eye,
  White Dragon, Buer, Rock Armor, Giant Ghost and Winged Skeleton from the
  constructor-table update path. The controllers now own their facing,
  bounded movement, phase and homing/hover updates instead of leaving those
  motions inline in the shared dispatcher.
- Preserved the parent-linked Peeping Eye and White Dragon emission and child
  update paths unchanged. This patch only changes the parent motion dispatch;
  it does not claim the Peeping Eye polar attack, White Dragon affine chain,
  probes, attacks, palette callbacks, sounds or rewards.
- The Aria enemy runtime test covers the updated constructor batch and linked
  children; all entries remain `partial` until their complete native routines
  and an in-game capture are available.
- Next: replace the approximate Peeping Eye hover with its native polar attack
  controller and connect White Dragon's affine body motion without changing
  the linked-child spawn contract.

## Patch 0470 - Aria linked Peeping Eye and White Dragon children

- Added parent-linked child roles to the Aria runtime. Peeping Eye now emits
  its two eye overlays and six native static segment frames; the children
  follow the parent lifetime, facing and hover phase.
- White Dragon now emits all nine animation-1 body parts in two batches so the
  runtime's eight-child report remains bounded. The parts follow the head and
  render with the native animation descriptor; the native affine chain is
  still explicitly open.
- Added child lifetime/update coverage to the ROM-free enemy tests and staged
  native-room captures for both identities. The generated private library now
  contains 348 object sequences and 1,344 frames, including Peeping Eye frames
  `0xF` and `0x10`.
- Both parity entries remain `partial`: attacks, polar/affine choreography,
  hit callbacks, fades, sounds and rewards are not promoted by this visual
  child pass.
- Next: port Peeping Eye's polar attack controller and White Dragon's affine
  body motion, then move to Bone Pillar's tower segments.

## Patch 0469 - Aria constructor-table batch: ten early enemies

- Connected the native constructor identities, private sheets, palettes,
  animation tables and frame-box metadata for Skeleton, Peeping Eye, Killer
  Fish, Bone Pillar, Buer, White Dragon, Ghost, Rock Armor, Giant Ghost and
  Winged Skeleton (IDs `0x02`, `0x06`-`0x0B`, `0x0E`, `0x12`-`0x14`). The
  exporter now produces 346 object sequences and the runtime accepts the
  larger native animation/frame ranges required by Skeleton and Rock Armor.
- Added the first observable native loops: floor placement, facing, bounded
  patrol/swim, hover, homing, direct child-frame rendering, parameter clamps,
  hidden producer forms and the documented inert states. These entries remain
  `partial`; articulated children, attacks, palette/fade callbacks, probes,
  sounds and rewards are explicitly not claimed.
- ROM-free tests cover all ten constructors, animation or motion progress,
  producer and inert branches, parameter clamping and descriptor addresses.
  Native-room captures verified the loaded Skeleton, Buer and Giant Ghost
  renders; capture files remain private and ignored.
- Next: port the linked children and attack state machines, starting with the
  Peeping Eye segments and White Dragon body chain, then promote each enemy
  only after an in-game capture proves its complete routine.

## Patch 0468 - Aria Merman and Ripper state machines

- Removed the last provisional `batch_enemy_update` path. Ripper now uses its
  six-way native random jump table, close-player reversal, carried drop,
  landing delay and attack animation, which emits an owner-linked accelerating
  blade with its own collision and falling state.
- Ported Merman's submerged wait, rise and ballistic phases, animation-3
  attack and animation-6 shot. Its parameter-1 record is now the hidden room
  producer, capped at three linked bodies; their removal releases the
  producer's count.
- Connected Merman's 40-frame and Ripper's 50-frame falling deaths. ROM-free
  tests cover both attack children, the producer cap path, Ripper's distinct
  jump constants and both death timers. The exported collision layer has no
  independent room-water height, so Merman's surface crossings use the native
  timing around its record Y; this remains an explicit fidelity limit.
- Recorded Ripper and its emitted blades in area 7 room 55 at frame 180, and
  active Mermen with two horizontal shots in area 7 room 2 at frame 240. Both
  entries now meet the project's functional evidence rule; no connected Aria
  bestiary entry remains partially functional.

## Patch 0467 - Aria Tiny Devil and Imp

- Replaced the shared provisional flight with the two native state-machine
  families. Tiny Devil now uses its randomized rest, approach, animation-4
  orb charge, animation-5/6 transition, aimed launch, native 10x10 collision
  box and 60-frame projectile lifetime.
- Added Imp's persistent linked helper, approach and braking states, visible
  control phase above Soma, alternating forced B/release input, native Soma
  state gate and retreat. The existing forced-input path is shared with the
  wooden doors rather than bypassing the player controller.
- Ported both 50-frame falling deaths and Imp's separate direct-frame `0x15`
  death child. The private sprite export and ROM-free regression coverage now
  include every new child lifecycle.
- Captured Tiny Devil's aimed orb and 26-damage hit in room 0/24 and Imp
  visibly forcing Soma's attack with no user input in room 4/1. Both entries
  now meet the project's functional evidence rule; acceleration rounding,
  affine details, sounds, palette callbacks and rewards remain documented
  refinements.

## Patch 0466 - Aria Harpy and Gorgon families

- Completed the shared Harpy/Siren body state machine: random flight-versus-
  volley selection, target approach and braking, weapon-hit recoil, the
  two-stage five-feather Harpy volley, the Siren's bending three-note fan and
  the four-fragment falling death are now connected.
- Completed the shared Gorgon/Catoblepas path with grounded patrol and wall
  turns, its attack wind-up, all 21 clouds in the native even-frame breath
  window, the cloud sprite transition and intermittent collision, and its
  specialized six-piece death.
- Extended the private object export with Harpy/Siren projectile and death
  frames 8-14 and Gorgon/Catoblepas breath and death frames 17-26. ROM-free
  regression tests cover each shared and distinct behavior.
- Recorded all four identities in native rooms with their attacks visible.
  They now meet the project's functional evidence rule; exact gait and
  acceleration rounding, collision-probe nuances, sounds, palette callbacks
  and reward plumbing remain documented fidelity refinements.

## Patch 0465 - Aria ten-enemy constructor-table batch

- Audited the ROM constructor table and corrected two historical identity
  collisions: Killer Doll now runs at native ID `0x1F` instead of Bomber
  Armor's `0x3A`, and Nemesis at `0x26` instead of Manticore's `0x36`.
- Connected eight additional enemies in one batch: Merman, Siren, Tiny Devil,
  Catoblepas, Imp, Gorgon, Harpy and Ripper. Their private native sheets,
  palette variants, animations and frame boxes now render from real room
  records, with basic locomotion, contact combat, generic death and inert
  states. The registry marks each new state machine partial and lists the
  attacks and specialized callbacks still to port.
- Extended the object exporter to accept the native per-component OBJ palette
  row used by Tiny Devil and Imp. ROM-free tests cover all corrected IDs, all
  eight constructors, movement, the Harpy/Siren and Gorgon/Catoblepas aliases,
  inert states and palette-row rendering.
- Recorded one in-game capture for each of the ten identities. The new batch
  renders and moves in native rooms; Merman, Gorgon, Harpy and Ripper also
  demonstrate native-stat contact damage in the captures.

## Patch 0464 - Aria Nemesis temporary OAM history

- Reconstructed the zero-filled 24-record scratch table allocated for each
  Nemesis technical child by `sub_080458E4`.
- Each callback now shifts the owner's integer position, sprite-frame id and
  mirror backwards, then selects the native delayed records 9, 12 and 6. The
  first callbacks retain the native zero-filled history instead of drawing the
  owner immediately.
- Added regression coverage for delayed frame selection and sprite-id mapping,
  and checked room 6/13 through frame 180 (Soma takes 52 damage). Palette
  callbacks and native sounds remain the next Nemesis gaps.

## Patch 0463 - Aria Nemesis effect lifecycle timing

- Matched the native technical-child lifecycle: each Nemesis effect starts in
  state 0, enters the active state 2 on its first callback and advances its
  private timer every frame.
- Preserved the native sequence identities in both the child step and variant
  fields, and synchronized the owner's vertical flip in addition to position,
  mirror, visibility and direct frame.
- Added regression coverage for first-frame activation and timer progression.
  The scratch OAM composition selected by `sub_08045748` remains the next
  visual-fidelity gap; the current renderer still uses the owner's exported
  direct frame as a ROM-free fallback.

## Patch 0462 - Aria Nemesis technical effect slots

- Ported the three visual child slots created by `sub_080C7B54` (native
  sequence identities 9, 12 and 6) into the Aria room entity list.
- The slots now follow their Nemesis owner, mirror its direct frame, remain
  non-combat and render with the native translucent OBJ mode; they are removed
  with the owner.
- Extended Nemesis static-frame extraction to include all four direct records
  and added ROM-free coverage for the three identities and synchronized draw
  state. A room 6/13 capture still shows the encounter and Soma's 52-damage
  contact hit at frame 180.
- Palette callbacks, native sound calls and exact animation-table timing remain
  open for the next Nemesis pass.

## Patch 0461 - Aria Nemesis linked helper

- Ported the linked Nemesis helper created by `sub_080C76D4` and updated by
  `sub_080C7784`: it now follows its parent slot, selects the native static
  frames 1-3, applies the +9/+10 pixel horizontal and +37/-4 pixel vertical
  offsets, and runs the 10/40-frame and 31-frame flicker windows.
- Extended the private object exporter with Nemesis static-frame anchors and
  wired the new parent-linked role through the Aria room runtime.
- Added ROM-free coverage for helper creation, offsets, frame selection and
  removal on parent defeat. A 180-frame room 6/13 capture still renders the
  Nemesis encounter and Soma's native 52-damage contact hit.
- The three technical effect slots created by `sub_080C7B54`, palette
  callbacks and native sound calls remain the next Nemesis gaps.

## Patch 0460 - Aria Nemesis

- Ported Nemesis (enemy `0x36`) into the native Aria runtime: its compact
  four-frame OAM descriptor, idle hover, homing approach, charge, recovery,
  facing, contact boxes and shared death flames are now represented in C.
- Extended the private object exporter for Nemesis's direct-frame descriptor;
  each native record is emitted as a one-frame animation state.
- Added ROM-free state, collision and death coverage and checked it in room
  6/13, where the Nemesis sprites hover and hit Soma for 52 damage by frame
  240.
- The linked helper entities and native sound callbacks remain explicitly
  documented as the next parity gap.

## Patch 0459 - Aria Killer Doll

- Ported the Killer Doll (enemy 0x3A): its close-range wake-up, rise,
  bounded random flight, pauses, aimed charge and timed landing cycle.
- Exported its native animations and five piece frames; its killing blow
  now scatters those pieces with their native trajectories, bounce, flame
  glows, dust and delayed owner removal.
- Checked in room 6/26: it wakes and flies, hits Soma for 66 damage, and a
  killing blow breaks it into its five burning pieces.

## Patch 0458 - Aria Lightning Doll

- Ported the Lightning Doll (enemy 0x3B): its dormant wake-up, random
  bounded flight, electrical attack, landing and return to flight.
- Ported its contact spark attached to Soma and its death into five native
  pieces, including the piece-specific trajectories, bounce, flame glows,
  dust and delayed owner removal.
- Checked in room 6/20: both dolls wake and attack Soma for 60 damage; a
  killing blow scatters one into its five burning pieces.

## Patch 0457 - Aria beam skeleton

- Ported the beam skeleton (enemy 0x59): its patrol, its random closing in
  and pauses, the charged beam with its muzzle flash, drawn with the
  shrinking vertical scale of its affine slot, and its death in seven
  bones.
- The exporter writes the probe points of single sprite frames and the
  runtime keeps each animation frame's sprite number; it takes up to ten
  single frames per kind.
- Checked in room 8/3: it patrols, closes in and fires at Soma, and a
  killing blow scatters its bones.

## Patch 0456 - Aria Skull Millione, Cagnazzo and Lubicant

- Ported the shared routines of Skull Millione (0x5B), Cagnazzo (0x49)
  and Lubicant (0x63) from their ROM records: the bob, the chase above
  the player, the dive, Cagnazzo's circling, the death with blood and the
  turning trident of the last two, flung off when they die.
- Checked in rooms 8/9, 7/54 and 8/13: they chase and dive onto Soma, and
  a killed Lubicant bleeds, loses its trident and sinks in flames.

## Patch 0455 - Aria dead crusader

- Ported the dead crusader (enemy 0x52) on the skeleton knight's walk: a
  faster pace, a shield raised while Soma attacks (blows deal 0 damage)
  and its thrust or slash attack, drawn with palette bank 1.
- Checked in room 8/13: it walks to Soma and strikes him, and raises its
  shield against his blows (two blocked over 300 frames).

## Patch 0454 - Aria Mimic

- Ported the Mimic (enemy 0x60): the placed closed chest that opens when
  Soma walks up and bites with a knockback, and the hidden Mimic whose
  trigger bursts in flames when struck and drops it to the floor.
- Checked in rooms 3/16 (the chest opens and knocks Soma back) and 11/5
  (a jump attack wakes the hidden Mimic, which falls and opens).

## Patch 0453 - Aria red crow

- Ported the red crow (enemy 0x2D) on the blue crow's port: its own hover
  and V-shaped swoop with the return to its perch, and the blue crow's
  death, feathers and stun, drawn with palette bank 1.
- Checked in room 5/2: it hovers, swoops onto Soma and hovers off (frames
  60, 160, 200 and 240).

## Patch 0452 - Aria Fleaman

- Ported the Fleaman (enemy 0x18): its four random jumps with hand-set
  animation frames, the ceiling and wall pushes, the landing, the hit stun
  and the death with blink and flames. A carried Fleaman (parameters
  0xFF) waits, its carrier not being traced.
- Checked in room 3/3: four Fleamen hop about toward Soma (frames 20, 45,
  70 and 100).

## Patch 0451 - Aria dead warrior

- Ported the dead warrior (enemy 0x2F): its charge on the probe walker,
  the braking with dust and the turn, and its death thrown back in fire
  (a new `Effect/ember`: animation 1, OBJ row 7).
- The exporter accepts animations of up to 128 frames and the runtime up
  to 80 (the warrior's death has 70).
- Checked in room 5/3: it charges, brakes in a cloud of dust, turns and
  charges off (frames 30, 80, 130 and 170).

## Patch 0450 - Aria skeleton knight

- Ported the skeleton knight (enemy 0x0D) with the shared room edge clamp
  `sub_0806B120`: its advance and retreat around the player, the sword
  attack in three animations and the generic death.
- Checked in room 0/15: it walks toward Soma, backs away facing him and
  raises its sword (frames 30, 90 and 160).

## Patch 0449 - Aria Erinys and Gremlin

- Erinys (enemy 0x62) and the Gremlin (enemy 0x37) only call the
  Valkyrie's and the gargoyle's routines with palette bank 1; they run on
  those ports (Erinys without the id 0x29 recoil). The runtime registers
  enemy ids up to 98.
- Checked in rooms 9/9 (Erinys) and 6/14 (Gremlin).

## Patch 0448 - Aria Valkyrie

- Ported the Valkyrie (enemy 0x29) with the shared range test
  `sub_0806CEF8` and the straight homing move `sub_080696B0`: its approach
  above the player, the held strike and 3.0 charge, the hover back, the
  recoil when struck mid-strike, the halved contact outside the strike
  frames and the falling death.
- Checked in room 4/8: the Valkyrie hovers in, takes position above Soma
  and charges with its spear (frames 40, 100, 140 and 200).

## Patch 0447 - Aria Durga

- Durga (enemy 0x11) only calls Curly's create and update routines with
  palette bank 0; the runtime and exporter register it on Curly's port.
- Checked in room 2/19: both Durgas pace toward Soma and attack in reach
  (frames 30 and 120).

## Patch 0446 - Aria Curly

- Ported Curly (enemy 0x2B) with three shared helpers: the uphill
  slowdown `sub_0806AC5C`, the drop and wall test `sub_0806B1A0` and the
  room edge test `sub_0806D188`. It paces and turns, chases once hurt,
  lunges at the player and dies with the generic death.
- Checked in room 4/14: Curly walks down the stairs and wounds Soma; the
  knife kills it at frame 99 and it burns.

## Patch 0445 - Aria Ghost Dancer and palette-bank variants

- Ported the Ghost Dancer (enemy 0x1D): its figure-of-eight dance, the
  fade between opaque and semi-transparent every half turn, the margin
  hiding, the hit stun, its death animation and inert state 5.
- Enemies now carry `translucent` (OBJ semi-transparency, drawn at half
  alpha: the blend coefficients are not traced); the runtime draws other
  palette banks from `Enemy/<name>_bank<N>` (gold Medusa heads, Ghost
  Dancer banks 1 and 2 by parameter 0).
- Checked in room 3/6: dancers of the three palettes dance opaque (frame
  60) and semi-transparent (frames 300 and 330).

## Patch 0444 - Aria gargoyle

- Ported the gargoyle (enemy 0x57): its pursuit of the point above the
  player, the brake, the swoop and the climb back, the falling generic
  death and its inert state 2; the runtime registers enemy ids up to 87.
- Checked in room 8/12: both gargoyles leave their perches, swoop onto
  Soma and knock him back, then fly off and climb back (frames 30, 90,
  150 and 210).

## Patch 0443 - Aria zombie officer

- Ported the zombie officer (enemy 0x20) on the soldier's building blocks:
  the 0.625 walk, the close attack within 50 pixels, its three gibs at
  death and its inert state 3; the exporter adds its sprites and gib
  frames.
- Checked in room 3/4: the officer walks in and slashes (frame 47); a knife
  kill at frame 59 shows its flames and gibs (frames 63 and 73).

## Patch 0442 - Aria Medusa heads

- Ported the Medusa head (enemy 0x39) and its spawner: heads appear at the
  screen edge chosen by Soma's position and facing, swing in their native
  wave, die off screen, are stunned by hits and die with the generic
  death; gold heads use palette bank 1 (their petrification is not
  ported).
- `AosHitReport` carries the spawned enemy's parameter 1; the runtime
  draws gold heads from a second exported sequence set.
- Checked in room 6/13: gold heads arrive from the right and swing toward
  Soma (frames 60, 90, 150 and 200).

## Patch 0441 - Aria Needles enemy

- Ported Needles (enemy 0x3F, the most frequent enemy record: 47 across
  the exported rooms): the looping animation with random pauses, the
  optional bobbing, its boxes packed in the constant 0x1010F8F8, contact
  damage, the five pieces of `0x08528B44` at death and its inert state 3.
- The runtime registers enemy ids up to 63; Needles pieces count down
  their parent like spawned zombies; the exporter adds Needles and its
  piece frames.
- Checked in room 6/33 (`--spawn 130 159 --atk 50 --repeat 2`, buttons
  0x82): a crouched knife kills the Needles at x 152 on frame 5 and its
  pieces fly at frames 7, 13 and 21.

## Patch 0440 - Aria weapon sprites asset functional

- `assets.aria_weapons` no longer lists only classes 0, 2 and 3: every
  record's sprites are exported and the runtime draws classes 0 to 5
  (classes 1 and 4 checked in room 0/15 in patches 0437 and 0438), so the
  asset is functional.

## Patch 0439 - Aria class 5 weapon entity; weapon entity functional

- Ported class 5 (`sub_08022DEC`): class 0 without the deletion on Soma's
  `0x20`; only `sub_08033F58` (a player copy driven by `gEwramData +
  0x4DC`, not ported) creates it, so it is unit-tested only.
- Checked classes 0, 2 and 3 in room 0/15 (weapons 0, 40 and 50 at frames
  6 and 10) beside classes 1 and 4: the weapon entity is now functional.

## Patch 0438 - Aria class 4 guns, shared effect module

- Ported the class 4 weapon entity `sub_08022A54` (records 55 and 56):
  the gun drawn at Soma's hand, the accelerating shot with the box packed
  in the pointer `0x0810FCF8` (-8, -4, 16, 8), its wall impact and the
  bouncing shell casing `sub_080228F8`.
- Moved the shared effects to `src/runtime/aos_effect.c` / `.h` (weapons
  now emit them too) and added the impact and casing; the exporter adds
  `Effect/impact` and `Effect/casing`.
- Checked in room 0/15 with weapon 55 (`--spawn 120 446 --weapon 55
  --hitboxes`, frames 4, 6, 9 and 14): the gun fires with its muzzle flash
  and the shot box leaves the barrel. Class 5 remains open.

## Patch 0437 - Aria class 1 weapon swing and traced affine rotation

- Ported the class 1 weapon entity `sub_080224BC` (records 18 to 39): the
  swing above Soma, its turning affine angle, its x drift and its 26 / 21
  / 21-frame lifetime; `AosWeapon` gains the record's variant and the
  runtime now runs weapon classes 0 to 3.
- Traced `sub_0803E058`: pa = cos, pb = sin, pc = -sin, pd = cos with a
  flip negating a row, so a positive angle turns the flipped texture
  clockwise. The axe and class 1 swords now rotate that way (the axe's
  direction was previously inferred the other way).
- Checked in room 0/15 with weapon 18 (`--spawn 120 446 --weapon 18
  --hitboxes`, captures at frames 3, 7, 11 and 16): the blade sweeps from
  overhead to the front and floor, following its hitboxes. Classes 4 and 5
  remain open, so the weapon entity stays partial.

## Patch 0436 - Aria wooden door visibility and passage flags

- `aos_door_update` now takes the camera and hides the door outside
  `sub_08068AD4(e, -32, 0, 0x130, 0xD0)`; the runtime skips hidden doors.
- The door exposes `holding` while it holds `gEwramData + 0x42C` bits
  0x101 and clears `0xA074` bit 1 (`sub_0800C5A8`) during a passage.
- Checked in room 0/16: walking right, the door at x 1272 opens at frame
  40, Soma walks through by frame 110 and arrives in room 0/28 at frame
  150. The wooden door is now functional; the cleanup state 0x63 and the
  readers of the two global flags remain untraced.

## Patch 0435 - Aria zombie drips, puddles, gore and hit stun

- A walking zombie now drops drips (`sub_0807B1CC`, sprite frame 0xD) and
  leaves puddles (`sub_0807B33C`, frame 0, drawn behind enemies); its death
  throws two gore pieces (`sub_0807B258`) beside the generic death's
  flames.
- Ported the hit stun of `sub_0806AD24` for the zombie and its state 2
  (`sub_0807B034`) under gEwramData + 0x8E bit 0x40.
- Checked in room 0/3 (`--spawn 160 176 --atk 80 --repeat 2`, buttons
  0x12): puddle trails and drips while walking, the kill at frame 169 with
  gore and flames at frames 175 and 189. The zombie is now functional, so
  every ported Aria enemy is functional; sounds, palette bits and the
  tracking list remain open for all of them.

## Patch 0434 - Aria axe armor and skull archer effects

- Ported the drifting effect `sub_08045CEC` / `sub_08045C9C` and the
  strike flame `sub_0806D5C0`. The axe armor's death now raises its fire
  column and sparks (`sub_080B11DC`: fire on even frames along a sine,
  a spark every 8 frames, during 32 frames); a struck axe or arrow leaves a
  flame; the archer's flung pieces leave a dust puff where they land.
- Ported the inert states under gEwramData + 0x8E bit 0x40: armor state 3
  and archer state 4.
- The exporter adds `Effect/fire`, `Effect/spark` and `Effect/dust`.
- Checked in room 0/15 (armor killed at frame 63, fire and sparks at
  frames 72, 82 and 94) and room 0/24 (archer killed at frame 159, dust at
  frames 199 and 213). Both enemies are now functional; sounds, palette
  bits and the global pause `0x4BE` remain open, and the axe's rotation
  direction is still inferred.

## Patch 0433 - Aria grenade burst and explosion particles

- Ported the particle entity `sub_08068F78` (7/8 speed decay, motion,
  40-frame blink, deletion after its animation) and the grenade's
  emitters: `sub_0806D894` smoke at a random direction every frame,
  `sub_0806D930` dark smoke at timer 3 and `sub_0806D644` flame particles on
  odd timers, during the burst and the explosion's first 8 frames.
- The exporter adds `Effect/smoke` and `Effect/dark_smoke` (animation 5,
  OBJ rows 6 and 5); `fusion_aria_runtime` draws them and reports the live
  effect count on the capture line.
- Checked in room 0/15 (`--spawn 170 430`): a grenade bursts on Soma at
  frame 232 (3 effects, then 12 at frames 238 and 246). The zombie soldier
  is now functional; its sounds and the possible blending of the dark
  smoke remain open.

## Patch 0432 - Aria zombie soldier gibs and state 3

- The soldier's first death frame now creates its two gibs
  (`sub_08092FCC` / `sub_08093078`: sprite frames 0x13 and 0x14 with the
  native random offsets and speeds, falling with ay 0x1800 until below the
  soldier's y), before the generic death's flames.
- Ported state 3 under gEwramData + 0x8E bit 0x40 (no collision block or
  floor snap, animation only). The exporter adds frames 0x13 and 0x14.
- Checked in room 0/15 (`--spawn 250 430 --atk 80 --repeat 2`): the kill
  at frame 241 shows the gibs and flames at frames 243, 249 and 257. The
  grenade's explosion particles remain open, so the soldier stays partial.

## Patch 0431 - Aria blue crow feathers, blood and hit stun

- The crow's death now creates its two feathers (`sub_080C9E2C` /
  `sub_080C9ED4`, sprite frame 8, 49 + 25 frames of drift and blink) and
  blood splashes (`sub_0806E664` / `sub_0806B5DC` type 1: animation 13 of
  the shared frames, OBJ row 8 = bank 4 of `0x082099FC`) at death and every
  16 frames, beside its flames.
- Ported the hit stun of `sub_0806AD24` / `sub_0806AE54` (flight and
  animation frozen while `+0x2D` counts down from 8) and state 3
  (`sub_080C9FFC`) under gEwramData + 0x8E bit 0x40.
- Shared effects are generalized (`AosEffect`: flame or blood, one
  animation set per kind); the exporter adds `Effect/blood/anim_13` and the
  crow's frame 8.
- Checked in room 0/12 (`--spawn 100 750 --atk 50 --repeat 2`, buttons
  0x22): the kill at frame 195 shows feathers, blood and flames at frames
  198, 215 and 240; with `--atk 3` the crow survives five hits. The crow is
  now functional; sounds, palette bits, the global pause `0x4BE` and the
  tracking list of `sub_08021924` remain open.

## Patch 0430 - Aria death flames and the bat's state 4

- Ported the death flame `sub_0806E524` and its spawners: `sub_0806BE74`
  (bat every 15 frames over 8 x 6 pixels, blue crow every 6 over 8 x 8) and
  the generic death `sub_0806AEAC` (every 4 frames over 16 x 12), with the
  native random draws (x, y, then the mirror flag).
- Traced the flame's art: the common sheet `0x081C15F4`, frames
  `0x0820ED60` animation 0, and OBJ palette row 7, which `sub_0800C5D8`
  fills with bank 3 of `0x082099FC`; `scripts/aos_object_sprites.py`
  exports it as `Effect/flame/anim_0` and `fusion_aria_runtime` updates
  and draws flames after the enemies.
- Ported the bat's state 4 (`sub_080AD6AC`), entered at creation while
  gEwramData + 0x8E bit 0x40 is set (set by `sub_0804C404`, cleared by
  `sub_0804CC14`; its meaning is not traced).
- Checked in room 0/5 (`--spawn 210 260 --atk 50 --repeat 2`): the knife
  kills a bat near frame 200 and the flames rise from the falling bat in
  captures at frames 212, 228 and 245 (ignored `captures/aria/`). The bat
  is now functional; its sound 0x72, palette bits and `sub_080683BC`
  remain open.

## Patch 0429 - Aria interactive options and held-key navigation

- The diagnostic SDL Options screen now toggles locally provided title art
  visibility using A; B returns to the diagnostic menu.
- Up/down held-key navigation repeats after 32 frames and every six frames;
  confirm/back remain rising-edge actions and are device-free tested.
- Original GameModeMainMenuUpdate, ROM graphics, save selection and game
  startup are still unported; native parity remains partial.
- Native checklist and web dashboard are regenerated after test execution.

## Patch 0428 - Interactive Aria SDL menu preview

- The SDL title host now shows an interactive diagnostic main menu.
- Navigate with arrows, confirm with A/START and return with B.
- New Game, Continue and Options explicitly remain placeholders.
- Pure C menu navigation has a dedicated CTest. This does NOT port
  native GameModeMainMenuUpdate or real save screens.
- Native parity stays partial; checklist/dashboard regenerated.

## Patch 0427 - Aria runnable SDL title preview

- Added `fusion_aria_title_preview`, a separate SDL3 executable which
  displays a locally supplied BMP, runs the native stage-2 fade and
  stage-8 input helper at 60 Hz, and offers an asset-free placeholder.
- Invoke `build/fusion_aria_title_preview --title-bmp /path/to/title.bmp`
  (or without arguments for a diagnostic blank-art preview).
- The preview is **not** the original title render: stages 3-7 graphics,
  sprite entities, real menu actions and integrated boot remain absent.
- Native parity stays partial; checklist and dashboard regenerated.

## Patch 0426 - Aria title stages six and seven

- Extended `aos_title_stages` with native stage-6 secondary-object
  fade (0x88 counter, alpha every eight frames) and stage-7 reveal
  deceleration and 600-frame idle timer handoff.
- CTest checks the counters, reveal, flags and 6->7->8 transitions.
- Native trig affine values, original title graphics, SDL frontend
  and file menus remain unported; Aria title parity stays partial.
- Checklist and local web dashboard regenerate after tests.

## Patch 0425 - Aria title motion stages four and five

- `aos_title_stages.c` implements the native stage-4 movement and
  stage-4-to-5 same-call fallthrough once the sprite flag is set.
- Stage 5 reproduces the affine contraction, 64-step counter,
  secondary entity flags and transition to stage 6.
- Device-free CTest covers precise state changes; sprite and BG
  rendering, stages 6/7 and the complete title lifecycle remain.
- Native parity remains partial; checklist and web dashboard are
  regenerated after testing.

## Patch 0424 - Aria title stage-three motion

- Reproduces stage-3 native title entity fixed-point motion, scale,
  palette trigger at 0x40000 and stage-four entry below 0x30000.
- Device-free `aos_title_motion` tests verify coordinates, flags,
  affine scale and thresholds. This does not render any title assets;
  native sprite, palette, remaining stages and SDL integration remain.
- Native parity stays partial; checklist and web dashboard regenerated.

## Patch 0423 - Aria title brightness fade foundation

- Ports the bounded `GameModeTitleScreenUpdate` stage-0 initial
  brightness and stage-2 16-to-0 brightness fade, including the
  native fallthrough tick, stage transition and 33-call timing.
- The implementation is a device-free state helper, tested by
  `aos_title_fade`; native graphics, sprite initialization, stage
  3-7 animations, SDL integration and saved-game menus remain absent.
- Native parity remains partial. Checklist and local HTML dashboard
  are regenerated after all tests.

## Patch 0422 - Aria title interactive input stage

- `aos_title_input.c` ports the bounded stage-8 input/idle-timer decisions
  from `GameModeTitleScreenUpdate` (10-second timer, up/down repeat,
  A/START acceptance, B submenu return and timeout).
- The code is device-independent and covered by `aos_title_input` CTest.
- It is not yet connected to the SDL room viewer: original boot,
  title graphics, title entity state, menu animations and fades remain.
- Native annotations stay partial; derived checklist and web dashboard
  are regenerated in that order after testing.

## Patch 0421 - Aria mode dispatch and title destination foundation

- Added `aos_game_mode.c`: native `SetGameMode`, `IncrementGameMode`
  and the result-handling tail of `GameModeUpdate` from `main.c`.
- Captured the five title-mode destinations in the original ordering
  (main menu, soul trade, logo, boss rush, sound test), without
  claiming that `GameModeTitleScreenUpdate` or menu graphics are ported.
- Device-free C tests cover transitions, reset stages and selection bounds.
- Original title art, animation, saved-game menus, display effects and
  the boot integration remain to be implemented and capture-tested.
- Updated native tracking; web dashboard regenerated after checklist.

## Patch 0420 - Aria native soft reset detection

- `aos_input_soft_reset_check` ports `SoftResetCheck` from native `main.c`,
  including its one-shot inhibit bit, newly pressed key gate, and the
  A+B+Select+Start chord semantics (all four rising together do not reset).
- Device-free assertions test held, simultaneous, staggered and inhibited
  combinations. Native `sub_080106DC` remains a separate unported room
  transition, not an input function.
- The Aria game startup/reset lifecycle is not implemented, so this cycle
  intentionally does not trigger a host reset or claim in-game parity.
- Native parity checklist and local web dashboard are regenerated together.

## Patch 0419 - Aria native input foundation

- `aos_input_update` ports Aria's `UpdateInput` held/new inputs, native
  repeat countdown and the one-frame player-input override reset.
- `aos_input_set_player` ports `SetPlayerInput` priority and edge rules.
- `sub_080106DC` is a room transition with fades and camera movement,
  not an input polling routine. Only its `SetPlayerInput(1, 0)` call
  is covered; do not classify it as ported or remove it from backlog.
- SDL3 sampling remains in `aria_runtime.c`; tests cover repeat,
  release/repress, priority arbitration and input suppression.
- Aria gamepad parity remains partial pending recorded in-game checks.
- The dashboard is regenerated after the native parity checklist.

## Current product state

The repository is a native-content reconstruction and editor project. It is not
yet a playable crossover and contains no active simulated or emulator-backed
game frontend.

| Area | State | Verified boundary |
|---|---|---|
| Story | Data baseline | Complete campaign bible and dependency-checked parallel timeline |
| Boss inventory | Verified native identities | 9 MZM encounters and all 11 Aria campaign bosses source/ROM-mapped |
| Savepoints | Verified metadata | 29 MZM platform rooms and 17 Aria map-flag rooms |
| MZM rooms | Partial, systematically audited | 331 descriptors: 330 partial renders, 1 unsupported sentinel, 0 errors |
| Aria rooms | Partial, systematically audited | 343 descriptors: 342 partial renders, 1 unsupported affine/non-text room, 0 errors |
| Character assets | Partial | canonical local Samus runtime library: 1,072 indexed sequences, 2,640 unique BMPs; 566 projectile sequences; Soma preview extraction |
| GTK4 editor | Active | shared room documents, native-data overlays, catalog and area/world render audits |
| Headless backend | Active foundation | stable CLI envelope plus room, tile, collision, door, transition, entity, story and audit operations |
| Native gameplay engines | Experimental MZM and Aria runtimes | MZM now boots through the intro, title and file select into its room runtime, with a broad but incomplete set of Samus systems, enemies and the Deorem, Acid Worm, Gadora, Imago Larva, Kraid, Ridley and Mother Brain encounters. Aria remains an experimental room/combat runtime. Neither campaign is complete and the crossover host is not implemented. |
| Dual-character AI | Not started | story/design requirement only |
| Cross-world travel | Not started | savepoint metadata exists; no native runtime loader |

## Patch 0047 - active-tree and documentation cleanup

- Copied 94 superseded files to local ignored `legacy/` storage with their
  original paths preserved.
- Removed the rectangle-room demo, sample tilemaps/world graph, simulated
  backends, mGBA diagnostic frontend, transition experiments and their tests
  from the active tree.
- Removed superseded feasibility/recompilation/side-project documents and
  replaced accumulated prototype documentation with concise current references.
- Reduced CMake to the native editor core, GTK4 editor, SDL3 room viewer,
  active native tests and the Python extraction/data suite.
- Removed obsolete graph/tile/demo/capture controls from GTK4 while retaining
  native room tabs, atlas browsing and local authentic sprite previews.

## Patch 0048 - Aria native room inventory

- Expanded the hash-gated Aria importer from the global map into all 343 native
  room descriptors, including three background records, resource references,
  entity placements and 725 resolved room transitions.
- Bounded every pointer walk, variant chain and terminated list; the private
  generated catalog contains metadata and ROM addresses, never payload bytes.
- Matched all eleven campaign bosses to native rooms and enemy-table identities.
  Chaos has two explicit phase rooms; repeated ordinary boss-type enemies are
  not misclassified as campaign encounters.
- Recorded raw enemy-table health values and named cvaos create/update symbols
  in the tracked boss inventory.

## Verified commands

Run after each change:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 scripts/check_no_proprietary.py
```

The latest exact result is recorded in the corresponding commit handoff.

Cleanup validation on 2026-10-08: the reduced CMake project built without
warnings, CTest passed `5/5`, the consolidated Python suite passed `82/82`, the
GTK4 editor survived a three-second Xvfb smoke run, and the proprietary-file
guard passed. The active editor binary has no mGBA linkage.

Patch 0048 validation on 2026-10-08: CTest passed `5/5`, the Python suite passed
`85/85`, the exact-ROM Aria import completed with 343 rooms and 2,336 entities,
and the proprietary-file guard passed.

## Patch 0049 - Aria native room previews

- Added a verified, generated index for all native Aria rooms, with save/boss markers.
- Added a read-only GTK4 browser with asynchronous on-demand graphics decoding,
  BG1/BG2/BG3 selection, composite preview and diagnostic collision preview.
- A room lacking BG1 may now composite another successfully decoded text layer.
- This does not imply the graphics reconstruction is complete; unsupported
  affine modes, animation, blending and entities remain explicitly unfinished.

## Patch 0052 - deterministic accessibility backend for GTK tests

- The unsaved-close dialog test is run with GTK_A11Y=test instead of
  trying to connect to the desktop AT-SPI bus under Xvfb. This is
  restricted to the GTK test binary/CTest, never the production editor.
- The modal close workflow itself remains tested; GTK criticals and
  memory errors are still fatal. Real GTK4 validation is pending.

## Immediate priorities

1. Migrate active historical asset paths one consumer group at a time.
2. Replace the Samus compatibility discovery adapter with one generic native scanner.
3. Map suits, transitions and action priorities from verified native data.
4. Build an equivalent automated Soma/Aria extraction pipeline.
5. Define both native engine adapter contracts around validated exported data.

## Non-negotiable constraints

- Never track ROMs, saves or extracted proprietary assets.
- Never mutate ROMs or upstream submodules.
- Never substitute fake geometry for missing original data without an explicit
  diagnostic label.
- Never call extracted metadata a completed native gameplay feature.

## Patch 0051 - confirm close and live tile stamp

- Native MZM and Aria documents display their real selected metatile as a semi-transparent cursor preview on hover; no painting occurs before mouse press.
- Picking a metatile from a room or the palette switches back to the Pencil stamp tool.
- Unsaved tab close offers Cancel, Discard changes, or Save and close in a GTK4 modal window; failed writes keep the document open.
- Additional GTK lifecycle test covers cancel, save and discard; private ROM-derived assets and overrides are never committed.

## Patch 0053 - common native world and narrative workspaces

- One original-coordinate *cell grid* for MZM and Aria. Aria cells are
  original 64x35 map occupancy; MZM currently exposes only room anchors.
- Bounded private ROM pixel-preview generation starts when a world or region is
  opened; only supported source data produces thumbnails.
- Native room tabs use one toolset for both worlds, and Ctrl+W routes through
  unsaved-edit confirmation with dialog teardown before close.
- GTK4 now exposes three-track event orchestration and a TOML-based shared
  cutscene editor with validate-before-save. These are **data editing tools**;
  no world-specific native cutscene runtime or automatic adapter exists yet.
- Keep room collision, precise Zero Mission map masks, source-accurate layer
  compositing, keyframe UI and native engine adapters on the outstanding list.

## Patch 0054 - GTK native document lifecycle hardening

- Tracked GTK page and label pointers with widget-owned document references
  so external notebook destruction and detached tools cannot retain stale labels.
- Closing callbacks now ignore documents during teardown; workspace shutdown
  detaches outstanding notebook pages before releasing manager ownership.
- Added lifecycle regressions for external page removal, repeated close/reopen,
  shutdown with an unsaved modal, and detached notebook cleanup.
- Build, CTest and on-device GUI verification must be recorded after running
  the patch locally; no result is claimed here.

## Patch 0055 - GTK4 notebook owner resolution

- Fixed closing, selecting and moving native room tabs by finding the owning
  GtkNotebook in the widget ancestor chain rather than requiring it as the
  immediate widget parent.
- Tightened GTK lifecycle tests: they now assert that pages are actually
  removed from *both* dock notebooks, including detached docks, not merely
  removed from the document manager's bookkeeping.
- Build, CTest and real GTK4 reproduction still require local validation.

## Patch 0056 - one room browser per world, one implementation

- **Zero rooms** and **Aria rooms** are instances of the same GTK4 room browser:
  area selection, source metadata, layer previews, async rendering, double-click
  to open the same editable native document tools, and identical close behavior.
- The right-click menu exposes the future **Create room** entry as disabled.
  It must not create pretend playable rooms: a verified authored-room schema,
  spatial/door validation, export adapter and consuming native gameplay runtime
  are required before it can be enabled for either world.
- The Aria global map is indexed separately (preview budget 0) before costly
  room preview generation; subprocess decoder errors are shown in the GUI.
  Existing map generation remains asynchronous, private and original-coordinate.
- Shared room browser GUI regression is added to CTest under Xvfb.

## Unified player systems - accepted requirements, pending implementation

- One character-independent capability interface: acquired powers, abilities,
  inventory flags, unlock conditions and per-world game mechanics.
- Player-specific presentation and rules: Samus upgrades, ammo/energy and
  equipment versus Soma souls, MP/HP, attributes, items and abilities.
- One status/menu framework with different views, stats and controls depending
  on which playable character is active. No fabricated equivalence of stats.
- Native controller support, configurable mapping, keyboard/controller parity,
  in-editor input preview and runtime gamepad navigation; SDL3 input backend
  must be attached only once engine input/action contracts exist.
- Cross-world abilities, room requirements and cutscene/event conditions must
  reference a shared canonical schema with world-specific adapters and checks.
- Export only project-authored assets. Never write ROMs or claim unfinished
  player systems exist in native gameplay yet.

## Patch 0057 - GTK4 room browser context lifetime

- Fixed a GTK4 ownership violation in the shared Zero/Aria browser: the
  future Create room popover was parented directly to GtkListBox. GTK4 expects
  GtkListBoxRow children, and filtering may dereference non-row widgets.
- A GtkOverlay now owns the popover, while the scrolled room list remains the
  overlay's regular child. Right-click popup coordinates are translated from
  the list into the overlay's coordinate space.
- The browser GTK regression now checks popover parenting for both games and
  repeats browser creation/destruction. Local GTK4 build and CTest must confirm.

## Patch 0058 - native room context menu ownership

- Changed the shared room browsers to use `GtkMenuButton` as the popover host,
  instead of placing a `GtkPopover` in a `GtkOverlay` allocation slot.
- The right-click shortcut opens the same anchored menu. Authored-room creation
  remains disabled until its native engine adapter and schema exist.
- The GTK test now emits progress diagnostics around both browsers' creation
  and removal, to identify the failing phase if a crash persists.
- A successful GTK4 build/CTest run on Debian is required before committing.

## Patch 0059 - GTK4 room browser test type correction

- Fixed the GTK4 room browser test's `-Werror=compare-distinct-pointer-types`
  by comparing `GtkWidget *` to `GtkWidget *` explicitly.
- The previous CTest run used the old test executable because the new test
  failed to link. It is not evidence of a remaining crash in patch 0058.
- The browser implementation is unchanged; rebuild must succeed before CTest
  results are considered meaningful. GTK4 runtime verification is pending.

## Patch 0060 - correct GTK popover return type

- GTK4's `gtk_menu_button_get_popover()` returns `GtkPopover *`, not
  `GtkWidget *`. Compare against `GTK_POPOVER(popover)` in the browser GTK
  regression test so `-Werror=compare-distinct-pointer-types` no longer blocks
  the test binary from linking.
- Correct the incorrect patch 0059 assertion and retain the 0058 diagnostics.
- The GTK browser crash is **not confirmed fixed** until the newly linked
  test executable passes on Debian. Do not rely on a stale CTest binary.

## Patch 0061 - GTK4 browser teardown ownership

- The shared Aria/Zero browser explicitly retains the filtered list and its
  referenced dropdowns, status labels, preview and menu button until the
  notebook page finalizes. GTK can otherwise destroy a toolbar before the
  GtkListBox callbacks that still read its controls.
- The page's state finalizer disconnects signal handlers and removes the list
  filter before releasing widget references; this is GTK lifetime hardening,
  not a change to private room data or playable world geometry.
- The browser GTK contract now selects synthetic rows before closing both tabs,
  exercising the close path without requiring local extracted ROM assets.
- The supplied GDB trace stopped during GTK settings initialization, not the
  CTest Aria tab-removal crash; run the debugger with CTest's isolated
  XDG_CONFIG_HOME if another crash persists. Local GTK4 validation pending.

## Patch 0062 - GTK4 dropdown model ownership

- Fixed a double `g_object_unref()` in the shared Aria/Zero room browser.
  `gtk_drop_down_new()` takes ownership of the passed `GListModel`,
  so an additional unref of the `GtkStringList` invalidated its model.
- The page teardown crash was reproduced in GDB at
  `g_list_model_get_n_items()`, via the browser's dropdown release.
- The GTK regression now checks both area models (8 and 13 entries)
  and changes their selections before removing the two tabs.
- Build, all six CTest cases, and the proprietary-file guard require
  validation on the developer's Debian GTK4 environment.

## Patch 0063 - shared private authored-room draft contract

- Added a strict versioned `metroidvania.authored-room` schema shared by Zero
  Mission and Aria, with stable world/area/slug identity and explicit metadata
  separating project-authored geometry from verified original ROM content.
- New `scripts.authored_rooms` can create, validate and list draft documents
  under ignored `assets/extracted/authored_rooms/<world>/<area>/`. Files are
  create-only, with duplicate rejection, input bounds and symlink guards.
- Drafts specify intended screen dimensions but contain no invented original
  assets, collision, entities or source-room data. They are deliberately marked
  `draft_not_playable`; game export fails closed until native engine adapters
  and spatial/collision validation exist.
- Added ROM-independent tests for both worlds, all areas, path traversal,
  duplicate IDs, malformed drafts and engine-readiness misrepresentation.
- **The GTK Create room action is intentionally still disabled**. The next
  step is connecting the common authoring form and editable project-owned
  room layers to this contract, then validated native engine adapters.
- Developer GTK4, CTest and proprietary-file verification remain required.

## Patch 0064 - GTK4 create-room draft form

- Enabled Create room in the shared Zero/Aria room browser context menu.
- A common GTK4 dialog collects area, slug, name and 1..8 screen dimensions;
  the active area is preselected independently for each game.
- Saving invokes `python3 -m scripts.authored_rooms create` with a separate
  argument vector (no shell) and asynchronous completion/error feedback.
- Duplicate IDs and private output paths are validated by the existing
  create-only Python schema; no ROM, original room or engine assets are changed.
- This creates **unplayable project drafts**, not native renderable/playable
  rooms. Existing draft browsing, editing and engine adapters remain pending.
- Added an Xvfb GTK4 form-opening test for both worlds without writing assets.
- Run local build, CTest and proprietary-file guard before considering verified.

## Patch 0065 - GTK4 button activation in create-room test

- GTK4 does not expose the old `gtk_button_clicked()` GTK3 helper.
- The GTK4 regression test uses `g_signal_emit_by_name(action, "clicked")`
  to exercise the real create-room action signal without proprietary assets.
- Only test code and documentation changed; the existing editor dialog and
  Python authored-room contract are not modified.
- Run the developer's GTK4 build, CTest and proprietary-file guard to verify.

## Patch 0066 - private authored rooms in GTK4 browsers

- Both Zero and Aria room browsers asynchronously list **validated** private
  drafts through `scripts.authored_rooms list --world ... --format tsv`.
- Draft entries are marked `[DRAFT]`, show their stable identity and intended
  screen dimensions, and participate in the original area filter.
- A Refresh drafts button reloads the validated catalog without duplicating
  entries; successful Create room jobs also refresh the corresponding browser.
- Selecting or activating a draft cannot invoke the original-room import or
  renderer. Open/Render controls are disabled for drafts; draft editing and
  engine export remain unsupported until authoring adapters are implemented.
- Async listing uses a strong page reference, page-attachment check, and
  monotonic refresh generation to avoid late results changing closed tabs.
- The GTK parser is tested with synthetic Zero and Aria draft rows only; the
  CLI TSV contract is independently tested with isolated temporary files.
- GTK4 CMake build, CTest and proprietary-file guard still require local
  verification on the developer's Debian workstation.

## Patch 0067 - source-faithful Aria global map and copyable GTK4 diagnostics

- Fixed Aria global-map generation for `MV_AOS_WORLD_2`: the importer stores
  original minimap occupancy as `rooms[*].map_cells`, rather than the
  previously expected top-level `map_cells`. Existing private `world.json`
  catalogs now work without re-importing the ROM.
- The generator validates original 64x35 coordinates, flags, unique occupied
  positions, and the imported `mapped_cells` total; it never infers footprints.
  Existing optional top-level cell catalogs are still accepted.
- Made informational/status/error/help labels selectable and Ctrl+C-copyable
  across Global maps, room browsers and creation dialogs, native room editor,
  story/cutscene workspace, Explorer, Inspector, and ROM visuals.
- Increased the Global maps diagnostic excerpt from 500 to 2048 characters.
- Added source-only Aria map-contract regression cases and GTK browser label
  selection checks. Real Debian GTK4 compilation/CTest is still required.

## Patch 0068 - truthful multi-cell global-map room spans

- Aria rooms sharing native minimap cell identity render as spanning rectangles
  in the GTK grid rather than six repeated identical full-room thumbnails.
  Irregular original shapes are partitioned into exact occupied rectangles;
  no missing cell is filled and clicking any segment selects the whole room.
- Zero Mission original minimap mapX/mapY are offsets from local player
  screen-grid coordinates (pinned src/minimap.c). If verified local Clipdata
  RLE can be decoded, its 16px block width/height supplies a conservative
  rectangular room footprint in 15x10 block units. All ambiguous/overlapping,
  missing or out-of-bounds candidate spans revert to original anchor-only.
- Source-provenance is preserved per generated map row (0=MZM anchor only,
  1=validated native MZM clipdata extent, 2=original Aria minimap cell).
  Existing six-field indexes remain readable by the GTK browser.
- Preview images are shown at most once for a complete rectangular room;
  nonrectangular fragments never repeat a misleading full-room screenshot.
- Python tests cover 2x3 spans, coordinate conflict fallback, bounds,
  Aria L-shaped cells and provenance. Runtime GTK4 tests remain mandatory.

## Patch 0069 - original global-map structure and automatic previews

- Replaced MZM's misleading spatial reconstruction with original per-area
  32x32 pause-screen minimap occupancy from private LZ77 sources, when
  available. Decode from pinned raw extraction or a locally verified ROM,
  never from guessed rectangles. Original room anchors remain separate from
  map tiles with **unknown room ownership**; double-click cannot import an
  arbitrary room for unassigned minimap tiles.
- Aria defaults to an all-areas 64x35 castle map; individual areas remain
  filterable, but one region can no longer masquerade as the global castle.
- The complete 64x35 grid uses homogeneous GTK sizing and vertical scrolling.
- Overview generation automatically indexes on world selection and then
  incrementally processes each area's original graphics previews, keeping
  the UI responsive. Generated output stays under ignored private assets.
- Source and GTK4 regression checks required before final commit/push.

## Patch 0070 - draggable world maps and dedicated map creation tab

- Added left-button drag panning across the true GTK4 scrolled grid, with
  clamped scroll adjustments and unchanged room double-click activation.
- Every occupied or empty map cell has a right-click context popover with
  coordinate copying and project-map creation navigation; verified room cells
  additionally offer Open room editor. Spanning Aria rooms report the exact
  clicked minimap case, not just the rectangle origin. Unknown MZM tiles never
  open arbitrary ROM room IDs.
- Reopening Global maps reuses the existing Map creation tab instead of
  creating duplicates.
- A dedicated Map creation tab now launches the same validated Zero/Aria
  room draft dialogs; coordinates selected from an empty map cell are shown as
  **informational only**. Map placement and new-room gameplay adapters remain
  deliberately unavailable until a separate validated placement contract.
- Generated or original ROM data are never overwritten by the UI.
- GTK4 CMake build, CTest and no-proprietary guard are required on Debian.

## Patch 0071 - two-level workbench and unified map panning

- The right sidebar also separates permanent ROM visuals / Inspector from
  transient native metatile tool palettes in a nested notebook.
- Permanent navigation workspaces (Zero rooms, Global maps, Map creation,
  events, cutscenes and Aria rooms) stay in the upper GTK4 notebook.
- Native room documents open in a SECOND GtkNotebook inside the permanent
  Open editors workspace. Their closable tabs never clutter navigation tabs;
  the existing save/discard/cancel contract and detachable document tabs remain.
- Focusing a room document activates both its nested tab and the upper Open
  editors tab, even when the document was opened from a global map.
- The current world badge follows the selected native editor, not merely
  the permanent Open editors tab.
- Panning uses a capture-phase gesture on a padded scrollable surface for both
  original maps. Padding leaves cell coordinates/sizes and source occupancy
  unchanged, including the native 32x32 Zero Mission minimap.
- GTK4 regression covers nested focus/close without ROM assets. Build/CTest
  and physical mouse navigation require validation on the Debian workstation.

## Patch 0071 - GTK4 nested focus recovery

- GTK4 notebook page widgets can have internal parents rather than the
  notebook itself; walking direct parent-child pairs skips registered pages.
- Native `focus_page()` now selects the containing registered page of each
  ancestor GtkNotebook. Opening a room activates both permanent and nested
  editor tabs, including the separate native palette dock.
- The existing GTK lifecycle test covers both outer notebooks, document close
  and preservation of permanent tabs. No ROM data is needed for the test.
- Compilation, CTest and proprietary guard must pass before commit/push.

## Patch 0071 - nested focus source-contract recovery

- After the GTK4 notebook ancestry fix, the original source-only Python test
  was still asserting that the removed direct-parent traversal existed.
- Its assertion now checks the real registered GtkNotebook page lookup and
  the descendant relationship used by `focus_page()`; the GTK runtime test
  remains the authority for actual parent/child notebook activation.
- Existing room content, generated assets, and Git history remain unchanged.

## Patch 0072 - dock allocation and splitter-aware responsiveness

- Enclosed each existing GTK4 notebook (Explorer, main editor, Inspector) in
  a scrollable, clipped viewport so oversized toolbars or notebook pages can
  no longer overdraw another splitter pane. Tab ownership and detach groups
  are unchanged.
- Both nested paned splitters explicitly support shrinking their children;
  horizontal dragging reallocates the middle workspace instead of obscuring it.
- The optional right Inspector now responds to actual space left by Explorer,
  not just window-wide breakpoints, and returns when space is restored.
- Added source-contract regressions for pane boundaries, resize behavior,
  and preservation of the permanent/temporary notebook arrangement.
- GTK4 interaction testing on the developer workstation remains necessary.

## Patch 0073 - private spatial placements for authored rooms

- Versioned, ignored, project-owned `assets/extracted/authored_rooms/placements.json`.
- Only validated existing draft identities may be placed/moved; the native
  Zero 32x32 per-area and Aria 64x35 shared minimap bounds are enforced.
- Refuse overlaps with original ROM minimap cells or other project rooms;
  missing original overview data fails closed rather than inventing geometry.
- Add a functional private placement panel to Map creation, including a
  refreshable draft selector and coordinates from the global-map right click.
- Render project drafts with distinct dashed borders; clicking one never opens
  an unrelated original ROM room. No playable engine export is implied.
- Python placement tests and full local GTK4 build/CTest required before push.

## Patch 0074 - source-joined MZM footprints and map interaction polish

- Zero Mission global-map ownership now joins `RoomEntryRom` origins, exact
  Clipdata playable dimensions and original pause-minimap occupancy. The
  engine's two-block guard border on each side is removed before converting to
  15x10-block screens; missing native minimap cells are never filled.
- Rooms sharing an origin are reported as progression variants. Overlapping
  candidate geometry without unique origin evidence remains explicitly
  unassigned, with a private JSON audit containing candidate rooms/origins.
- The current verified local input resolves 1,015 of 1,180 native cells;
  37 cells remain geometry-ambiguous and 128 have no geometry claim. Sixteen
  grouped anchors intentionally absent from the pause minimap remain navigable.
- Both shared room editors now expose a read-only collision/wall overlay:
  native MZM Clipdata at 16x16 resolution and Aria BG1 collision at 8x8.
  Original values are visualized diagnostically and never written to ROM.
- Ctrl+mouse-wheel zooms both global maps and native room canvases. Map panning
  now uses stationary scroller coordinates and coalesces adjustment updates to
  one per GTK frame, removing the feedback jitter/flicker caused by dragging a
  moving content widget.
- Aria global-map semantics remain unchanged. Python contracts, GTK4 build,
  CTest and the proprietary-data guard are required before push.

## Patch 0075 - native room records and shared object catalog

- Native room imports now produce a bounded private annotation table alongside
  each work file. Zero Mission records preserve spriteset identity, graphics
  slot and event variant; Aria records preserve entity identity, parameters,
  flags and room-transition targets.
- Every room editor exposes independent Walls, Objects, Doors and Events
  overlays plus a Room data inspector. Trigger controls are present but
  disabled and labelled unavailable until native trigger structures are
  decoded; original records remain read-only.
- Added a permanent two-column Object catalog for both games. It inventories
  206 Zero Mission primary sprite types with decoded stat expressions and 174
  Aria entity types present in the verified local world catalog, including
  placement counts and known boss health.
- Missing object images use an explicit diagnostic icon. Create/clone actions
  are disabled until a project-owned object schema and validated engine
  encoders exist; the editor does not claim native mutation or sprite support.
- Nested palette notebooks now resolve their actual owning notebook during
  close and detach operations, preserving the GTK document-lifetime contract.
- Validation: 160 Python tests and all six CTest targets pass; the GTK4 editor
  was smoke-tested under Xvfb with the object catalog, Brinstar 023 annotations
  and object overlays rendered from local private data.

## Patch 0076 - semantic object identities and scroll-bounded MZM placement

- Aria catalog and room overlays no longer expose opaque `kind/id` labels.
  Enemy IDs are resolved through the hash-verified ROM constructor table and
  named pinned cvaos symbols; bosses, ordinary enemies, special objects,
  candles and conditional pickup classes now have semantic display names.
- Zero Mission object labels remain based on the pinned primary-sprite enum,
  with numeric variants formatted for readability and unused types sorted away
  from the main catalog entries.
- Zero Mission global-map ownership now uses all 138 native custom-scroll room
  records before falling back to Clipdata rectangles. Door and sprite
  coordinates provide direct evidence where source-derived regions overlap.
- With the current verified private input, owned native minimap cells increase
  from 1,015 to 1,065 of 1,180. Ambiguous cells fall from 37 to 8; the remaining
  overlaps stay explicitly unassigned instead of receiving a guessed room.
- The GTK catalog and corrected Brinstar overview were smoke-tested under Xvfb.
  Validation passes 165 Python tests, all six CTest targets and the
  proprietary-data guard.

## Patch 0077 - native room-record context menus

- Visible objects, doors/transitions, events and decoded triggers now accept a
  secondary click directly on the room canvas. Hit testing follows the active
  overlay and current zoom, preferring the topmost visible native record.
- The context menu shows the semantic label, native identity, variant,
  geometry and decoded parameters. It can open the full information window,
  locate the matching row in **Room data**, or launch a type-specific editor
  shell.
- The same context menu is attached to every **Room data** entry. The selected
  canvas marker receives a yellow outline so overlapping records remain clear.
- Editor shells are intentionally read-only and keep Apply disabled until a
  versioned project schema and validated per-engine encoder exist. They never
  modify the original ROM or private imported annotations.
- Object-menu and editor-window interaction was smoke-tested under Xvfb with
  Brinstar 023. Validation passes 166 Python tests, all six CTest targets and
  the proprietary-data guard.

## Patch 0078 - semantic, independent native entity overlays

- MZM room sprites and Aria entity records use explicit ENEMY/ITEM/OBJECT/DOOR/OTHER
  annotation kinds. No conditional MZM sprite variant is relabelled as an
  EVENT: the original variant and event identifiers remain in the details.
- Items and enemies can be filtered independently of doors, scenery, events,
  triggers and unknown native actors in the shared GTK room editor.
- Zero Mission classification consults pinned native sprite identities and
  source sprite health/damage fields. Unknown types stay OTHER; no fake item
  or enemy identities are invented. Unparsed special item/trigger tables are
  not claimed to be decoded. Existing legacy ENTITY files remain viewable
  under OTHER until their private native annotations are regenerated.
- Aria native kind/id controls enemy, pickup, special object and door/gate
  semantics, while separately parsed screen transitions remain DOOR records.
- The pre-existing GTK source contract is updated to assert all eight overlay
  labels and the still-disabled native trigger decoder.
- All records and original graphics remain read-only. No ROM bytes are altered.

## Patch 0079 - editable private room entity markers

- Shared Zero Mission / Aria room editor: right-click empty room pixels to
  create a project enemy, item or world-object marker; the selected world and
  verified room geometry are enforced by the project entity validator.
- Native source annotations are never modified. Private authored entities live
  in versioned, ignored `assets/extracted/overrides/{metroid,aria}/entities/`
  JSON documents with stable monotonically increasing IDs and atomic saves.
- Select tool supports direct mouse drag of project entities. The context menu
  also offers Move (click destination) and Delete. Coordinates are 16-pixel
  aligned and bounded to the current native room canvas.
- Separate dashed white borders and `[PROJECT]` labels distinguish authored
  markers from read-only native records. Existing native collision and door
  annotations remain untouched. Creation deliberately does NOT promise gameplay
  execution or a native-ROM encoder.
- Python tests validate round-trip create/move/delete, cross-world identity,
  schema/geometry constraints, symlinks, and non-mutation of original annotations.
- All tests, GTK4 compilation and proprietary-file guard must pass on Debian
  before Auto AI PyPatch commits/pushes this change.

## Patch 0080 - validated native constructor binding for project markers

- Zero and Aria project ENEMY/ITEM/OBJECT markers now select a native definition
  from the matching source catalog instead of accepting invented type strings.
- MZM uses source-derived primary-sprite roles; Aria uses native kind/ID and
  excludes door constructors from the generic OBJECT editor.
- Existing project markers can be rebound through their context menu. The
  project-owned room JSON retains stable ID, room and coordinates, and gains a
  validated reference plus source display name; originals are untouched.
- Catalog absence falls back to a clearly unassigned project marker. No
  sprite preview or gameplay encoder is claimed where none exists.
- ROM-free Python regression tests cover category boundaries, assignments and
  fail-closed wrong-game/wrong-role IDs. GTK4 build/CTest required on Debian.


## Patch 0081 - Aria item controls and native-image-aware selectors

- Native-definition dropdown rows render a 28px image from private, verified
  `assets/extracted/sprite_previews/{aria,mzm}/<native-token>.png` when present.
  Undecoded monster images explicitly show `image-missing-symbolic`, never
  misleading cropped backgrounds or unrelated community/fan sprites.
- Aria lists all documented pickup subtypes (money, consumable, weapons,
  armor/accessories, red/blue/yellow/ability souls), with normal, hard-mode
  and all-souls-found families, even if never placed on a vanilla world map.
- Aria authored items support bounded item ID, two native-sized u16 metadata
  parameters and u8 flags. Existing schema-v1 documents remain readable; item
  settings are optional and validated in both CLI and Python API. These are
  project settings, not a claim of engine export/behavior.
- The optional local-ROM thumbnail decoder only accepts source-verified icon
  pages with an explicit 0x2000-byte header and refuses unknown page wrappers;
  no ROM bytes or generated assets are committed. PNG icons are illustrative
  of item subtype; the item-ID-specific previews are cached privately.
- A complete enemy sprite extraction / OAM compositor is not implemented in
  this patch. Genuine enemy PNGs, when already decoded into the documented
  cache, display in the same dropdown factory.

## Patch 0082 - named Aria item choices and per-record thumbnails

- Dropdown offers individual named US-ROM item/soul records (weapon, armor,
  consumables and four soul types), not merely pickup-category labels.
  Hard Mode / All Souls variants remain visible as separate condition families.
- Names are decoded from the verified owner's ROM text pointer table; native
  constructor subtype and item ID remain separate for schema-v1 compatibility.
- Aria item thumbnails resolve by selected subtype AND item ID, directly in
  GTK4 dropdown rows. Actual monster sprite/OAM decoding remains future work.
- Missing graphics use a neutral diagnostic symbol, not a fake sprite. The
  original ROM, extraction data and untouched native records remain immutable.

## Patch 0083 - authentic Aria graphics wrappers and name controls

- Fixes the v0081 icon extractor: GBA pages use byte-encoded GfxWrapper
  descriptors (raw or GBA LZ10), not a uint32 0x2000 prefix.
- Icon table entries pack 1-based sprite index and +4 palette number;
  palette pages have a four-byte descriptor and multiple 16-color palettes.
  Regenerate private PNG caches using correct source colors and indices.
- Aria string decoder handles known embedded formatting/control opcodes and
  the complete documented USA extended-character range rather than marking
  any formatted item name undecodable. Provides item-name diagnostic counts.
- No attempt to fake enemy sprites, souls without verifiable icons, or
  executable ROM changes. All modified assets remain private and ignored.

## Patch 0084 - Aria item-name CLI and GTK4 widget lifecycle

- Move `scripts.aria_item_details`' `__main__` entrypoint below all native
  decoder functions; direct `python3 -m scripts.aria_item_details` no longer
  crashes with an undefined `item_name` before reporting genuine ROM status.
- Track `Room data` list/status widgets with their document lifetime, including
  closing or detaching their right-side palette notebook page while a room is open.
- Guard popup selection callbacks with weak-tracked child widgets; disconnect
  callbacks when the form is destroyed to prevent stale GTK4 label accesses.
- Add ROM-free execution tests for direct module entrypoint and GTK lifecycle
  source contracts; no item placement or commercial source data is changed.
- GTK4 runtime still requires local testing after Auto AI PyPatch compilation.

## Patch 0085 - color legend and authentic cached room sprites

- Every room tab displays an eight-role color legend, including the decoder status
  of native triggers and the distinction between native and authored entities.
- Enemy and item category toggles now control BOTH their colored outlines and
  native 1D sprite preview PNGs on the room canvas (Cairo nearest-neighbor).
  Original hitboxes, room collision and read-only native annotations remain intact.
- Exact Aria item icons (when decoded locally) are matched using pickup subtype
  plus project item ID or original native placement parameter 0. Conditional
  Aria enemy ID references and MZM PrimarySprite identities resolve private
  preview paths without mixing unrelated enemies and items.
- A per-room bounded, negatively cached Cairo PNG cache avoids re-reading every
  image each frame; reopening or project edits invalidate cache safely.
- Native enemy OAM decomposition is NOT finished. Existing genuine previews are
  shown; an opt-in validated PNG import helper accepts accurately identified
  enemy sprites from the user's legally extracted assets. Missing sprites leave
  the color square visible, without substituting unrelated sprites.
- All original ROM bytes and private copyrighted resources stay untouched.

## Patch 0086 - source dock in place of Explorer and compact room legend

- Retired Explorer: it only duplicated existing GTK navigation buttons and did
  not own any data or editing tool. Its left dock now contains actual persistent
  source tabs: Zero rooms, Aria rooms (immediately adjacent), Global maps,
  Map creation, Object catalog, Events and Cutscenes. ROM visuals and room
  palettes stay in the right dock.
- Kept the center for Open editors and its nested, closable native room tabs;
  permanent source tabs and ephemeral documents retain separate drag groups.
  Source and center panes remain independently resizable. Opening a room while
  in narrow single-pane mode focuses the center editor.
- World badge responds to source-tab and document selection without relying on
  the former Explorer shortcuts.
- Compact, left-aligned, non-expanding colored legend uses short labels and
  small native-color squares; it wraps if needed rather than stretching.
  Original independent overlays, sprite cache and color hitboxes are unchanged.
- Updated layout source tests, including the legacy object-catalog UI check
  to assert that its widget is now constructed in the left source dock.
  ROM source assets and authoring data untouched.

## Patch 0087 - vertical workspaces and selected central view

- Replaces the wrongly horizontal/source-tabs layout of 0086 with one compact
  GTK4 navigation list on the left, listing actual permanent center pages from
  Zero rooms and Aria rooms through Map creation, story tools and Open editors.
- Each selection activates exactly one full-sized central page; center notebook
  tab labels are hidden. Source browser pages are no longer squeezed into the
  narrow navigation column. Navigation also synchronizes on programmatic
  center-page changes and when opening a native room document.
- Right side retains Inspector and Room palettes, removes redundant ROM visuals
  UI and its now-unused static callbacks. Private extracted graphics and all
  actual ROM sprite importers are untouched.
- Earlier 0086 source-layout tests are updated to assert the corrected design;
  a dedicated regression asserts navigation mapping, room focus and removal of
  the ROM visuals page. GTK4/CMake, CTest, all Python tests and proprietary-file
  guard remain validation gates.

## Patch 0088 - exhaustive render audit and shared headless backend

- Added a real decoder audit for all discovered rooms, with bounded area-level
  multiprocessing and structured per-room identities, resource references,
  layer status, expected dimensions, unresolved references, duration and errors.
- Corrected MZM native RLE bounds/tails, source-table tileset aliases and test
  resource paths. Corrected Aria palette resource units as a final row index.
  Current private audit: MZM 331 rooms (330 partial, 1 unsupported, 0 errors),
  Aria 343 rooms (342 partial, 1 unsupported, 0 errors).
- Added `scripts/editor_backend.py`, the stable `fusion_editor_cli` launcher and
  `fusion_map_editor --headless`. Both run without GTK/display initialization,
  use strict command options, JSON/error envelopes, bounded batch validation,
  dry-run behavior and honest unavailable capabilities.
- Both GTK room browsers now run selected-area or complete-world audits through
  the shared backend and retain detailed reports only in ignored private output.
- Rendering caches reuse parsed MZM source/tilesets and decoded Aria resources
  within a worker. Audits exercise the renderer without producing hundreds of
  preview images.
- Native gameplay, full rendering, audio decoding and engine export remain
  unavailable; the capability matrix and CLI reference state these boundaries.
- Validation: all 222 Python tests and all eight CTest targets pass, including
  both no-display native entry points and GTK4 lifecycle coverage. The private
  exhaustive audits above completed with zero decoder errors, and the tracked
  proprietary-file guard passed.

## Patch 0089 - complete current-surface backend parity

- Routed GTK draft creation/listing, global-map placement, native room opening
  and rendering, object inventory, project entity editing, and story validation
  and storage through the shared editor backend. The old feature-specific Python
  subprocess entry points remain compatibility tools, not GTK business logic.
- Added strict headless contracts for draft placement/removal, project entity
  list/inspect/create/move/assign/delete, named catalogs, Aria item parameters,
  story validation/save, project audit, native workroom opening and asset lists.
  Destructive removals require explicit confirmation; dry runs validate without
  persistence; GTK row consumers use bounded TSV from the same backend.
- Added `fusion_native_map_cli`, an internal adapter around the exact C
  `native_map_load/edit/save` core used by GTK. Public `layer-list`, `tile-get`,
  `tile-set` and `tile-fill` operations now provide no-display BG1/BG2 parity
  without duplicating native-map rules. Writes target only ignored overrides.
- Raised the artificial native-workroom axis cap from 128 to 255 cells while
  retaining the 6,144-cell per-layer bound. This opens verified wide/tall rooms
  such as MZM Brinstar room 003 (`19x134`) without weakening allocation bounds.
- Added a functional CMake adapter round trip (read, set, reload, bounds error),
  Python draft/entity/story round trips, confirmation/dry-run contracts and a
  documented GUI/backend/CLI feature matrix. Collision, doors, native entity
  encoding, custom object definitions, graphical event/cutscene authoring and
  audio remain explicitly unavailable rather than simulated.
- Validation: all 230 Python tests and all nine CTest targets pass, including
  the no-display native entry points, GTK lifecycle tests, native-map adapter
  round trip and proprietary-file guard. The 0088 exhaustive private audit
  remains MZM 330 partial + 1 unsupported and Aria 342 partial + 1 unsupported,
  with zero decoder errors.

## Patch 0090 - unified project collision, doors and transitions

- Upgraded the existing per-room private entity document to the unified
  `metroidvania.project-room-data` version 2 schema instead of creating separate
  sidecar files. It now owns entity markers, sparse semantic collision cells,
  project doors and transition references. Version 1 documents migrate in
  memory and remain untouched until an explicit save.
- Added strict collision list/get/set/fill/clear/validate operations. Zero
  Mission uses 16px project cells and Aria uses 8px project cells; bounds,
  duplicate coordinates, semantic types and destructive confirmation are
  validated. These are project semantics, not rewritten native collision bytes.
- Added project door list/inspect/create/update/delete and transition
  list/create/update/delete/validate operations, plus `door-link`. Door geometry,
  IDs, types, facing, source uniqueness and native target-room existence are
  checked. Non-zero destination-door IDs remain explicitly unverified without
  target project geometry, and both engine adapters remain unavailable.
- GTK room documents load the same backend TSV, render project collision above
  the immutable native diagnostic overlay, and expose context actions to set or
  clear collision cells and create/view/delete project doors. Full GTK property
  and transition-link forms remain pending; the complete operations are already
  available through the shared headless backend.
- Added functional migration, collision, door, dependency, persistence and CLI
  round-trip tests. Validation: all 234 Python tests and all nine CTest targets
  pass, including GTK lifecycle/no-display entry points and the proprietary-file
  guard. No ROM, native annotation or extracted proprietary asset is modified.

## Patch 0091 - show native Zero Mission doors by default

- Fix GTK room documents opening with every native annotation overlay hidden.
  Enemy, item, object, door and event markers are now visible on first open;
  walls and unknown records stay opt-in, and undecoded triggers stay disabled.
  Users can still independently hide/show any enabled category.
- Ignore the real `DOOR_TYPE_NONE` MZM table sentinel instead of drawing a
  non-door at (0,0). Preserve original door indices for destinations.
- Add an actual GTK toggle interaction regression and a synthetic native MZM
  door-table parser test. No ROM, extracted image or private override is changed.
- Native doors are purple diagnostic outlines with D indices; decoding and
  displaying their authentic graphical hatch sprites is still separate work.

## Patch 0092 - Zero Mission original world-grid reconstruction

- Corrected the native GBA minimap occupancy check to mask palette and flip bits
  before comparing against tile ID `0x140`; preserves full raw tile words.
- Native door/sprite evidence may identify an otherwise unowned original minimap
  case only when exactly one source-room family matches. Ambiguous cases remain
  unassigned instead of inventing room membership.
- Added a separate source-derived native-door index in original minimap
  coordinates, without treating doorless sentinels as actual doors. GTK shows
  native door markers in a dedicated toggleable global-map overlay.
- MZM global-map previews are now individual 240x160 playable screens cropped
  from BG1 after the genuine 32px Clipdata guard, downsampled to native-aspect
  60x40 thumbnails, not whole-room images stretched over arbitrary shapes.
- Aria multi-cell geometry and previews remain unchanged.
- Fixed prefetch skipping the first map area after a zero-budget indexing pass;
  bounded asynchronous batches continue while new previews are generated.
- Added ROM-independent mapping/preview regressions; run the local CMake and
  full test suite before pushing. This does not complete animated graphics,
  BG0/BG3 blending or full gameplay rendering.

## Patch 0093 - compile and legacy minimap regression repair

- Correct GTK global-map preview prefetch callback to use its existing `out` subprocess output buffer, fixing two undeclared `output` references.
- Update the earlier minimap-source regression to expect provenance 1 for an original ROM-backed room anchor, consistent with patch 0092.
- No changes to native minimap coordinates, private renders, ROMs or project data; validate with CMake, CTest and the proprietary guard.

## Patch 0096 - verified Zero Mission door transitions in global map

- Escape separator pipes in compound native door type labels, and extend
  the ignored private `world_overview/mzm_doors.tsv` with destination
  room and destination door only for native intra-area door-table targets.
  Out-of-area links, invalid indices and sentinel targets remain unresolved.
- Keep backward compatibility for earlier six-column TSV indexes; regenerate
  after import to populate target-room navigation.
- Add a read-only, expandable GTK door inspector for the selected MZM room.
  Open verified destination rooms directly in the native workroom editor;
  never create guessed area links or modify original ROM data.
- Native map door labels now include known destination IDs in their tooltips.
  Rendering authentic hatch sprites remains separate work.

## Patch 0097 - native MZM doors in individual rooms

- Fix the native room annotation TSV parser: the details field contains the
  original door bitflag expression with a literal `|`. GTK previously split
  such lines into eleven fields and discarded the entire native door.
- Preserve the full native expression by splitting only the first nine `|`
  separators, including for already-extracted private annotation files.
- Add a real GTK importer test with both original doors of a synthetic
  Brinstar 010 room (the source table has two corresponding entries).
- Keep optional global door-number badges switched off by default. The
  global map's verified door-navigation panel remains unchanged.
- Native room door markers are still read-only diagnostics; authentic
  animated hatch sprite rendering remains future work.

## Patch 0098 - original native Zero Mission hatch BG1 graphics

- Decode the original shared 0x400-based metatile table and common graphics/
  palette from the owner's verified US ROM or ignored extracted raw resources.
  Generate private transparent hatch PNGs for all five hatch types and both
  orientations, with a separate Mothership graphics family.
- Identify real hatch type and facing from native Clipdata (not guessed colors),
  using the exact engine values. Place each hatch one block beside its door
  transition as ConnectionLoadDoors does. Unknown entries remain read-only
  diagnostic boxes, with no fabricated hatch image.
- Display original 16x64 metatile graphics at actual room scale via the
  existing cached sprite-preview surface; no gameplay or ROM mutation.
- Add synthetic pixel/PNG and Clipdata identity regressions.
- Opening/closing animation and event-dependent palette switching remain future
  work; CMake/CTest and private-ROM guard still required locally.

## Patch 0099 - native Zero Mission hatch animation preview

- Reconstruct the original hatch opening/closing BG1 metatile indices from
  `ConnectionUpdateHatchAnimation` (four opening frames, three intermediate
  closing frames, return to the closed native tilemap).
- Export private ROM-derived PNG frames using the same verified common source
  graphics as the closed hatch, without inferred animation art.
- Add an optional, read-only `Animate hatches` toggle to MZM workrooms only.
  The GTK animation uses a short preview loop, not gameplay/event simulation;
  the timer is cleaned up when a room closes.
- Keep closed hatch artwork as fallback for missing original frames; preserve
  Aria workrooms, map overview and authored collision/door overrides.
- Add native index/pixel regression tests and a GTK document-timer test.

## Patch 0100 - native Zero Mission hatch lock/event preview

- Decode the pinned original hatch event tables by *hatch slot*, not the global
  door number. Add event conditions to read-only native door metadata only
  while the prior hatch slot ordering can be verified from original Clipdata.
- Add GTK4 per-room manual preview modes: original, all named native events
  off/on, fully open frame, temporary lock and permanent security lock.
  BEFORE and AFTER conditions follow the decomp's event-bit polarity; the
  security hatch graphic uses native common metatile 0x2A.
- Only source-derived graphic PNGs may be displayed; unsupported frames fall
  back to the original native closed hatch. A manual scenario suspends the
  animated preview; Aria and the global map remain unchanged.
- This is a visual, hypothetical all-events snapshot, not a saved game state.
  More complete event engine state/door runtime mutation is future work.
- Source-only Python regressions cover event flags and security tile indices.

## Patch 0101 - Aria original screen-transition geometry

- Use 256x256 screen/map blocks rather than the 240x160 LCD viewport.
- Preserve native screen identity. Draw approximate edge markers only where
  room extent and boundary can be determined; never assert pixel precision.
- Invalid native coordinates are inspector-only, not fabricated exits.
- Aria-only import, no ROM edits or changes to Zero Mission.

## Patch 0102 - shared project-door property editor

- Replace the read-only project-door editor shell with a dedicated GTK4 form
  for label, geometry, semantic type and facing in both Zero Mission and Aria.
  The same validated headless backend implements door-update and linking.
- Choose an existing native target world, area and room and save project-owned
  transitions; edit or remove an existing link. Original source doors remain
  immutable and cannot open a project-edit form.
- All edits stay in the ignored versioned project room document; cross-world
  targets are validated by the existing backend, not assumed playable.
  `target_door_id=0` explicitly means unspecified.
- Scope: target room is chosen in the form, not by clicking the global map.
  True map-click destination picking, bidirectional link assistance and engine
  adapters remain follow-ups. The patch does not alter existing map layout,
  tileset renderer, ROM, native annotations or story data.

## Patch 0104 - native door project overrides

- Context Edit on an original Zero Mission or Aria DOOR now creates/reuses a
  project-owned override bound to the original annotation's verified index,
  variant and type. The backend validates that identity against the private
  imported room annotation TSV and refuses arbitrary/invented source geometry.
- The unchanged original remains in the ignored private source annotation file;
  its canvas marker is temporarily superseded by the editable override.
  Deleting the private override reveals the source door again.
- Existing project-door edit and cross-world destination form are reused;
  legacy version-2 room documents and project doors without native source
  references remain compatible. This is not a native engine door encoder.
- Real Aria door sprite graphics and frame-accurate transition positions still
  require separate native research; screen-boundary anchors remain approximate.

## Patch 0106 - native room picker for project door targets

- One-shot, cancellable Global maps room selection from the shared GTK door form.
  Both Aria and Zero Mission use verified original map ownership; unknown tiles,
  private drafts and unassigned cells are refused.
- Selection fills target world/area/room only. Target project door ID and spawn
  point reset to zero (safe unspecified values); explicit Save destination still
  runs existing backend validation before persisting. Nothing is saved on click.
- Closing a form abandons its callback; closing a map cancels pending
  selection. Weak pointers prevent dangling map/form/workspace references.
- No ROM, project data format, runtime engine, minimap grouping or room tile
  rendering changes. Bidirectional linking is not automatic.

## Patch 0107 - Grab entity placements

- Added an explicit GTK Grab tool (M) for visible native and authored entities,
  objects, items, and doors. Zero Mission snaps to 16px, Aria to 8px.
- Native placements are private visual authoring overrides saved in ignored
  GKeyFile documents under `assets/extracted/overrides/*/annotation_positions`;
  original ROMs, native placement descriptors and transition identities do not
  change. A native door already adopted by the project remains shadowed.
- Project entities use the existing validated headless backend; project doors
  use `door-update`. Aria's validator now permits 8px project entity positions.
- The feature does not yet implement engine-native entity re-encoding.

## Patch 0108 - Aria BG3 previews

- Export exact, static RGBA BG3 imagery from verified local Aria US ROM
  using the existing decoded native 4bpp text-layer metadata, graphics and
  palette. The room editor composites it behind editable BG2/BG1.
- Keep original transparency (PNG); reject incomplete, oversized or
  unsupported affine/8bpp BG3 rather than fabricating source imagery.
- Original Zero Mission BG3 extraction is unchanged and remains partial.
- Parallax, original blending priorities, room camera and animated
  backgrounds still need native runtime reconstruction.

## Patch 0109 - deferred GTK room changes

- Native annotation-position overlays and shared project room data (entities,
  doors, transitions, collision) are staged independently per open room tab.
- The native ROM and persisted private overrides remain unchanged until the
  same diskette / Ctrl+S used for metatiles explicitly commits changes.
- The tab gets an unsaved star on validated mutations; Save-and-close commits,
  Discard removes staging files, Cancel preserves pending edits.
- GTK Grab has a symbolic hand icon instead of a mouse.
- This is a static editor overlay, not a source-ROM encoder. Per-file writes
  are atomic but a multi-file save is not a fully transactional commit.

## Patch 0112 - chronological shared room undo

- One per-tab chronological Undo/Redo history captures a source-independent
  tile map and both validated temporary project documents (room JSON and native
  positions INI). Tile painting, Grab and successful backend authoring commands
  can be reversed in their actual execution order, limited to 16 snapshots.
- Both stage files retain absence/presence, so undo of the first project edit
  removes the temporary draft, and redo restores it without ROM changes.
- A snapshot matching the last saved/opened state removes the dirty star.
  Save still controls all durable override writes; Undo/Redo never commits.
- GTK/Xvfb regression exercises Tile -> JSON -> Undo twice -> Redo twice.
  Native source encoders and atomic multi-file disk commits remain pending.

## Patch 0113 — fixed room toolbar and collision authoring

- Room controls use two fixed-width, horizontally scrollable icon strips instead
  of the responsive wrapping tool grid. Icons remain compact across window sizes.
- Wall (W), Water (U) and Air (A) paint private semantic collision strokes;
  Zero Mission uses 16px cells and Aria uses 8px cells. Drag previews never write
  to disk, and each stroke is one backend mutation and one shared history entry.
- Water and Air are explicit version-2 project collision semantics; Air overrides
  a native wall as passable without altering source ROM collision data. Clearing
  the project collision entry in the context menu restores native provenance.
- No native physics adapter or ROM collision writer is implemented yet. Existing
  imported collision images remain diagnostic-only, not destructive edit targets.

## Patch 0116 — saved project connections on global maps

- A read-only backend `connection-list` inventories saved project room doors
  (not native door indices or per-tab uncommitted stage files); identifies
  reciprocal, one-way, interworld, missing and invalid connections.
- The GTK original world atlas can overlay project edges on actual mapped room
  footprints, with selected-room default and an optional all-links view.
  Cross-world / cross-area connections appear in the destination inspector,
  not as fabricated lines between unrelated world coordinate systems.
- The map's original GTK grid, occupancy and double-click behavior stay in
  place behind a click-through Cairo overlay; the inspector can navigate to
  verified destination rooms in their original maps.
- No writes, gameplay engine or source ROM mutation. Refresh by reselecting a
  room or toggling the connection visibility after saving a room editor tab.

## Patch 0114 - exact saved project door targets

Patch 0114: target project door picker. Lists only previously saved project doors
in a ROM-verified target room. The chosen ID is checked against the saved
target geometry by the headless backend, and no native-only door index is
silently reused as a project door ID. The target chooser does not change the
room until the user explicitly saves the destination and then the room.
No automatic reciprocal link or native engine encoder is implemented.



## Patch 0115 - staged reciprocal door links

- Added a read-only backend/CLI `door-return-plan` that verifies a previously
  saved forward link, the exact saved project doors at both ends, and a vacant
  destination door before producing a reverse-link proposal.
- Added `Prepare return link` in the GTK project door form. The target room is
  opened or reused and the reverse link is created **only in that room's
  per-tab private draft**, with its own dirty star, undo/redo, and diskette.
- The source room must be explicitly saved before preparing a return link.
  The two room saves are separate, not a global transaction. No ROM or saved
  destination document is mutated by planning or by GTK staging.
- Original native-only door IDs are not substituted for project door IDs;
  gameplay and cross-engine transition adapters remain unavailable.

## Patch 0117 - freehand collision brushes and responsive global map

- Wall and Water now paint continuous freehand cells, including interpolated
  cells when the pointer moves quickly. One complete gesture is validated and
  staged as one backend mutation and one shared Undo entry.
- Wall is rendered red and Water blue. Air is a visible eraser preview while
  dragging, then becomes an invisible explicit passable override that masks
  both project collision and the read-only native collision preview below it.
- The global map no longer creates a GTK widget for every empty map case. One
  Cairo background owns empty-cell drawing and context targeting, while only
  occupied room segments remain widgets.
- Removed the artificial 720x520 scroll padding. The map scroll range now ends
  at real map bounds, and the crowded control row uses a wrapping GTK flow box
  so narrow windows form additional rows rather than overflowing horizontally.
- Native ROM collision and map data remain immutable; engine collision adapters
  are still unavailable.

## Patch 0118 - one-way platform brush and collision cursor

- Added a green Platform tool and `P` shortcut to both native workroom editors.
  It authors the existing validated `one_way` project collision semantic at
  16px resolution in Zero Mission and 8px resolution in Aria.
- Wall, Platform, Water and Air now show a color-coded cell outline under the
  pointer before painting. The cursor follows Aria half-tile cells correctly.
- Compact tool buttons expose stable semantic names for GTK automation and
  accessibility lookup instead of relying on theme-dependent icon identity.
- Fixed tool-toggle ordering so the previously active Pencil cannot reactivate
  itself while Wall, Platform, Water or Air is being selected.
- Platform strokes use the same bounded continuous interpolation, staging,
  Undo/Redo and explicit Save workflow as the other collision brushes.
- This remains engine-neutral project data; no ROM or native physics encoder is
  implied.

## Patch 0119 - cross-world collision capability matrix and full brush suite

- Verified static pass-through platforms in both native formats. Zero Mission
  exposes `CLIPDATA_TYPE_PASS_THROUGH_BOTTOM`; Aria's collision byte represents
  a jump-through platform as a top without solid sides/bottom or damage effect.
  Moving and crumbling platforms remain object records, not static collision.
- Added Hazard, rising Slope and falling Slope tools to both workroom modes.
  Their `D`, `R` and `T` shortcuts, continuous gesture interpolation, live
  previews, backend validation, shared Undo/Redo and explicit Save behavior
  match the existing collision brushes.
- Added `collision-capabilities --world=<world>`. The backend now reports all
  seven shared project semantics, the 16px MZM / 8px Aria resolution, and the
  exact or family-level native format reference verified for each semantic.
- Native parity and native writing remain separate states: every listed source
  format counterpart is verified, but `native_encoder` is still `unavailable`
  for both worlds and source ROM data remains immutable.
- Functional tests author every collision type in both world schemas and query
  both capability matrices. GTK tests activate the complete tool strip.

## Patch 0120 - atomic project entity property editor

- Added the validated `entity-update` backend operation. One request updates a
  project enemy, item or object label, snapped position, native catalog
  reference and optional typed Aria item fields without partial mutations.
- The project entity GTK window now edits names and X/Y coordinates instead of
  only reassigning a native type. Zero Mission coordinates snap to 16px; Aria
  coordinates snap to 8px. Both remain bounded to the current room.
- Opening **Enemy editor**, **Item editor** or **Object editor** on a
  project-owned marker now opens this complete form directly. Native ROM
  records still open the immutable inspector and retain their disabled override
  action.
- The form is vertically scrollable for narrow displays. Source-derived icons,
  named catalogs and Aria item/soul ID, parameter and flag fields remain in the
  same workflow, with one shared Undo entry and explicit room Save.
- Added ROM-free tests for MZM and Aria field updates, alignment rejection,
  atomic failure behavior, CLI round trips and the shared GTK contract.

## Patch 0121 - project event and trigger region authoring

- Extended the existing per-room project document to version 3 instead of
  creating another sidecar. Version 1 and 2 documents migrate in memory and
  remain untouched until an explicit Save.
- Added bounded event and trigger regions with a typed activation, action,
  stable action reference and one-shot flag. Zero Mission uses its 16px grid;
  Aria uses its 8px grid.
- Added complete backend and CLI list/inspect/create/update/delete operations,
  strict validation and an explicit unavailable engine-adapter status.
- Added room overlays, right-click creation, a responsive property form, Grab
  movement, context inspection/deletion and shared staged Undo/Redo/Save for
  both workroom modes.
- Native event/trigger records and both source ROMs remain immutable. Native
  trigger decoding and gameplay execution are still pending.
- Added ROM-free schema migration, atomic failure, two-world CLI, GTK lifecycle
  and source-contract coverage.

## Patch 0122 - typed event action reference validation

- Added `event-validate` with structured and TSV diagnostics for every saved
  room event, while keeping both gameplay adapters explicitly unavailable.
- New and edited actions now require a matching typed reference: timeline or
  cutscene for story, project entity for spawn, another project event for
  toggle, project transition for transition, or a stable checkpoint key.
- References resolve against the current room plus validated timeline and
  cutscene sources. Missing targets and event self-references are rejected
  before the staged room document can replace its saved version.
- Entity, transition and event deletion now refuses to orphan an existing
  spawn, transition or toggle action reference.
- Full project validation now audits every saved event reference. The GTK form
  documents the accepted prefixes, shows a local rejection state and retains
  the existing shared Undo/Redo/Save workflow in both workroom modes.
- Added two-world CLI coverage, atomic persistence checks and resolution tests
  for every action family without requiring either commercial ROM.



## Patch 0124 — validated deterministic project-room export and SDL3 loader

- `room-export` reads **saved** private project-room version-4 documents and
  validates their structure, typed event references and target transitions.
  It explicitly ignores GTK unsaved staging tokens; close/Save still controls
  what is exportable. `--dry-run=true` computes the intended content-addressed
  package without writing anything.
- Two immutable, content-addressed files under ignored `assets/extracted/exports/`:
  `room.json` (complete project-authored room data, collision, doors, entities,
  transitions and event conditions) and `preview.tsv` (bounded, pixel-coordinate
  geometry subset, no labels or proprietary image/ROM/tileset bytes).
- The `fusion_room_package_viewer` SDL3 utility verifies and displays the preview
  with colored rectangles. `--check` parses it without a display server.
  It is a **format-consumption proof**, not gameplay and not a native render.
- Export does not package BG1/BG2 tile arrays or ROM-derived assets. Those need
  a separate lawful, validated resource pipeline. Native encoders and both game
  runtime adapters remain unavailable.

## Patch 0123 - multiple typed event conditions

- Upgraded the shared room document to version 4. Version 3 events migrate in
  memory to an unconditional `all` group and are rewritten only by explicit
  Save; no additional room-side document was introduced.
- Added up to 16 ordered conditions per event, combined through `all` or `any`,
  with individual negation. Supported targets cover story flags, other events,
  project entities, project transitions and stable checkpoint keys.
- The GTK form now exposes add/remove condition rows, contextual reference
  examples, an All/Any selector and Not toggles in both workroom modes.
- Backend/CLI create, update, list and validate operations carry the complete
  condition group. Missing, self-referential and duplicate conditions are
  rejected atomically, and deletion guards prevent orphaned dependencies.
- Empty `all` is the explicit unconditional state; empty `any` is rejected to
  avoid ambiguous future runtime behavior. Both gameplay adapters remain
  unavailable and no native ROM event data is modified.
- Added version-3 migration, two-world CLI round trips, all condition-family
  resolution, dependency protection and GTK source-contract coverage.

## Patch 0125 - private native BG1/BG2 overlays in SDL3

- The project-room SDL3 preview automatically looks for locally extracted
  Zero Mission BG1/BG2 BMPs in the existing ignored private preview directory.
  `--bg1`, `--bg2` and `--no-auto-bg` permit explicit control; no native
  images, tiles, audio or maps enter `room.json` or `preview.tsv`.
- Exact pixel dimensions must match the saved project-room package; otherwise
  an automatic image is skipped with a diagnostic, while an explicit wrong BMP
  is refused. No stretched or guessed source-to-project geometry.
- Press `1` for BG1, `2` for BG2, `0` for geometric view, `C` to toggle
  collisions, `M` for entity/door/event markers; BMPs use nearest-neighbor
  display and the same coordinates as the project overlay.
- The existing Zero Mission decoder supplies partial, *authentic* private
  source graphics. BG1/BG2 are deliberately shown separately: compositing,
  transparent native priority layers, BG3 and animations are not implemented.
  This remains a diagnostic viewer and is not gameplay.

## Patch 0125d - native preview defaults to original data only

- `python3 -m scripts.native_room_viewer_demo --area Brinstar --room 33`
  now uses the original locally decoded BG1 and an **empty** geometry
  interchange for SDL3; it no longer invents collision cells, entities or doors.
- The old illustrative shapes are available only via `--demo-overlays`.
- Use `--launch` to open the newly generated, exact (hash-named) preview
  immediately without accidentally choosing an older synthetic export.
- The empty work document lives exclusively under ignored
  `assets/extracted/native_demo_0125/`; it is not a native ROM reconstruction.
- Authentic native collision/object placement, missing palette rows, BG0/BG3,
  animation and GBA layer priority/compositing are still NOT rendered in SDL3.
  We do not mistake a partial native BMP for the complete game framebuffer.


## Patch 0126 - transparent indexed-color BG1/BG2 diagnostic composite

- MZM BG1 and BG2 retain authentic decoded RGB diagnostic previews. An optional
  per-pixel visibility buffer now records the GBA 4bpp transparent palette index
  zero separately from decoded opaque pixels and missing raw resources.
- When BG1 and BG2 have matching source dimensions, a private diagnostic BMP
  `*_bg12_composite.bmp` displays decoded BG1 opaque pixels over decoded BG2
  pixels. Unknown pixels remain black and are counted, not reconstructed.
- The independent SDL3 room viewer auto-discovers this local diagnostic and
  selects it by default when present; `3` selects the composite, `1`/`2` the
  individual layers, `0` the data-only view, `C`/`M` project overlays. Explicit
  `--composite` inputs require exact dimension equality.
- Layer ordering here is **only a diagnostic convention**: original per-room GBA
  BG priorities, scrolling, BG0/BG3, color effects, sprites, common graphics
  and native entity/door placement are not reconstructed and not claimed.
- ROMs and derived BMP pixels are never added to committed project exports.


## Patch 0126b - SDL3 native overlay parity

- The SDL3 room viewer accepts a separate, private, opt-in `--native-source`
  sidecar generated from original MZM Clipdata and native annotations. Its
  identity and geometry must match the project preview; a mismatch is rejected.
- Pressing C shows both authored collision and real native Clipdata diagnostic
  rectangles; M shows real native annotation categories and authored markers.
  Both overlays now use translucent fills and colored outlines matching GTK4.
- Raw Clipdata IDs are preserved and colored diagnostically. Red does not imply
  all native cells share the `solid` game behavior. Native metadata is never
  bundled into room.json or the content-addressed project preview.
- If original data are unavailable or the native collision geometry does not
  align with BG1, the generator reports the limitation and does not invent
  replacement geometry. Exact full native gameplay rendering remains pending.

## Patch 0127 - standalone experimental native BG3 diagnostic

- `scripts.native_room_viewer_demo --decode-bg3` opts in to the existing
  experimental BG3 text-map decoder. Default no-ROM fixture runs stay pure.
  Unavailable resources are reported without inventing pixels.
- SDL3 `4` presents the private 256x256 or 256x512 BG3 tilemap at its **own**
  dimensions; this is not stretched over BG1/BG2. `1`, `2`, `3`, `0` preserve
  their previous meanings, and `C`/`M` overlays remain visible in room-space
  modes but are deliberately hidden in independent BG3 view.
- `--bg3` allows an explicit bounded local BMP. Automatic lookup is restricted
  to audited MZM area/room names. Headless `--check` checks BG3 independently
  of project-room dimensions. No ROM-derived pixels enter room.json or TSV.
- BG3 character origin/palette decoding is experimental; hardware priority,
  per-frame scrolling, color effects, BG0 and full GBA compositing remain pending.

## Patch 0182 - canonical private asset roots and Samus runtime pipeline

- Added one canonical path contract for new private Zero Mission, Aria and
  shared outputs. The general importer now writes raw blocks and basic Samus or
  Soma frames below their world root, with its manifest under
  `shared/manifests/`.
- Added a deterministic private-tree audit with source-reference accounting and
  exact SHA-256 duplicate measurement. The initial canonical-root audit found
  2,286 duplicate groups and 124,191,011 potentially reclaimable bytes. This is
  an inventory, not permission to coalesce path-sensitive room previews.
- Replaced the numbered Samus bundle producer with the stable
  `scripts.mzm_samus_pipeline` entry point. Its canonical content-addressed
  runtime output contains 580 indexed sequences and 1,982 unique BMP objects,
  preserving native durations and five explicit suit categories without
  inventing Varia or Gravity variants.
- Updated the SDL3 Brinstar 033 shortcut to load the canonical library. The old
  top-level runtime bundle was verified equivalent and moved to ignored local
  `legacy/extracted-assets/samus-runtime-bundle-v1/`, where it remains
  recoverable. Historical composition directories still used as intermediate
  inputs were not removed.
- Regenerated both games' raw blocks and basic character frames in canonical
  locations, then verified all 3,969 replacements byte-for-byte before moving
  the old `raw/`, `sprites/` and root catalogue into the ignored legacy
  archive. The unreferenced CLI demo cache was archived as well; user ROMs,
  editor staging data and active room caches were not touched.
- Added ROM-free tests for canonical importer paths, exact duplicate accounting,
  symlink refusal, idempotent object storage and the absence of new historical
  bundle output. No ROM-derived file is tracked.
- Validation on 2026-10-10: the warning-clean Debug build succeeded, CTest
  passed 13/13, the Python suite passed 406/406, and the proprietary-file guard
  passed. `fusion_room_runtime --room brinstar_033` loaded the 304x224 room,
  204 native collision records and a safe grounded spawn under SDL's dummy
  video driver; it remained live until the three-second smoke timeout.

## Patch 0183 - consolidate all Samus private working directories

- Removed every `samus_*` directory from the root of `assets/extracted/`.
  Required inputs now have semantic names below the canonical Samus tree:
  `intermediate/body`, `intermediate/composed`, `intermediate/special`, and
  `metadata`. The SDL runtime continues to consume only `runtime/`.
- Updated the body exporter, composition producers, special-pose producer,
  cannon research tools, sidecar generators and compatibility catalogue/index
  tools so future runs cannot recreate patch-numbered root directories.
- Archived the superseded cannon, incremental composition, special diagnostic,
  basic runtime-preview and duplicate catalogue directories under ignored
  `legacy/extracted-assets/`; no generated user data was deleted.
- Rebuilt the canonical library after migration. Its 580 sequences, 2,612
  indexed frames and 1,982 unique BMP objects are unchanged, and the complete
  runtime index retains the exact pre-migration SHA-256
  `9ce35be51ca94495f635ad55841676acdc1fa460fae774e0534d2ebdd337231b`.
- The post-migration deterministic audit snapshot contains 13,042 files
  (1,314,411,010 bytes), reports no legacy Samus root, and finds 2,269 exact
  duplicate groups with 123,555,917 reclaimable bytes. Most remaining
  duplication is within active room previews or between reproducible
  intermediates and the runtime object store, so it is not removed blindly.
- Validation on 2026-10-10: CTest passed 13/13, the Python suite passed 407/407,
  the proprietary-file guard passed, and Brinstar 033 remained operational with
  its 204 native collision records after the source-cache migration.

## Patch 0184 - automated Samus preparation and semantic runtime registry

- `python3 -m scripts.mzm_samus_pipeline` is now the complete default pipeline.
  One command verifies the ROM, decompilation inputs and reference ELF, resolves
  symbols and palettes, rebuilds all body/composed/special sources, deduplicates
  the runtime object store and emits both runtime tables. `--bundle-only` is the
  explicit prepared-cache mode.
- The real full run resolves and exports 500 body animations, 42 cannon/body
  compositions and 38 special-pose compositions, producing the unchanged 580
  sequences, 2,612 indexed frames and 1,982 unique BMP objects in 4.7 seconds
  on the current workstation.
- Added a generated, deterministic 408-row semantic registry for 27 actions,
  five requested suit modes, both facings and applicable aim directions. All
  rows reference an exact existing catalogue key. Its report exposes 22
  Suitless equipment-action combinations absent from native data instead of
  manufacturing replacements; Varia/Gravity palette fallback is explicit.
- The SDL3 runtime validates the semantic map against the complete library and
  rejects unknown keys or duplicate selectors. Implemented input/movement states
  now select through the registry rather than constructing animation names.
  Turning, skidding, landing and spin startup preserve one-shot native timing
  with priority-based interruption; spin/Space Jump/Screw Attack keep priority
  over generic MidAir. F6 continues to expose every raw catalogue entry for
  mechanics that are not implemented yet.
- Added `--check-animations` for display-free registry validation plus ROM-free
  orchestration, semantic-map and one-shot timeline regressions.
- Validation on 2026-10-10: CTest passed 14/14, the Python suite passed 409/409,
  and the proprietary-file guard passed. The headless registry check accepted
  all 580 sequences and 408 bindings; Brinstar 033 loaded the same registry and
  204 native collision records, then remained live through the smoke timeout.

## Patch 0185 - physical Morph Ball and Wall Jump states

- Added an actual Morph Ball gameplay state to the SDL3 room runtime. X on the
  keyboard or the gamepad West button now toggles it, switches to a shorter
  collision box without moving Samus's feet, and selects native morph, rolling
  and unmorph sequences through the semantic registry.
- Unmorphing performs a full standing-clearance query before changing the
  hitbox. A blocked request leaves Samus safely morphed instead of intersecting
  a ceiling. Suitless preview mode rejects Morph Ball because the source
  catalogue contains no corresponding native sequences.
- Added collision-driven Wall Jump activation for airborne spin jumps. Contact
  is sampled independently on each side, the launch faces and travels away from
  the wall, and a short steering lock preserves the provisional impulse. The
  native one-shot wall-jump pose can interrupt spin startup at equal transition
  priority.
- Added ROM-free regression coverage for foot-preserving hitbox contraction,
  blocked and successful expansion, and left/right/ambiguous wall contact. The
  10-pixel ball height, launch speed and steering interval are explicitly
  provisional until original Zero Mission movement constants are verified.
- Validation on 2026-10-10: the warning-clean Debug build succeeded, CTest
  passed 14/14, the Python suite passed 409/409, and the proprietary-file guard
  passed. Brinstar 033 loaded 204 native collision records plus all 580 indexed
  sequences and remained live through the three-second dummy-video smoke test.

## Patch 0186 - collision-driven ledge traversal

- Added a geometry-based ledge query that finds a solid-to-air corner beside
  the upper body without assuming a metatile grid. A grab is accepted only when
  both the hanging volume and the complete supported standing destination are
  collision-free.
- Falling toward a valid corner now snaps into a stationary hanging state and
  selects the native ledge loop. The approach direction must be released before
  forward can pull onto the platform, preventing the held grab input from
  skipping the hang. Jump selects the separate pull-up sequence; crouch or the
  direction away from the ledge drops with a short re-grab delay.
- Pull transitions place Samus only at the prevalidated standing destination
  and freeze motion for the exact 60 Hz duration read from the active semantic
  animation row: currently 12 ticks forward and 9 ticks upward. Morph input is
  rejected while hanging, so the two physical forms cannot overlap.
- Added ROM-free regressions for left and right ledges, exact snap/destination
  coordinates, invalid sides and a low-ceiling rejection case. Corner sampling,
  drop impulse and re-grab timing remain provisional until the original Zero
  Mission player constants and pose anchors are verified.
- Validation on 2026-10-10: the warning-clean Debug build succeeded, CTest
  passed 14/14, the Python suite passed 409/409, and the proprietary-file guard
  passed. Brinstar 033 loaded its 204 native collision records and the complete
  580-sequence/408-binding registry, then remained live through the three-second
  dummy-video smoke test.

## Patch 0187 - native jump echo and damage lifecycle

- Reproduced the `SamusEcho` control model verified in the pinned Zero Mission
  decompilation. The runtime records a 64-position history at 60 Hz, activates
  on fast upward airborne movement at the native `80/192` launch-speed ratio,
  refreshes for six ticks and draws one old position while cycling four
  distance-two samples. Ledge grabs immediately clear the echo as in the source.
- The SDL copy uses the current native body frame and a translucent violet
  modulation. Timing, history and selection are source-derived; the exact OBJ
  palette-bank-1 colors and body-only composition remain an explicit pipeline
  task rather than being represented as finished fidelity.
- Added a 99-energy diagnostic health state reachable with H while native
  entities are absent. A hit subtracts 20, preserves the source's 48-tick
  invincibility window, flashes the rendered sprite and drives a 13-tick hurt
  state with the mapped knockback animation. The vertical launch ratios derive
  from the native grounded and airborne hurt velocities; horizontal recoil is
  still provisional.
- Zero energy now interrupts every lower-priority transition, disables player
  control and holds the native 92-tick death sequence on its final frame. Enter
  resets health and all transient movement/form states at a collision-safe
  spawn without restarting the process.
- Added ROM-free regressions for history warm-up, four-position cycling, echo
  expiry, damage immunity, energy clamping, persistent death and health reset.
- Validation on 2026-10-10: the warning-clean Debug build succeeded, CTest
  passed 14/14, the Python suite passed 409/409, and the proprietary-file guard
  passed. Brinstar 033 loaded all 204 collision records and the complete
  580-sequence/408-binding registry, then remained live through the three-second
  dummy-video smoke test with the new controls initialized.

## Patch 0188 - remove obsolete fixed-sprite runtime loaders

- Removed the runtime's historical `--samus-sprites`, `--samus-composed*` and
  `--samus-special` loaders plus the heuristic key builder that predated the
  semantic registry. They read archived patch-numbered caches that the
  canonical pipeline no longer produces and had no remaining consumers.
- The runtime now accepts Samus graphics only through the validated
  `--samus-assets` bundle or an explicit `--samus-library`/`--samus-map` pair;
  supplying only one half of the pair is rejected.
- Validation on 2026-10-10: warning-clean build, runtime/room CTest subset
  passed, and Brinstar 033 still loaded 204 native collision records plus the
  complete 580-sequence/408-binding registry in the dummy-video smoke test.

## Patch 0189 - native Samus pose controller

- Added `src/runtime/mzm_samus.{h,c}`, an SDL-free controller ported from the
  pinned decompilation. It reproduces the native pose handlers and graphics
  loops for standing, running, turning, shooting, crouching, MidAir, midair
  turning, landing, spin start, spinning, wall-jump start, Space Jump, Screw
  Attack, Morph Ball (morphing, idle, rolling, unmorphing, midair and bounce),
  ledge hanging and both pulls, hurt and dying, plus the carries performed by
  `SamusSetPose`, `SamusSetMidAir`, `SamusSetLandingPose` and
  `SamusChangeToHurtPose`.
- Replaced the provisional float physics with native units and constants:
  60 Hz frames, subpixel positions, ground/midair acceleration and caps,
  gravity and fall caps, low/High Jump/Suitless/Morph Ball jump velocities,
  jump-release cut and the native 14x31, 14x23 and 14x15 block hitboxes
  (spin poses use the crouched box). The previous 12x16 placeholder box and
  invented skid-on-release behavior are gone.
- Wall jump, Space Jump renewal, ledge grabbing (now requiring Power Grip),
  pull-up velocities, hurt velocities and invincibility follow the source.
  Damage now passes through the native Varia/Gravity reduction rule with an
  explicit energy/equipment record.
- The runtime maps each pose to one semantic action and renders the
  controller's own native frame index, so animation priority and transition
  timing are no longer inferred from held keys. Spin poses fall back only to
  the spin sequence. The registry gained `turn_midair` and `turn_crouch`
  (468 bindings over 29 actions).
- Input follows the GBA layout on keyboard and gamepad. Diagnostic keys cycle
  suit presets, Space Jump/Screw Attack and High Jump; F7 outlines the hitbox.
- Added `fusion_mzm_samus_tests` (native tables, damage, running and jump
  apex, spin/Space Jump/Screw Attack, wall jump with and without a wall,
  crouch/morph/unmorph and tunnel clearance, ledge hang alignment and pulls,
  hurt/death, and a 40,000-frame random-input no-embedding invariant). A
  200,000-frame random-input run against the real Brinstar 033 collision,
  including steep slopes, never embedded the hitbox.
- Known gaps: collision uses verified boxes plus a subpixel sweep instead of
  the original point probes (no slope speed changes or ceiling nudges); Speed
  Booster, Shinespark, weapons, bombs, aiming while hanging and crawling are
  not implemented; fire only triggers the native shooting-pose reaction.
- Validation on 2026-10-10: warning-clean build, CTest passed 15/15, the
  headless registry check accepted 580 sequences and 468 bindings, and
  Brinstar 033 loaded its 204 native collision records and remained live
  through the dummy-video smoke test.

## Patch 0190 - native table-driven Samus composer with visible suits

- Added `scripts/mzm_samus_compose.py`, which composes every Samus animation
  from the decompilation pointer tables the way `SamusUpdateGraphicsOam`,
  `SamusDraw` and `SamusUpdatePalette` select them: body table and selector,
  matching arm cannon animation (or `_All[pose]`), arm cannon graphics by the
  running/hanging/zipline/default rules, cannon front/behind order and suit
  palette. Frames are rendered once to palette indices, colorized per suit,
  cropped, and carry their native draw offset including SamusDraw's 2-pixel
  shift.
- Suit changes are now real: Varia uses Power Suit graphics with the Varia
  palette, Gravity uses Full Suit graphics with the Gravity palette, matching
  the source. The previous registry mapped Varia and Gravity to unmodified
  Power Suit art, and Full Suit/Suitless frames lacked their arm cannon.
- `python3 -m scripts.mzm_samus_pipeline` now builds the library directly from
  ROM, ELF and tables in about three seconds: 1,384 sequences, 6,008 frames,
  3,192 unique BMPs, zero unresolved variants. The runtime index moved to the
  versioned v3 schema with per-frame offsets, unreferenced objects are pruned
  (1,943 obsolete objects removed locally) and `sequences.json` records the
  source symbols of every key. `--bundle-only` and the intermediate caches are
  no longer used by the canonical pipeline.
- Validation against the previous library: 387 of its 580 sequences are
  reproduced pixel for pixel; the other 193 were body-only exports that now
  include the native arm cannon (hidden body pixels replaced by cannon pixels).
- The semantic registry now has 600 bindings over 33 actions, including up and
  down aims, rolling, midair Morph Ball, morphed hurt and the Space Jump Screw
  Attack variant. The SDL runtime loads the v3 index with dynamic storage,
  draws every frame at its native offset from Samus's position and selects
  `ScrewAttacking[TRUE]` when Space Jump is equipped.
- Added ROM-free tests for table pairing, selector aliases, cannon graphics
  rules, draw order, the Dying exception, cropping/offsets, registry generation
  and pipeline deduplication, pruning and failure on unresolved variants.
- Validation on 2026-10-10: warning-clean build, CTest passed 15/15, the
  registry check accepted 1,384 sequences and 600 bindings, and Brinstar 033
  remained live through the dummy-video smoke test. A contact sheet of the
  private output confirmed distinct Power, Varia, Full, Gravity and Suitless
  renders with their arm cannons.

## Patch 0191 - retire the patch-numbered Samus extraction chain

- The pipeline now checks that every ELF symbol it reads (animation, arm cannon
  and palette arrays) holds exactly the ROM's bytes, using an `objcopy` image
  of the reference ELF. This replaces the old hard-coded palette-address guard
  and fails closed on a mismatched build. The two files differ only outside the
  Samus data (header logo and unrelated blocks).
- Removed 21 superseded scripts (`mzm_samus_export_0150`, compositions
  0160/0164/0166, bulk 0170, special 0171/0173/0174, cannon 0156-0159, runtime
  sidecars 0163/0165/0167, runtime library/index 0175/0176, bundle 0181,
  `mzm_samus_catalog`, `mzm_samus_resolve` and the OAM diagnostic 0172) and
  their 22 test modules. Their only consumers were each other; the composer
  reproduces or supersedes all of their output. The editor preview chain
  (`mzm_samus_sprite`, `mzm_samus_body`, `mzm_samus_frame`,
  `mzm_samus_oam_decode`, `gba_oam`) is unchanged.
- Dropped the unused intermediate/metadata/diagnostics layout constants and
  deleted the matching private caches (21 MB of `intermediate/` plus
  `metadata/`), which no remaining code reads. The runtime library is rebuilt
  from ROM, ELF and tables alone.
- Validation on 2026-10-10: CTest passed 15/15 (Python suite 354 tests), the
  rebuilt library still has 1,384 sequences and 600 bindings, and the
  proprietary-file guard passed.

## Patch 0192 - shared sprite libraries and the complete Soma animation set

- Added `scripts/sprite_library.py`: a game-neutral content-addressed writer
  (`metroidvania-sprite-index-v1`) with per-frame native draw offsets,
  pruning of unreferenced objects and atomic writes. The Samus pipeline now
  uses it unchanged in output; the SDL loader accepts the shared schema and
  gained `--check-library` to validate any library without a display.
- Added `python3 -m scripts.aos_soma_pipeline`, which exports every animation
  of Soma's native descriptor instead of six hand-picked sequences: 83 Soma
  animations and one knife animation, 382 frames, 150 unique BMPs, each with
  native durations and offsets from Soma's draw origin.
- Documented in `docs/AOS_SOMA.md` what remains unidentified (palette banks
  1-6, other weapons, multi-component knife OAM, most semantic names) and the
  evidence still missing for an Aria movement and collision kernel. No Aria
  physics or collision semantics are invented.
- Validation on 2026-10-10: warning-clean build, CTest passed 16/16, both
  libraries validated by the C loader (Samus 1,384 sequences / 6,008 frames;
  Soma 84 sequences / 382 frames), and a contact sheet confirmed feet-anchored
  Soma frames.

## Patch 0193 - projectile sprites, arm cannon offsets and armed cannon art

- The Samus composer now also renders the missile-armed arm cannon graphics
  (`sArmCannonGfxPointers_*_Armed_*`, with the source's running, hanging and
  zipline rules) as `<key>/armed` sequences, and records each frame's arm
  cannon offset exactly like `SamusUpdateArmCannonPositionOffset` in
  `cannon_offsets.tsv`.
- Added `scripts/mzm_projectile_compose.py`. The Samus pipeline now writes a
  projectile sprite library: 54 native `FrameData` tables (all beams and
  charged beams, pistol, missiles, super missiles, bombs, power bombs) in four
  flip states, 216 sequences and 492 unique BMPs, staged with the common sprite
  graphics/palette and each beam set's graphics and palette row as the game
  loads them. Symbols are verified against the ROM like the Samus data.
- Library totals: Samus 1,864 sequences (7,912 frames, 4,544 unique BMPs) with
  the same 600 semantic bindings; projectiles 216 sequences.
- Added ROM-free tests for table filtering, beam-set selection, flip mirroring,
  palette-row checks, armed cannon rules, muzzle sign handling and the offset
  table. A contact sheet confirmed native beam, missile and bomb colors.
- Validation on 2026-10-10: CTest passed 16/16 and all three libraries
  validated through the C loader.

## Patch 0194 - native Power Beam and missiles in the SDL runtime

- Added `src/runtime/mzm_projectiles.{h,c}`, an SDL-free port of weapon
  highlighting (R arms missiles, Select toggles super missiles), the fire
  check with native cooldowns (7/9/11 frames), projectile limits (6 beams,
  4 missiles, 4 super missiles), spawning at the arm cannon offset, native
  movement (Power Beam 16 then 20 subpixels per frame with a 13-update short
  beam lifetime; missiles 48 subpixels then accelerating, super missiles
  faster; diagonal shots at 7/10; Samus's velocity added), point collision
  with verified Clipdata, ammunition use and disarming, projectile animation
  timing and the distance despawn.
- The runtime feeds the native hasNewProjectile flag to the pose controller
  (shooting poses and spin breaks now come from real shots), reads per-frame
  arm cannon offsets from `cannon_offsets.tsv`, draws projectiles from the
  projectile library with their flips, switches to the `/armed` arm cannon art
  while missiles are highlighted, and shows ammunition in the window title.
  The sprite loader now resolves keys by binary search.
- Added `--capture out.bmp FRAMES BUTTONS REPEAT` for headless verification of
  the real renderer. Captures in Brinstar 033 show the native shooting pose
  with a Power Beam shot at the muzzle and an upward missile from the armed
  cannon.
- Added controller tests for weapon highlighting, beam spawn position,
  cooldown, movement, lifetime, wall collision, diagonal speed, the six-beam
  limit, missile acceleration, velocity inheritance, ammunition and firing
  during a spin.
- Not implemented: charge beam, Long/Ice/Wave/Plasma behavior, pistol, bombs,
  projectile hits on sprites or destructible blocks, particles and trails.
- Validation on 2026-10-10: warning-clean build, CTest passed 16/16, and
  Brinstar 033 loaded the 1,864-sequence Samus library, 600 bindings and the
  216-sequence projectile library.

## Patch 0195 - fix MZM raw-data consumers after the asset migration

- The canonical importer writes Zero Mission raw payloads below
  `assets/extracted/metroid/raw/data/`, but `mzm_room_render`,
  `mzm_hatch_preview` and `world_overview` still read the removed
  `raw/metroid/data/` tree, so native MZM room rendering (and the editor's
  `room-render`) failed on a fresh extraction. They now use the new
  `METROID_RAW_DATA` layout constant; their tests use the canonical path.
- Validation on 2026-10-10: the Python suite passed, and
  `fusion_editor_cli --command=room-render --world=zero_mission --area=Brinstar
  --room=33` succeeded again on the private extraction.

## Patch 0196 - native Clipdata types and every MZM room in the runtime

- Added `python3 -m scripts.mzm_runtime_room --area <Area> --room <N>`, which
  exports a room's partial BG1-over-BG2 render and every Clipdata cell with
  its native collision type, resolved like `RoomLoadTileset`/`ClipdataProcess`
  from the decompilation tables at export time (main table, contiguous tilemap
  table, cleared RAM in between). All 330 Zero Mission rooms export; 114 of
  them contain slight slopes the runtime previously ignored, and Brinstar 033's
  hatches are now solid doors.
- The runtime replaced its collision list with a per-block type grid and
  ports `ClipdataConvertToCollision`: solid and door blocks, the two steep and
  four slight floor slopes at subpixel precision, and the actor rules of
  enemy-only, stop-enemy and tank blocks. Box queries only visit overlapped
  blocks. Samus's ledge and wall-jump probes and projectiles use native point
  queries (Samus sees solid columns outside the room, projectiles air).
- `--room <area>_<NNN>` now opens any exported room; the Brinstar 033 alias no
  longer depends on the historical preview and overlay caches. Exported rooms
  are also detected when passed by path.
- Added C tests for every collision type and actor rule, point queries, the
  type grid and the native room parser (with a synthetic fixture and a CTest
  `--check` run).
- Validation on 2026-10-10: warning-clean build, CTest passed 17/17, all 330
  rooms exported, and captures confirmed Samus standing and running on native
  geometry in Brinstar rooms 1 and 33.

## Patch 0197 - native doors, room transitions and hatches

- The runtime room export now also writes, from the decompilation tables,
  every door of the room (`sAreaDoors` with type, rectangle and the
  destination door's room and exit offsets), the door-transition and
  vertical-transition clip behaviors, and the room's hatches built like
  `ConnectionLoadDoors` (side, type from `sHatchTypeTable`, weakness and
  health from `sHatchBehaviors`). All 330 rooms export: 759 doors and 506
  hatches (437 normal, 47 missile, 13 super missile, 7 power bomb, 2 locked).
- The SDL runtime ports `BgClipCheckTouchingTransitionOrTank` and
  `ConnectionCheckEnterDoor`: touching a transition at the native probe
  points enters the matching door, loads the destination room's export and
  places Samus with the `RoomLoad` exit formula, keeping her height inside
  the door (`gSamusDoorPositionOffset`, clamped as in the source).
- Projectile impacts now reach hatches like `BgClipCheckOpeningHatch`: the
  weakness mask selects which weapons count, super missiles open missile
  hatches at once, locked hatches stay shut, and an opened hatch clears its
  four Clipdata cells. Hatch shells are drawn as tinted rectangles because the
  common hatch tiles are not rendered yet.
- Not yet native: hatch open/close animation and re-closing, persistent
  opened-hatch flags, event-based door overrides, area connections
  (elevators), camera scrolling rules and the transition fade.
- Added ROM-free tests for door touch detection, the in-door height offset,
  exit placement, hatch opening and locked hatches, and projectile impacts
  reporting their damage type.
- Validation on 2026-10-10: CTest passed 17/17. A capture shot the Brinstar 033
  hatch, walked into Brinstar 31 and on to Brinstar 20, where Samus grabbed a
  ledge after the transition.

## Patch 0198 - reconstruction principle documented

- `docs/ARCHITECTURE.md` now defines the permanent constraint: the ROMs are the
  source of truth, native resources are rebuilt automatically, native behavior
  is ported from decompilation evidence, and editor modifications, project data,
  gameplay extensions, character adapters and shared systems are separate,
  non-destructive layers. It states the target pipeline and separates what
  exists, what is experimental and what is not implemented. README and the
  roadmap link to it; no tool was restructured.

## Patch 0199 - native Samus palettes

- The Samus library now stores palette-indexed 8-bit frames per graphics
  family (1,056 sequences, 2,600 unique BMPs instead of 1,864 per-suit copies)
  plus `palettes.tsv` with every `sSamusPal_*` row, verified against the ROM.
- The runtime colorizes indexed frames per palette variant with a small
  texture cache and selects rows like `SamusUpdatePalette`: invincibility
  flashing (replacing the old alpha blink), Screw Attack flash, beam release,
  unmorph and the dying sequence (including the source's read into
  `sSamusPal_Generic_Dying`). The jump echo now uses palette bank 1 instead of
  a violet tint. The controller tracks `unmorphPaletteTimer` and the weapon
  state tracks `beamReleasePaletteTimer`.
- Added tests for palette parsing, every selection rule, the echo bank and the
  indexed BMP writer; the registry test now checks suit-to-family binding.
- Validation on 2026-10-10: CTest passed 17/17; a Brinstar 001 capture showed
  correct Power Suit colors from the indexed frames.

## Patch 0200 - Aria BG1 collision decoded and ported

- Located the Aria collision readers in cvaos: `sub_08001A00` (cell lookup on
  the decompressed table at `gEwramData + 0xE0D0`), `sub_08001800` (same
  indexing as the importer, which is now verified), `sub_08001B40` (slope
  height) and the vertical/horizontal probes. Checked against all 342 BG1
  tables: bit 0 blocks from above (floors and platforms), bit 1 from below
  (ceilings), `0x03` is solid, slopes encode step, start height and direction.
  Bit 3 and a few rare values remain unidentified (`docs/AOS_SOMA.md`).
- Added `scripts/aos_runtime_room.py` (342 of 343 rooms export background and
  native collision bytes, with the slope X-flip toggle applied as the game
  does) and `src/runtime/aos_collision.c`, a ROM-free port of the collision
  primitives with tests. The Aria importer now also records per-cell block
  X flips.
- Recorded the probe points of Soma's collision routine `sub_08014A04` as
  reading notes; Soma's controller is still blocked on its update logic.
- Validation on 2026-10-10: CTest passed 18/18 and the proprietary guard
  passed.

## Patch 0201 - Soma motion rules

- Added `src/runtime/aos_soma.c`, a ROM-free port of the verified Soma motion
  rules from cvaos: position integration and fall cap (`sub_0801B0D8`),
  steering with friction, the normal jump and its three-frame coyote window
  (`sub_08019180`), jump release, apex float and slow fall (`sub_0801938C`),
  the ledge start (`sub_08018020`) and gravity (`sub_08018B98`).
- Corrected `docs/AOS_SOMA.md`: the flag that weakens jumps and adds 0.375
  gravity is `0x4000000`, not `0x400000` (which is the slow-fall flag).
- Not ported yet: collision response (`sub_08014A04`), ceiling bump, second
  and mid-air jumps, backdash, recoil and the other player states.
- Validation on 2026-10-10: CTest passed 19/19 and the proprietary guard
  passed.

## Patch 0202 - Soma collision pass

- Ported the tile path of Soma's collision pass `sub_08014A04` into
  `src/runtime/aos_soma.c`: wall pushes from the ROM probe lists with the
  quarter bounce, ceiling flags, bit-3 flags, floor snap with the step-down
  probe, slope flags, one-way platform catching and normal/hard landings; the
  gravity routine now bumps the head on ceilings. The order and constants
  are documented in `docs/AOS_SOMA.md`.
- Tests on synthetic layers cover landing, hard landing, walls, ceilings,
  rising through and landing on platforms, walking off a ledge and walking
  down a slope. A local check dropped Soma into exported rooms 0/1, 0/5 and
  0/9; he landed on their floors and slopes.
- Still missing for a playable Soma: the ground and air state routines of
  `sub_0801B0D8` (only their steering is ported), moving platforms, the
  animation selection and the runtime integration.
- Validation on 2026-10-10: CTest passed 19/19 and the proprietary guard
  passed.

## Patch 0203 - Soma normal and hard-landing states

- Ported the movement part of player state 0 (ground and air: speed, slope
  slowdown, crouch, friction) and state 4 (hard landing), and the complete
  non-ability part of the jump routine `sub_08019180` (low-ceiling block,
  platform drop-through, the flag 0x800000 jump). `aos_soma_update` runs one
  native frame: integration, collision pass, state routine.
- The tests now drive Soma through `aos_soma_update` and cover uphill
  slowdown, crouching, ceiling-forced crouch, drop-through and the
  hard-landing exit driven by the animation-end flag.
- Corrected `docs/AOS_SOMA.md`: the "probable backdash" was the slide
  (state 3); the real backdash is 3.75 px/frame; case 1 is an attack state,
  not the air state.
- Validation on 2026-10-10: CTest passed 19/19 and the proprietary guard
  passed.

## Patch 0204 - Soma animation timing and requests

- Added `src/runtime/aos_anim.c`, a port of the animation start
  `sub_0803F2C8` and the encoding-1 step `sub_0803EC34`, and wired Soma's
  native animation requests: idle, walk start/walk, turn, stop, crouch
  down/crouch/stand up, look up, jump/forward jump/fall, landing and hard
  landing, with the one-shot request slot `+0x20`. The hard landing now ends
  with its animation, as in the game.
- Soma's wall probes are selected per frame like the game (standing, low
  ceiling, flag 0x800000, or the speed-dependent airborne list).
- Tests check the animation timing and every ported animation sequence with
  a synthetic animation set.
- Validation on 2026-10-10: CTest passed 19/19 and the proprietary guard
  passed.

## Patch 0205 - experimental Aria runtime

- Added `fusion_aria_runtime` (`src/runtime/aria_runtime.c`): loads an
  exported Aria room (native collision bytes and background) and Soma's
  sprite library, whose durations become the native animation timings, then
  runs `aos_soma_update` at 60 Hz with keyboard input, a following camera,
  mirrored frames when facing left, `--check` and a headless `--capture`.
- Checked on area 0 room 5: idle, walk and forward-jump captures show the
  native animations with the feet on the floor surface. The spawn point is a
  documented test placement; enemies, souls, attacks and transitions are not
  modelled.
- Validation on 2026-10-10: CTest passed 19/19 and the proprietary guard
  passed.

## Patch 0206 - native Aria room transitions

- Decoded the Aria room exit path in cvaos: the bounds and exit-cell test
  `sub_08011A44`, the velocity adjustments of `sub_08010244`, the entry
  choice and on-screen placement of `sub_08010350`, and the BG1 load offsets
  of `sub_0800ED5C`/`sub_0800EE54` (vertical + 0x30) and `sub_0803FBBC`.
  Recorded in `docs/AOS_SOMA.md`.
- `scripts/aos_runtime_room.py` exports the transition list (format version
  2) and gains `--all`; `src/runtime/aos_room.c` ports the rules with
  ROM-free tests; `fusion_aria_runtime` changes rooms at the native arrival
  position and follows Soma with the transition camera target.
- `--audit-transitions` over the 342 exported rooms: 725 entries matched,
  723 edge arrivals inside their target, 2 door exit-cell entries reported
  separately (door entities not ported).
- Validation on 2026-10-10: CTest passed 20/20, the Python room export tests
  passed and the proprietary guard passed.

## Patch 0207 - native Soma animation palettes

- Identified the per-animation palette table at `0x080E126C` (read by
  `sub_0803C7B4` on direct animation changes): Soma's library now colorizes
  animations 0x21-0x22, 0x2A-0x2B, 0x49-0x4A and 0x4B with palette banks 2,
  3, 4 and 5 instead of bank 0; banks 1 and 6 stay reported as unidentified.
- Validation on 2026-10-10: CTest passed 20/20, the sprite library tests
  passed and the proprietary guard passed.

## Patch 0208 - Soma ability moves

- Ported Soma's ability moves from `sub_0801B0D8`, `sub_08019180`,
  `sub_080190E0`, `sub_08017D90`, `sub_08017F94` and `sub_08017CC8`:
  backdash (3.75, ends with its animation), slide (state 3, 32 frames),
  high jump (state 5, vy -10.0, extra ceiling probes), ceiling crash
  (state 6), mid-air jump (once per jump) and dive kick (state 7), with
  their animation requests and the per-frame flags cleared at the end of
  the player update. Ability bits and the default button mapping (B attack,
  A jump, L backdash/high jump, R guardian) are documented.
- `fusion_aria_runtime` enables all five moves by default (`--moves`), maps L
  to Q/Left Shift; captures in room 0/6 show the slide and the ceiling crash.
- Validation on 2026-10-10: CTest passed 20/20 and the proprietary guard
  passed.

## Patch 0209 - Aria wooden doors and the collision block override

- Ported the 16x16 solid-block override of `sub_08002200` / `sub_08002248`
  and its rule in `sub_08001A00` into `aos_collision.c`.
- Ported the wooden door (special object 0x00, `Object00Create` /
  `Object00Update`) into `src/runtime/aos_door.c`: blocking, contact and
  facing test, input lock while it swings open for 64 frames, forced walk
  through, and the open/walk-out/close sequence when Soma enters through a
  door, with ROM-free tests.
- The room export (format version 3) carries the entity records; the
  runtime spawns doors in the native camera window (`sub_0800F4F8`) and
  applies their forced input. Doors are drawn as outlines until their
  graphics are extracted. Checked in room 0/3: the door opens and Soma walks
  into room 0/0.
- Validation on 2026-10-10: CTest passed 21/21, the transition audit and the
  room export tests passed, and the proprietary guard passed.

## Patch 0210 - native door sprites

- Added `scripts/aos_object_sprites.py`: decodes the door's LZ77 tile sheet,
  OAM frame components, palette banks and its palette-cycling script
  (`sub_0803CC70` / `sub_0803C150`), and exports the wooden and green door
  styles as looping sequences in a private object library, with ROM-free
  tests.
- `fusion_aria_runtime` draws doors with these sprites and palette cycles
  (mirrored when facing left, checked against the doorway art of rooms 0/3
  and 0/16) instead of outlines.
- Validation on 2026-10-10: CTest passed 21/21, the object sprite tests
  passed and the proprietary guard passed.

## Patch 0211 - Soma attacks (body)

- Ported the attack start `sub_080197B4` and player state 1: standing,
  crouched and air attacks with the body animation of the equipped weapon,
  air steering, the landing rules (weapon flag 0x2000 keeps the attack and
  switches to the standing animation), recovery requests and the backdash
  cancel. Landing now clears `0x20031E` for such weapons, as the game does.
- Added `scripts/aos_weapons.py` (private export of the 59 weapon records,
  the unarmed record and their attack animations) and `--weapon` in the
  runtime. Weapon entities (blade sprites, hitboxes) are not ported yet.
- Validation on 2026-10-10: CTest passed 21/21 and the proprietary guard
  passed.

## Patch 0212 - Soma weapon entity

- Decoded the weapon entity of classes 0, 2 and 3 (`sub_080221CC`) and the
  OAM component format of the IWRAM sprite builder `sub_0804311C` (shape and
  size, alternate tile base, flips, palette mask).
- `scripts/aos_weapons.py` now also exports every weapon's animation as a
  private sprite sequence and its per-frame hitboxes; `src/runtime/aos_weapon.c`
  ports the entity lifetime, placement and hitbox with ROM-free tests.
- `fusion_aria_runtime` draws the blade with the attack (knife and rapier
  checked in captures) and outlines the hitbox with `--hitboxes`. Damage,
  enemies and the class 1, 4 and 5 weapons are not ported.
- Validation on 2026-10-10: CTest passed 22/22, the object sprite tests passed
  and the proprietary guard passed.

## Patch 0213 - first Aria enemy: the bat

- Traced the shared enemy framework (spawn stats, activity window,
  facing, collision and damage call chain) and ported the parts the bat
  needs into `src/runtime/aos_enemy.c`, with the native random generator and
  the sine of `sub_080009E4` (computed; it matches all 1024 table entries).
- Ported the bat's create (ceiling climb) and states 1 and 2 (hang, alert,
  stretch, hop, dive, sine-wave flight, off-screen deletion) with ROM-free
  tests; `scripts/aos_object_sprites.py` exports enemy animations and
  per-frame boxes.
- `fusion_aria_runtime` spawns bats in the native camera window; checked in
  room 0/5. Collisions, damage and deaths are not ported yet, so bats are
  harmless and cannot be hit.
- Validation on 2026-10-10: CTest passed 23/23 and the proprietary guard
  passed.

## Patch 0214 - Aria combat: collisions, damage and bat deaths

- Traced and ported the entity collision system: collision blocks, world
  rectangles, inclusive overlaps, recent-hit slots with per-type cooldowns
  (81 frames of immunity after an enemy hit), the cooldown tick, the enemy
  damage formula with weaknesses and resistances and the player damage
  formula (`src/runtime/aos_combat.c`), plus Soma's hurtbox at every
  animation change.
- Bats now hurt Soma on contact and die to his weapon: frame boxes, the hit
  callback and the death state (fall, blink, deletion) are ported; enemy
  stats and the death blink are exported privately.
- `fusion_aria_runtime`: `--atk/--def/--hp` (diagnostic stats: the new-game
  values are not traced), `--repeat N` for captures, `--hitboxes` also
  outlines hurtboxes. Checked in room 0/5: the knife kills bats (12 damage
  against 10 HP) and a bat's touch costs 8 HP. Soma's knockback reactions
  are not ported yet.
- Validation on 2026-10-10: CTest passed 23/23, the object sprite tests
  passed and the proprietary guard passed.

## Patch 0215 - Soma hit reactions

- Ported Soma's reaction to damage from `sub_0801B0D8`: the ground flinch
  (front, back and crouched animations, 50 frames of immunity, state 12)
  and the knockback (state 13, ending in a hard landing), with the pending
  hit recorded by the player damage routine. Soma's HP now lives in his
  state.
- Tests cover both reactions, the strong-hit rule and the HP clamp; in room
  0/5 a bat's touch makes Soma flinch and the second bat is ignored during
  the immunity window. Death is not ported yet.
- Validation on 2026-10-10: CTest passed 23/23 and the proprietary guard
  passed.

## Patch 0216 - Soma death

- Ported Soma's death (`sub_0801AF20`, state 16 `sub_0801B03C`): the death
  animation, hurtbox off and the drift that stops; the runtime reports it
  (the game-over mode is not ported). Checked with `--hp 5` in room 0/5.
- Validation on 2026-10-10: CTest passed 23/23 and the proprietary guard
  passed.

## Patch 0217 - zombies and generic enemies

- Ported the zombie (enemy 0x01): spawner records, rise, burst walking with
  the walker terrain collision `sub_08069A00`, sinking, screen-margin
  despawn, the hit callback and the generic enemy death `sub_0806AEAC`, with
  ROM-free tests.
- The enemy runtime is now generic by enemy id (sprites, boxes with separate
  attack boxes, stats, despawn margins exported privately); spawned zombies
  are added as runtime entities. Checked in room 0/3: zombies rise and walk
  toward Soma, hurt him for 14 HP and die in two knife hits.
- Validation on 2026-10-10: CTest passed 23/23, the object sprite tests
  passed and the proprietary guard passed.

## Patch 0218 - native parity inventory and rebuild graph

- Made full, independent reconstruction of Zero Mission and Aria the active
  architectural and roadmap priority; crossover engines, character adapters
  and combined progression are explicitly deferred until both original games
  can run from title screen to credits.
- Added `scripts/native_inventory.py`, a deterministic ROM-free scanner for the
  pinned C and assembly sources. It currently records 2,970 MZM and 3,573 Aria
  routines with stable IDs, source locations, exposed addresses, categories and
  19,877 resolved direct caller/callee edges. Header-only declarations, data
  tables, indirect calls and runtime evidence remain explicit inventory gaps.
- Added separate reviewed annotations in `data/native_parity/annotations.json`.
  They link the first eleven partial runtime features to native symbols, local
  implementation files and tests without allowing a source rescan to overwrite
  human validation.
- Added the generated `docs/NATIVE_PARITY_CHECKLIST.md`. It creates the complete
  requested category surface for both games, reports empty and unclassified
  areas honestly, and treats routine counts as discovery metrics rather than
  fidelity percentages.
- Added `python3 -m scripts.rebuild --all`, initially registering `inventory`
  and `checklist` tasks. The orchestrator declares dependencies, hashes real
  inputs, versions tasks, records ignored cache state atomically and skips
  unchanged outputs. Existing direct extraction commands remain unchanged;
  private ROM-backed tasks have not yet been migrated into the graph.
- Added ROM-free parser, annotation, generated-output and incremental-cache
  regressions. Validation on 2026-10-10: the warning-clean build succeeded,
  CTest passed 23/23, the Python suite passed 377/377, the generated outputs
  were byte-identical across a forced rebuild, the incremental dry run reported
  both tasks up to date, and the proprietary-file guard passed.

## Patch 0219 - declarations, native data and table dispatch

- Upgraded the source inventory to schema v2. Both pinned decompilations now
  contribute header function declarations, top-level C/assembly data symbols,
  named struct/union/enum definitions with lexical members, and object-like
  constants in addition to function definitions.
- Linked declarations to matching definitions and resolved indirect call edges
  when both a function-pointer table initializer and its indexed call expression
  are statically visible. This proves 406 MZM and 17 Aria table-dispatch edges;
  dynamic assignments and structure-member callbacks remain explicitly open.
- The indexed source surface now contains 1,521 MZM / 1,078 Aria header
  declarations, 16,063 / 139 data symbols, 36 / 11 recognized pointer tables,
  221 / 183 named aggregate types and 2,796 / 2,423 object-like constants.
- Replaced the near-limit monolithic JSON with a small inventory manifest and
  deterministic record fragments. Every fragment records its count and SHA-256,
  loads are checksum-validated, stale generated fragments are pruned, and every
  file remains below the repository's 4 MiB guard.
- Extended the rebuild graph so all manifest fragments are declared outputs and
  checklist inputs. Added ROM-free regressions for header/type/constant parsing,
  pointer-table discovery, real Samus pose dispatch, shard size limits and
  checksum rejection.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  23/23, the Python suite passed 379/379, every manifest fragment and the
  generated checklist were byte-identical across a forced rebuild, the
  incremental dry run reported both tasks up to date, and the proprietary-file
  guard passed.

## Patch 0220 - canonical TSV parity annotations

- Replaced `data/native_parity/annotations.json` with the canonical,
  human-edited `data/native_parity/annotations.tsv`: one tab-separated row per
  feature with explicit `game`, `id`, `category`, `status`, `title`,
  `native_routines`, `local_sources`, `tests`, `dependencies`, `divergences`,
  `notes` and `next_action` columns, and `|`-joined multi-value fields. TSV is
  preferred over CSV because notes contain commas and semicolons but never
  tabs, keeping the file directly editable in a spreadsheet or text editor.
- All eleven existing feature annotations migrated without loss; the new
  `next_action` column records each feature's immediate follow-up.
  `docs/NATIVE_PARITY_CHECKLIST.md` remains a generated view and now exposes
  dependencies, divergences and next actions when present.
- TSV loading enforces the exact header, column count, game-prefixed unique
  ids sorted in file order, strict status/category vocabularies, existing
  local paths, declared dependency ids and inventory-resident native symbols;
  a renamed or removed native routine now fails validation as an orphaned
  reference. Automatic inventory discoveries still cannot overwrite the human
  annotations.
- Added ROM-free regressions for the TSV round trip (parse/serialize bytes are
  stable), malformed headers, column counts, unknown games, id prefixes,
  ordering and unknown dependency rejection.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  23/23, the Python suite passed 382/382, the incremental rebuild dry run
  reported both tasks up to date, and the proprietary-file guard passed.

## Patch 0221 - anonymous typedefs and multi-declarator inventory coverage

- Inventory schema v3 indexes `typedef struct/union/enum { ... } Name;`
  anonymous aggregates as named type records (flagged `anonymous`) and every
  identifier in multi-declarator data and member statements, including
  bitfields and pointer declarators. Comma splitting respects nested
  parentheses, braces and brackets so initializers cannot split a declaration.
- The pinned sources gain 6 MZM and 40 Aria previously invisible type records
  (227 and 223 total). Multi-declarator file-scope data turns out to be absent
  from both pinned trees, so data symbol, pointer table and call-edge counts
  are unchanged (16,063/139 symbols, 36/11 tables, 6,071/14,228 edges with
  406/17 proven indirect); the gap is now closed by capability, verified on
  synthetic sources.
- Pre-existing duplicate data ids from mutually exclusive `#ifdef` build
  variants (for example both REGION_US_BETA branches of `demo_data.c`) are
  retained deliberately: both are real source alternatives, not index noise.
- The remaining open gaps are dynamic callbacks/function pointers stored in
  structure members and runtime-observed dependencies, as now reported by the
  generated checklist.
- Added ROM-free regressions for anonymous typedef members, bitfield and
  pointer multi-declarators and per-declarator initializer references.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  23/23, the Python suite passed 382/382, the incremental dry run reported
  both tasks up to date, the generated checklist check passed and the
  proprietary-file guard passed.

## Patch 0222 - function-pointer member callbacks in the inventory

- Finishes the interrupted inventory schema v4 commit: its tracked fragments
  had been generated before the final generator and reported no member
  callback edges. Regenerated, the inventory links 9 Aria and 91 MZM callbacks
  where a routine body writes a named routine into a member (`a->m = F;`,
  casts allowed) or a data initializer assigns one (`.m = F`), and the member
  is declared as a function pointer directly or through a function-pointer
  typedef (for example `SoundInit` installing `MP2K_event_null` into
  `SoundMixerState.CgbOscOff`, or the MZM cutscene stage tables'
  `pFunction`). Member names are matched lexically, so each edge lists every
  type that declares the member as a candidate.
- The checklist's open inventory gaps are now callbacks assigned through
  computed values or assembly stores, and runtime-observed dependencies;
  `docs/ARCHITECTURE.md` and `docs/ROADMAP.md` say the same.
- Added ROM-free regressions for function-pointer member detection (direct and
  typedef members), member writes, designated initializers and a known tracked
  edge.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  23/23, the Python suite passed 383/383, the incremental dry run reported
  both tasks up to date, the generated checklist check passed and the
  proprietary-file guard passed.

## Patch 0223 - native blue crows

- Ported the blue crow (Aria enemy 0x09) into `src/runtime/aos_enemy.c`: it
  perches until Soma comes within 60 pixels, flaps in place for 33 frames,
  then whenever he is more than 72 pixels away horizontally and 16 vertically
  flies at 1.625 per frame to 42 pixels above and 32 pixels behind him.
  Unlike the bat and the zombie it has no activity window and its collision
  pass needs no on-screen test. Its hit callback starts the native death: a
  64-frame fall at 0.5 with the shared blink, then the deletion.
- Added the homing helper `sub_080694B8` with the BIOS `ArcTan2` (exported as
  `aos_arctan2`, following the mGBA high-level BIOS polynomial, which is not
  checked against the BIOS itself), the BIOS integer `Sqrt` and the fixed
  product `sub_0803E86C`.
- `scripts/aos_object_sprites.py` exports the crow's sprites, palette and
  frame boxes; `fusion_aria_runtime` registers enemies by native id and runs
  the crows of area 0 rooms 12 and 24. A capture in room 0/12 shows the crow
  perched on the stair post, then holding position behind Soma's head.
- Not ported: the feathers, sounds, particles, palette bits, hit stun, the
  unentered glide step and the global `0x8E & 0x40` state.
- The parity annotations gain `aos.enemies.blue_crow`, and the bat record now
  cites its native routines (`EnemyBatCreate`, `EnemyBatUpdate`,
  `sub_080AD364`, `sub_080AD44C`, `sub_080AD5B8`).
- ROM-free tests cover the ArcTan2 quadrants and an exact atan(2), perching,
  the take-off, the 33-frame flap, the hold within 72 pixels, the flight to
  both sides of the player and the death.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  23/23, the Python suite passed 383/383, the incremental dry run reported
  both tasks up to date, the generated checklist check passed and the
  proprietary-file guard passed.

## Patch 0224 - address-taken routines and output-checked rebuilds

- Inventory schema v5 adds `address_references` to routines: the routines
  whose address they take without calling them, from C identifiers used as
  values (excluding call sites and member names) and from assembly
  literal-pool words. They are recorded as possible callback targets, not as
  calls. The pinned sources yield 124 MZM and 1,408 Aria such edges, for
  example `DemoInit` installing `DemoVBlank`, `HazeSetupCode` selecting its
  haze routines, and `EnemyBlueCrowCreate` registering its contact and hit
  callbacks `sub_0806E1B8` / `sub_080CA030`. Most Aria enemy, entity and
  callback registrations are of this assembly form, which schema v4 could
  not see.
- `scripts/rebuild.py` now also records a digest of each task's declared
  outputs: a fragment edited or corrupted by hand is regenerated even when
  the inputs are unchanged (it was previously reported up to date). A dry
  run marks tasks whose dependencies would be rebuilt as pending instead of
  failing on their not-yet-generated inputs (a deleted inventory fragment
  previously aborted the dry run).
- Checked by hand: a deleted and a corrupted inventory fragment are both
  detected and regenerated identically, and a forced rebuild is byte-identical.
- Added ROM-free regressions for address-reference discovery in C and
  assembly, tracked edges in both games, tampered outputs and pending
  dependencies.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  23/23, the Python suite passed 385/385, the incremental dry run reported both
  tasks up to date, the generated checklist check passed and the
  proprietary-file guard passed.

## Patch 0225 - ROM extractors in the rebuild graph

- `scripts/rebuild.py` registers ten private asset tasks around the existing,
  unchanged extractors: `aria_rom` / `metroid_rom` (SHA-1 verification,
  recorded in a private manifest), `raw_import` (`import_game_assets.py`,
  both ROMs), `aria_world`, `aria_rooms`, `aria_soma`, `aria_objects`,
  `aria_weapons`, `metroid_samus` (Samus and projectile libraries; also needs
  the reference ELF) and `metroid_rooms`. New selections: `--native`,
  `--assets`, `--task NAME` (repeatable); `--list` shows groups,
  dependencies and required files.
- A task's digest covers its ROM, its extractor and the `scripts` modules it
  imports, and its decompilation's pinned revision and tracked-file state.
  Missing required files skip the task and its dependents instead of failing.
  Outputs are validated beyond their digest: new `scripts/export_index.py`
  indexes multi-file exports by SHA-256, and sprite libraries are checked
  against their content-addressed object names.
- `aos_runtime_room --all` now writes `export_index.tsv` (files and skipped
  rooms) and `mzm_runtime_room` gains `--all` with the same index (330 rooms
  exported, 1 skipped).
- Checked on the local ROMs: a full `--assets` pass rebuilt all ten tasks in
  4 min 7 s; the next dry run of all twelve tasks reported every one up to
  date in 1.6 s; appending a byte to an exported Aria room made `aria_rooms`
  pending again, and restoring it made it up to date.
- Added ROM-free regressions for skipped requirements, invalid exports and
  extractor module discovery.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  23/23, the Python suite passed 388/388, the dry run of all twelve tasks
  reported them up to date, the generated checklist check passed and the
  proprietary-file guard passed.

## Patch 0226 - evidence-based routine classification

- Inventory schema v6 classifies routines without a path rule from two
  explicit kinds of evidence, recorded per routine as `category_evidence`:
  native name rules (`Enemy*Create/Update` enemies, `ObjectNN*` environment,
  `Skill*Use` abilities, `SoulInventory_*` inventory, `GameMode*` handlers
  by mode, the sound driver, BIOS and entity primitives...), then exclusive
  referrers: a routine whose callers and address-takers all share one
  category inherits it, to a fixed point. Mixed or absent referrers leave a
  routine unclassified; Aria bosses stay within enemies (no boss marker
  traced).
- Aria goes from 136 classified routines (path only) to 2,192 (136 by path,
  503 by name, 1,553 by exclusive referrers); 1,381 shared or unreferenced
  routines remain unclassified. Spot checks agree with the traced code: the
  blue crow AI, `sub_080694B8`, `sub_0806AEAC` and `sub_0806BE74` are
  enemies; the animation, collision-pass and Soma routines stay unclassified.
  Zero Mission is unchanged (all 2,970 routines by path).
- The generated checklist states each category's evidence breakdown;
  classification is still not coverage: category completeness remains
  unestablished.
- Added ROM-free regressions for the name rules, transitive and
  address-based referrer inheritance, mixed referrers and a tracked routine.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  23/23, the Python suite passed 389/389, all twelve rebuild tasks were up to
  date, the generated checklist check passed and the proprietary-file guard
  passed.

## Patch 0227 - shared gamepad input for both runtimes

- New `src/runtime/gba_input.{h,c}`: SDL3 gamepads mapped to the GBA
  KEYINPUT mask, opened at start-up and on hot-plug, replaced by the next
  connected gamepad when unplugged, with the left stick driving the D-pad
  beyond a dead zone and opposite directions cancelling as on the GBA.
  `--input-map FILE` (both runtimes) remaps up to three gamepad buttons per
  GBA key (`south`/`east`/`west`/`north` or SDL names) and the dead zone; a
  bad line rejects the file with its line number.
- `fusion_room_runtime` now uses it instead of its single, start-up-only
  gamepad (same default layout and the same half-travel stick threshold); its
  diagnostic pad buttons (Guide, North, Start) are unchanged.
  `fusion_aria_runtime` gains gamepad support for the first time; Soma's key
  bits are checked at compile time to equal the GBA bits, and opposite
  keyboard directions now cancel out as they would on the GBA.
- Annotations `aos.input.gamepad` and `mzm.input.gamepad` (partial): the
  native key reading and repeat logic and an in-game controls menu are not
  ported.
- Device-free tests (`tests/test_gba_input.c`, CTest `gba_input`) cover the
  default layout, stick threshold, opposite directions, remapping, SDL and
  positional names, rejected lines and map files. Captures of both runtimes
  are unchanged. No physical gamepad was available in this session, so
  hot-plug was not exercised on hardware.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  24/24, the Python suite passed 389/389, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0228 - F1 debug menu (Aria runtime)

- New SDL-free `src/runtime/debug_menu.{h,c}`: toggles, bounded values (with
  optional names) and actions bound by pointer to the engine's state, GBA-key
  navigation (Up/Down wrap, Left/Right, L/R by ten, A, B closes), and
  formatted lines for a 240-pixel screen.
- `fusion_aria_runtime`: F1 or the gamepad debug chord (new `debug` line of
  input maps, Back + Start by default) opens the menu, which freezes the game
  and offers pause with F2 frame stepping, frame step, hitboxes, HP / max HP /
  refill, diagnostic ATK and DEF, the five ported ability moves, spawning
  any loaded enemy kind (bat, zombie, blue crow) 48 pixels ahead through
  `aos_enemy_create`, removing all enemies, and teleporting to an exported
  room (Soma keeps HP, moves, weapon and facing). A status panel shows the
  room, frame, Soma's position, state, animation, HP and live enemies.
- `--debug-menu` and `--debug-input MASK,...` drive it in captures. Checked
  in captures of room 0/12: the menu renders with the engine's values, a
  scripted spawn puts a zombie in front of Soma, "remove all" clears the
  crows, and the teleport loads room 0/15 at 384,223.
- Not in the menu yet: weapon, equipment, soul and inventory selection,
  flags, AI inspection and layer views (their engine parts are not ported or
  not exposed yet), and the Zero Mission runtime's menu.
- Tests: `tests/test_debug_menu.c` (navigation, bounds, names, actions,
  closing, capacity) and the debug chord in `tests/test_gba_input.c`.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  25/25, the Python suite passed 389/389, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0229 - F1 debug menu (Zero Mission runtime)

- `fusion_room_runtime` gets the shared debug menu: F1 or the gamepad debug
  chord freezes the game; items cover pause with F2 frame stepping, frame
  step, the hitbox outline, the suit preset, the items whose native effect
  the engine implements (High Jump, Space Jump, Screw Attack; Speed Booster
  is left out because it has no effect yet), energy, missiles and super
  missiles with their maxima (clamped), refill, 20 damage through the
  existing native hurt path, and teleporting to any exported
  `<area>_<NNN>` room at the default safe spawn. A status panel shows the
  room (also updated by door travel), frame, Samus's position, pose,
  energy and ammunition. Keyboard diagnostics stay available while the menu
  is closed.
- `--debug-menu` and `--debug-input MASK,...` (runtime key bits) drive it in
  captures. Checked: the menu renders the engine values in brinstar_033, and
  a scripted area/room change and teleport loads ridley_000 with Samus on
  the elevator platform.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  25/25, the Python suite passed 389/389, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0230 - development dashboard

- New static dashboard `dashboard/` (`index.html`, `style.css`, `app.js`,
  generated `data.js` / `data.json`): overview indicators (overall, Zero
  Mission engine, Aria engine, asset reconstruction, tools, Metroidvania
  gameplay, story, platforms, automated tests), one view per area with
  collapsible categories, search, status filters, the CTest and Python test
  tables, the git timeline, a blocked-and-incomplete view, the native
  inventory figures and the generation date and commit. It opens from
  `file://` (data loaded by a script tag, no request) and suits GitHub
  Pages; it ships no ROM data or extracted asset.
- New generator `scripts/dashboard.py`: read-only; combines the native parity
  annotations (unchanged, mapped onto the dashboard scale: missing -> not
  started, research -> unknown) with the new project registry
  `data/dashboard/features.tsv` (36 Metroidvania, story, tools, assets and
  platform features with stable ids). It keeps declared facts (status,
  fidelity, limitations) apart from verified ones (sources found, tests
  registered, test results with `--run-tests`), warns when a feature is
  declared validated without a passing test, and computes shares of
  registered features only.
- Registry content reflects the repository today: every crossover feature
  is not started (deferred by the roadmap), the story is design data, the
  editor tools are partial, Windows is unknown (never compiled there).
- Checked: generated with `--run-tests` on this Debian host (25 CTest tests
  and the Python suite passed); rendered from `file://` in headless Chromium
  (desktop and 390-pixel mobile widths) and Firefox. Edge was not available
  and is not claimed as tested.
- `tests/test_dashboard.py`: registry validation, share arithmetic, unknown
  statuses never counted as progress, declared versus verified facts, CTest
  source mapping, private-data refusal in outputs, the static offline page
  (only local scripts, no request APIs) and no test run unless requested.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  25/25, the Python suite passed 399/399, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0231 - native zombie soldiers and grenades

- Ported the zombie soldier (Aria enemy 0x0C): floor snap, walking in steps
  of 0.25 that face the player at the start of walk frames 1 and 5, the close
  attack (80 x 70 box, one chance in two) and the grenade throw (220 x 70
  box, one chance in four), the return to walking at the end of each attack,
  and the death in state 2. Identified the ids of the area-0 enemies from the
  ROM enemy table (`0x080E9644`).
- The grenade is a child entity sharing the soldier's sprites (frame 18) and
  record: vx = distance / 44 frames capped at 1.25, vy = -2.0, gravity
  0x1800, the projectile collision `sub_08069770`, one bounce at a third of
  its speed, explosion on the second floor hit or a ceiling with an attack box
  growing to 9 pixels around it, a harmless burst when it touches Soma
  (always a knockback) or a weapon, and deletion beyond screen margin 2.
  `aos_enemy_update` returns new child entities, which the runtime adds to
  the room; code-driven boxes are drawn in red with `--hitboxes`.
- Framework fixes from the traced code: starting an animation now clears
  the cycle-done bit, and a defeated enemy (`+0x3E` bit 1, set at death) skips
  the on-screen collision pass, so dying zombies, soldiers and bats can no
  longer be hit again or hurt Soma. Screen margins are kept per entry.
- The sprite exporter adds the soldier and single-frame sequences
  (`Enemy/<name>/frame_<n>`) for child entities.
- Checked in a capture of room 0/15: the soldiers walk on the lower floor, a
  grenade flies, and Soma takes a 24-point knockback (state 13).
- Not ported: the gibs, explosion particles and sounds, and state 3.
- Tests: walking, the throw with its exact grenade parameters, the bounce
  speed, the explosion box and duration, the grenade hitting Soma, and a
  death that lasts exactly 40 frames while the weapon keeps overlapping it.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  25/25, the Python suite passed 399/399, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0232 - native axe armors and the probe walker

- Ported the axe armor (Aria enemy 0x04) and the generic probe walker it
  uses (`sub_0806CAF8` / `sub_0806C828`): wall probes at the pre-move
  position from a ROM probe table (exported as a `probes` line of
  `enemy_frames.tsv`), ceiling and floor probes with the 4-pixel snap, the
  floor cell byte and slope flags, and the 0x14 mode that refuses walls,
  slopes and ledges. The armor patrols (turning at obstacles, or after
  parameter-0 steps), throws high or low when the player is ahead within 99
  pixels, backs away when he stays in reach, turns to its attacker on every
  hit and dies with its own animation.
- The axe is a child entity of collision type 0xA: 2.5 toward the throw,
  decelerated by 0x800 per frame so it returns, spinning (drawn rotated; the
  direction is inferred), destroyed when struck, deleted off screen. The
  collision pass now uses each entity's own type for Soma's recent hits, so
  an axe is tracked apart from enemy bodies (121-frame slot).
- `AOS_ENEMY_MAX_FRAMES` rises to 24 (the armor's walk has 18 frames).
- Checked in a capture of room 0/38: the armor walks, throws, the axe hits
  Soma for 26 (he flinches, type 0) and flies on spinning.
- Not ported: death explosions, the strike effect, palette bits, the global
  pause and state 3.
- Tests: snap, turning at a wall, patrol turning at walk frame 17, the throw
  timing and axe placement, the axe's flight and return, its own hit type,
  its destruction by a weapon, and the armor's death.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  25/25, the Python suite passed 399/399, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0233 - native skull archers and arrows

- Ported the skull archer (Aria enemy 0x05): it stands facing the player
  and shoots when he enters a 240 x 35 box, cycling its volleys 0, 0, 1, 1, 2
  from the ROM volley table (exported as `volley` lines: one arrow, or
  three for the last volley); a patrolling archer (parameter 0) walks at
  0.25 with its own probe table, backs away at 0.75 from a player within 79
  pixels and shoots once he is beyond 99. Arrows (collision type 0xA, sprite
  frame 25) fly at 3.0, knock Soma back and stick to him for 30 frames, and
  vanish when struck. A killed archer disappears on the next update.
- `AOS_ENEMY_MAX_ANIMS` rises to 12 (the archer has 12 animations).
- Checked in a capture of room 0/24: the archer shoots, the arrow flies and
  knocks Soma back for 22 (state 13).
- Not ported: the bone pieces, the strike effect, the global pause, state 4.
- Tests: facing, the box trigger, the five-volley cycle with its arrow
  counts and placement, the arrow's speed, knockback and 30 stuck frames,
  the patrol, retreat and shot, and the death.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  25/25, the Python suite passed 399/399, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0234 - generic debris and the archer's shattering

- Ported the generic debris piece `sub_0806C5AC`: friction of 79/80 on vx,
  clamped accelerations, deletion on a solid cell under the centre of its
  sprite's last OAM component (`sub_0806C48C`; the exporter now writes an
  `anchor` line per enemy animation frame) or off a widened screen. A killed
  skull archer now throws its seven pieces away from the attacker instead of
  vanishing.
- `aos_enemy_update` can now return up to eight child entities per update
  (`child_count` / `children`), which the runtime adds to the room.
- Documented that the game keeps entity positions screen-relative (the
  collision probes add the camera); the port keeps room coordinates and
  converts at absolute screen comparisons.
- Checked in a capture of room 0/24: the archer's skull, bones and bow fly
  away from Soma after the killing blow.
- Tests: seven pieces with their animations and velocities, no collision,
  and a piece that rises, falls and vanishes at the floor.
- Validation on 2026-10-10: the warning-clean build succeeded, CTest passed
  25/25, the Python suite passed 399/399, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0235 - skeleton research notes

- Recorded the traced structure of the Aria skeleton (enemy 0x02) in
  `docs/AOS_SOMA.md`: the wobbling affine body, its two child parts attached
  to frame attachment points, the patrol, approach, pickup and bone-throw
  states, the thrown bone and the death pieces. Not ported yet: the code read
  leaves the body and its first part hidden (`+0x58` bit 0x20, which
  `sub_0803AC40` treats as hidden), so the routine that makes the skeleton
  visible has to be found before an honest port.
- Confirmed from `sub_0803AC40` that entity positions are screen-relative
  (drawing is culled outside x -64..304 and y -32..224).

## Patch 0236 - Zero Mission sprite engine core and Zoomers

- New `scripts/mzm_sprites.py` (rebuild task `metroid_sprites`): room sprite
  placements resolved through the default spritesets, the enemy sprite
  library for sprites with a ported AI (OAM tables in four flip states with
  their palette row), and `metadata/sprites.tsv` (sprite RNG table, falling
  speeds, primary sprite stats) read from the ROM through verified reference
  ELF symbols. `mzm_runtime_room` now writes `P` lines; the 330 exported rooms
  hold 1,160 default sprite placements.
- New SDL-free `src/runtime/mzm_sprites.{h,c}`: `SpriteInitPrimary`, the
  `SpriteUpdate` order with the native sprite RNG, `SpriteUpdateAnimation`,
  `SpriteCheckOnScreen`, the sprite view of Clipdata
  (`ClipdataConvertToCollision`, original version) and the vertical and slope
  collision helpers, and the Zoomer AI (init onto any surface, crawling with
  its eight turn cases, slope animations, falls and landings).
- `fusion_room_runtime` loads, updates and draws the room's ported sprites
  (hitboxes with F7 / the debug menu), respawns them on door travel and
  teleport, reports live sprites in captures, and gains `--spawn X Y`.
- Checked in captures of brinstar_000: the six yellow Zoomers spawn on the
  floor, walls and ceiling; the floor Zoomer crawls, climbs the pillar and
  turns its corners.
- Not ported yet: combat (contact damage, projectile hits, death, drops),
  freezing and every other sprite.
- Tests: `tests/test_mzm_sprites.c` (clipdata conversion by actor, crawling
  speed, wall climb, fall and landing, removal without a surface, the red
  Zoomer on a ceiling, on-screen status) and `tests/test_mzm_sprites.py`
  (enum, spriteset resolution, data tables).
- Validation on 2026-10-11: the warning-clean build succeeded, CTest passed
  26/26, the Python suite passed 402/402, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0237 - Zero Mission sprite combat

- `mzm_sprites` ports `SpriteUtilSamusAndSpriteCollision` for sprites that
  hurt Samus: the pose hitbox against the sprite hitbox, the screw attack
  kill of `SpriteUtilSpriteTakeDamageFromSamusContact` (or a 3-frame stun),
  `SpriteUtilTakeDamageFromSprite` through the runtime (stats damage, suit
  reductions, hurt pose), the knockback away from the sprite from
  `gPreviousXPosition`, and the 15-frame ignore timer.
- It ports the beam, missile and super missile parts of
  `ProjectileCheckHittingSprite` (`ProjectileHitSprite`,
  `ProjectileMissileHitSprite`, `ProjectileSuperMissileHitSprite`,
  `ProjectileDealDamage`, `ProjectileHitSolidSprite`) with the native
  projectile boxes, weaknesses, damage, stun and destroyed pose; a destroyed
  Zoomer is removed.
- The runtime now follows the native frame order: `SamusUpdate`,
  `SpriteUpdate` (contact first), then `ProjectileUpdate` ending with the
  sprite hits. `metadata/sprites.tsv` weaknesses are parsed (low byte of the
  `C_16_2_8` column; the Zoomers' 0xFA matches the decompilation).
- Captures now report Samus's energy and each sprite's health. Checked in
  brinstar_000 (`--spawn 230 496`, B and Down held): a Zoomer touching Samus
  takes 10 energy (99 to 89) and puts her in the hurt pose; a Zoomer
  climbing down the wall beside her falls from 3 to 1 health on a beam hit
  and is removed by frame 330. Standing beams pass over floor Zoomers.
- Not ported yet: death explosions, debris and sounds, enemy drops, tumbling
  missiles, the stun palette flash, charge beams and the pseudo screw, speed
  booster and shinespark kills, solid and frozen sprites, power bombs.
- Tests: `tests/test_mzm_sprites.c` adds contact damage, invincibility and
  ignore timers, screw attack kills and stuns, beam stun timing and kills,
  beam immunity, missile damage and the super missile box edge.
- Validation on 2026-10-11: the warning-clean build succeeded, CTest passed
  26/26, the Python suite passed 402/402, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0238 - Zero Mission enemy drops

- `mzm_sprites` ports `SpriteUtilDetermineEnemyDrop` (ROM probabilities,
  the native roll and its full energy and ammunition substitutions),
  `ZoomerDeath` / `SpriteUtilSpriteDeath` turning the sprite into its drop,
  and the enemy drop AI: hidden spawn, 20 frames ignoring Samus and
  projectiles, collection with the native refills, blinking during the last
  80 ticks and despawn after 200 ticks counted every other frame.
- Fidelity fixes to patch 0237 found while reading the drop code: only the
  first hurting or collecting contact of a frame is handled
  (`gIgnoreSamusAndSpriteCollision`), the ignore timers run down at the end
  of `SpriteUtilSamusAndSpriteCollision` instead of after each sprite, and
  `SSC_HURTS_SAMUS` takes its real value (4).
- `mzm_sprite_update` now takes an `MzmSpriteFrame` (world, tables, the
  Zoomer and drop OAM durations, the equipment, frame counters).
- `scripts/mzm_sprites.py` exports the eight drop tables from the common
  sprite graphics and palette; `sprites.tsv` drop columns are parsed.
  Captures print Samus's position.
- Checked in brinstar_000 (`--spawn 230 496`, B and Down held): the Zoomer
  shot down beside the pillar becomes a small energy drop at frame 330,
  shown from frame 350 next to Samus (knocked back to x 204, out of reach),
  blinking by frame 600; a later contact (-10) pushes Samus onto it and it
  is collected (+5): energy 89 to 84.
- The grouped Mecha Ridley pickup selects one of three OAM tables from its
  follower part, restores 60 energy and clamps to the current maximum. Its
  natural boss source remains unavailable until Mecha Ridley is ported.
- Tests: `tests/test_mzm_sprites.c` adds one contact per frame, the ignore
  timer order, eight drop rolls, the drop spawn, projectile immunity,
  collection and the blink and despawn timing; `tests/test_mzm_sprites.py`
  checks the drop ids, tables and common graphics addresses.
- Validation on 2026-10-11: the warning-clean build succeeded, CTest passed
  26/26, the Python suite passed 403/403, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0239 - Zero Mission Rippers

- `mzm_sprites` ports the Ripper AI (`ripper.c`) for the brown and purple
  Rippers: init, `SpriteUtilChooseRandomXFlip`, the horizontal patrol with
  its wall probe, the two-part turn and the death into drops.
- Sprite AIs are grouped into families (`MzmSpriteFamily`: Zoomer, Ripper,
  enemy drops); the runtime loads each family's OAM frame durations from the
  sprite library, and `MzmSpriteFrame` carries them by family.
- `scripts/mzm_sprites.py` exports both Rippers with their own graphics and
  palettes (101 enemy sequences now).
- Checked in brinstar_003 (`--spawn 120 352`): the five brown Rippers patrol
  at half a pixel per frame (29 pixels in 59 frames) and turn at the walls.
- Tests: `tests/test_mzm_sprites.c` adds the Ripper init, speed, wall turn
  and turn animation, its immunity to beams and missiles, the super missile
  kill, its death point and the purple Ripper's speed.- Validation on 2026-10-11: the warning-clean build succeeded, CTest passed
  26/26, the Python suite passed 403/403, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0240 - Zero Mission Skrees and secondary sprites

- `mzm_sprites` ports the Skree AI (green and blue: detection, spin, dive
  with the drift toward Samus, landing, crash) and `SkreeExplosion`, the
  first secondary sprite: `SpriteSpawnSecondary`, secondary stats for
  contact damage, whole-sprite rotation, and the `SP_KILL_OFF_SCREEN` range
  of `SpriteCheckOnScreen`. Contact damage now skips the screw attack check
  for sprites ignoring projectiles, as `SpriteUtilSpriteTakeDamageFromSamusContact`
  does.
- `MzmSpriteFrame` carries Samus's position and the sprite array for
  spawning. `scripts/mzm_sprites.py` exports both Skrees, the
  `sSecondarySpriteStats` rows (`sstats`) and `sSkreeFallingSpeed`
  (`table`); the runtime draws secondary sprites with their parent's
  graphics and rotates whole-sprite rotations.
- Checked in brinstar_004 (`--spawn 150 160`): the first Skree spins and
  dives as Samus stands below, hits her (99 to 89), lands at frame 60, and
  its four parts spin outward from frame 113; the second Skree follows.
- Not ported yet: the crash debris, the huge explosion particle, sounds.
- Tests: `tests/test_mzm_sprites.c` adds the Skree wait and detection range,
  the spin, the dive with drift and landing, the 60-frame crash, the four
  parts (slots, ids, same-frame update, movement, rotation, secondary damage,
  projectile immunity, harmless frames, end of table) and the off-screen
  removal.
- Validation on 2026-10-11: the warning-clean build succeeded, CTest passed
  26/26, the Python suite passed 403/403, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0241 - Zero Mission particles and sprite debris

- New SDL-free `src/runtime/mzm_effects.{h,c}`: `ParticleSet`,
  `ParticleProcessAll` (high OAM priority first), `ParticleUpdateAnimation`
  and `ParticleCheckOnScreen` for the one-shot effects of the ported
  sprites and projectiles: sprite explosions (small to huge), the screw
  attack kill and the normal beam, invincible, missile and super missile
  impacts.
- `mzm_sprites` ports sprite debris (`SpriteDebrisInit`,
  `SpriteDebrisProcess`, the draw range check) and
  `SpriteUtilRandomSpriteDebris`; deaths set their particle, the screw
  attack kill its debris, the Skree its four impact debris and huge
  explosion, and projectile hits on sprites their impact particle.
- `mzm_projectiles` records wall impacts so the runtime sets their
  particles in the native order (projectile processing, then sprite hits,
  then `ParticleProcessAll`).
- `scripts/mzm_sprites.py` exports the particle and debris tables from the
  common sprite graphics and the nine debris velocity tables.
- Checked in brinstar_004 (`--spawn 150 160`): the crashing Skree throws its
  debris at frame 61 and leaves the huge explosion with its spinning parts
  at frame 116.
- Not ported yet: particle sounds, the super missile screen shake, debris
  splashes in liquids and the other particle effects.
- Tests: `tests/test_mzm_sprites.c` adds particle slots and replacement,
  animation timing and removal, the on-screen window, debris arcs, landing,
  bounce, delay, animation, draw range and slot limit, and the particles and
  debris of beam, invincible and screw attack hits.- The parity registry files the enemy drops under `items`
  (`mzm.items.enemy_drops`) and the particles under `animation`
  (`mzm.animation.particles`).
- Validation on 2026-10-11: the warning-clean build succeeded, CTest passed
  26/26, the Python suite passed 403/403, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0242 - Zero Mission Zeelas and Wavers

- `mzm_sprites` ports the Zeela by generalizing the Zoomer crawler (speed,
  hitbox, ground X flip, slope table, falling and landing animation speeds),
  the red Zeela's eyes (`ZeelaSpawnEyes`, `ZeelaEyes`: thrown, spinning,
  stopped by walls or Samus, exploding) with
  `SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT`, and the Waver.
- The runtime draws per-part rotation (`ROTATION_SCALING_SINGLE`) by turning
  the flipped frame around its centre. `scripts/mzm_sprites.py` exports both
  Zeelas (the red one with palette row 1), the Waver and the eyes' falling
  speed tables.
- Checked in brinstar_017: the five Wavers fly at their native speeds (59 by
  29 pixels in 59 frames) and one hits Samus for 12 (99 to 87); in kraid_017
  the three red Zeelas crawl on their walls.
- Tests: `tests/test_mzm_sprites.c` adds the Zeela speed, flip and hitbox,
  the red Zeela palette and eyes (spawn, motion, rotation, contact damage,
  stop, explosion, landing on the floor), and the Waver start, speeds and
  wall and floor bounces.
- Validation on 2026-10-11: the warning-clean build succeeded, CTest passed
  26/26, the Python suite passed 403/403, all rebuild tasks were up to date,
  the generated checklist check passed and the proprietary-file guard passed.

## Patch 0243 - Tracking registry audit and freshness check

- Audited both registries against the code, tests and recent commits:
  - `mzm.enemies.zoomer`, `mzm.enemies.ripper` and `mzm.enemies.skree` no
    longer list the death explosions, the Skree crash debris and the huge
    explosion as missing (ported in patch 0241); the Skree's rotation note now
    states what the port actually draws; their next actions were updated;
  - `mzm.weapons.beams_and_missiles` records the sprite hits and impact
    particles, with their sources and tests (`tests/test_mzm_samus.c`,
    `tests/test_mzm_sprites.c`), and lists what is still missing;
  - the particles no longer depend on the Skree (no dependency cycle);
  - `assets.enemy_sprites_mzm` lists every exported sprite family, the drops,
    particles and debris instead of the Zoomers only;
  - `tools.debug_menu` states what the two F1 menus edit today.
  No status changed: every native feature remains partial.
- `README.md` and `docs/MZM_SPRITES.md` no longer say that the Zoomers do not
  fight or that explosions, debris and other AIs are missing.
- New `python3 -m scripts.dashboard --check` and `python3 -m
  scripts.check_generated` (checklist and dashboard together): they write
  nothing, run no tests, compare only registry-derived facts and fail on
  stale generated documents. `tests/test_dashboard.py` covers the check.
- `AGENTS.md` now requires synchronizing the registries, the documents that
  state a feature's scope and the generated documents after every feature
  change, without raising statuses for percentages.
- Validation on 2026-10-11: the warning-clean build succeeded, CTest passed
  26/26, the Python suite passed 404/404, all rebuild tasks were up to date,
  `check_generated` reported fresh documents and the proprietary-file guard
  passed.

## Patch 0244 - Sidehoppers and Dessgeegas (Kimi's work completed)

- Kept and completed Kimi's unfinished commit 4887b26 (poses, OAM table
  names, families, ids and velocity table fields for the Sidehopper and the
  Dessgeega).
- Fixed the regression it introduced: `runtime_sprite_kinds_open` required
  every declared family, so the two unexported families disabled every Zero
  Mission sprite (the brinstar_000 Zoomers stayed at pose 0). Each family now
  loads alone: a missing OAM table is reported by name with its family, its
  sprites are not spawned and `mzm_sprite_update` skips a family without
  frame durations; the drops stay required. The AI tables are parsed from one
  list with their exact decompilation lengths, and a missing one is named.
- Restored `MzmSpriteFrame.samus_y` to Samus's position (the Skree reads it)
  and added `samus_hitbox_top` for `SpriteUtilCheckSamusNearSpriteLeftRight`.
- `mzm_sprites` ports `Sidehopper` and `Dessgeega` as one state machine
  (ground and ceiling, idle and head shake / scream, detection, jump warning,
  high and low jumps, wall bounces, falls, landings, deaths with the
  single-then-big explosion, the Sidehopper's jump hitboxes, the Dessgeega
  death event bug) and the Long Beam Dessgeega (event checks, door lock,
  hidden wait, drop through the floor with `CAA_REMOVE_SOLID`, dust and
  debris) through new `MzmSpriteHooks` (events, `gDoorUnlockTimer`, block
  removal). `mzm_effects` adds the medium and two-medium dust particles.
- `scripts/mzm_sprites.py` exports the Sidehopper and both Dessgeegas, the
  dust particle tables and `sSpritesFallingCeilingSpeed` and the four jump
  velocity tables.
- Checked in captures: in brinstar_000 the Zoomers again match their
  pre-regression positions (frame 60: 410,352 and 314,496); in kraid_001 the
  four Sidehoppers (two on the ceiling) spawn with 30 health, warn, jump,
  land and fall back when a ceiling jump meets the floor; in ridley_003 the
  four Dessgeegas jump about 35 pixels, one detects Samus, turns to her and
  hits her for 30 (99 to 69, the ROM damage) with the hurt knockback.
- Not ported: freezing, sounds, screen shakes, the door lock lifecycle, the
  statue room event and the event spritesets placing the Long Beam
  Dessgeega (so that variant runs in tests only).
- Tests: `tests/test_mzm_sprites.c` adds the Sidehopper init, idle loop,
  high jump (42 frames, 208 subpixels high, 192 across, hitboxes), landing,
  Samus detection and facing, wall bounce, fall, ceiling placement and death
  particle, the Dessgeega sizes, kept hitbox and death event bug, and the
  whole Long Beam sequence; `tests/test_mzm_sprites.py` checks the ids,
  exported tables and AI tables against the decompilation.
- Validation on 2026-10-11: Debug configure and warning-clean build, CTest
  26/26, the Python suite 405/405, `scripts.rebuild --all` with every task up
  to date, `check_generated` fresh and the proprietary-file guard passed.

## Patch 0245 - Zero Mission Violas

- `mzm_sprites` ports the Viola (blue and orange): placement onto the
  surface around it, crawling along floors (with slopes), ceilings and both
  walls with the native corner probes and block snapping, falls and landings,
  death into drops. `scripts/mzm_sprites.py` exports both Violas.
- Checked in ridley_003 (`--spawn 600 144`): the two blue Violas climb their
  pillars at half a pixel a frame (30 pixels in 59 frames) and the orange one
  reaches the floor; they draw with their ROM graphics.
- Tests: `tests/test_mzm_sprites.c` adds the Viola placement, speeds, a
  3000-frame loop around a closed box (all four surfaces, no fall, centres
  exactly 24 subpixels from each surface) and the fall onto a lower floor.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0246 - Zero Mission Sovas

- `mzm_sprites` ports the Sova (purple and orange) as its own crawler:
  ceiling state in `work2` with a ceiling table, animation-frame speeds,
  28-subpixel probes, its eight turns and offsets, two slope tables, falls,
  random-direction landings and death into drops. `scripts/mzm_sprites.py`
  exports both (orange with palette row 1).
- Checked in norfair_005: the eight Sovas crawl on floors and walls (purple
  about 31 pixels, orange 49 in 119 frames); from `--spawn 120 352` two
  contacts cost Samus 24 energy (12 each, the ROM damage); from `--spawn
  180 160` a purple Sova is drawn on the floor beside her.
- Tests: `tests/test_mzm_sprites.c` adds the Sova placement, hitbox and flip,
  the per-frame speed table, a loop around a closed box over the four
  surfaces (ceiling through work2, never Y flipped, no fall) and the orange
  palette and speed.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0247 - Zero Mission Zebs and Geegas

- `mzm_sprites` ports `Zeb` and `Geega` as one pipe bug state machine
  (detection above and beside Samus, rise, flight, respawn, Geega leaders and
  followers), `SpriteUtilCheckSamusNearSpriteAboveBelow`,
  `SpriteUtilCountChildPrimarySprites`, `SpriteSpawnDropFollowers`,
  `SpriteUtilCheckHasDrops` and `DEATH_RESPAWNING`. Primary sprites now keep
  their `primarySpriteRamSlot` (`mzm_sprite_init_primary` takes it; the
  runtime passes the slot), inherited by their secondary sprites and drops.
- `scripts/mzm_sprites.py` exports both Zebs and the four Geega sprites.
- Checked in brinstar_012 (`--spawn 1380 144`): the pipe Zeb rises beside
  Samus, hits her for 6 (99 to 93, the ROM damage), flies left 3 pixels a
  frame (180 pixels in 60 frames) and is back in its pipe by frame 200; from
  other spawns the Zebs face Samus on either side. In kraid_003 the Geegas
  spawn with their ROM health and the nearer one rises at once.
- Tests: `tests/test_mzm_sprites.c` adds the Zeb wait, horizontal and range
  limits, rise, harmful delay, stop height, flight speed, respawn, death drop
  following the pipe and the pipe waiting for it, and the Geega leader's
  follower (spawn, position after its init, removal) and wait.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0248 - Zero Mission Zebbos

- The pipe bug state machine covers `Zebbo` (green, yellow, green leader and
  follower): its 24-subpixel hitbox and the leader's two followers (1.5 and
  2.5 blocks below, 10 and 18 frames before they turn harmful, the second
  only spawned after the first). `scripts/mzm_sprites.py` exports the four
  Zebbo sprites.
- Checked in ridley_006 (`--spawn 130 928`): the green leader and its two
  followers rise in a column 16 pixels apart, fly toward Samus and hit her
  for 10 (the ROM damage).
- Tests: `tests/test_mzm_sprites.c` adds the Zebbo leader's hitbox, its two
  followers' positions and delays, and its wait for both.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0249 - Zero Mission Reos

- `mzm_sprites` ports the Reo (green and purple wings): idle hovering from
  the ROM tables, detection, homing with per-axis acceleration and braking,
  wall turns, pushing an overlapping Reo apart, and the big explosion death.
  `scripts/mzm_sprites.py` exports both Reos and `sReoIdleYMovement` /
  `sReoIdleXMovement`; the runtime reads them.
- Checked in kraid_009: the nearest Reo detects Samus, dives and hits her
  repeatedly for 12 (99 to 87 to 63, the ROM damage) while the three beyond
  seven blocks keep hovering.
- Tests: `tests/test_mzm_sprites.c` adds the Reo init and idle offsets, a
  full idle cycle returning to its position, the detection delay, the speed
  caps and step, braking and turning past the target, and the push of an
  overlapping Reo.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0250 - Zero Mission Mellas, Holtzes and Multiviolas

- `mzm_sprites` ports the Mella and the Holtz (shared swooper movement
  probes, idle tables, detection, swoop, slide or climb, return under the
  ceiling) and the Multiviola (diagonal bounces, animation offset by slot).
  The OAM frame limit per table rises to 64 for the Multiviola's 56 frames.
- `scripts/mzm_sprites.py` exports the three sprites and the Mella and Holtz
  movement tables (`sMellaIdleYMovement`, `sMellaIdleXMovement`,
  `sMellaGoingDownYMovement`, `sMellaGoingUpYMovement`,
  `sMellaMovingXMovement`, `sHoltzIdleYVelocity`); the runtime reads them.
- Checked in captures: in norfair_001 the Mellas hover and one swoops when
  Samus stands within 3 blocks (`--spawn 250 160`); in ridley_003 the nearest
  Holtz dives, slides on the floor and bonks back under its ceiling; in
  ridley_010 the Multiviolas bounce around and one hits Samus for 18 (the ROM
  damage).
- Tests: `tests/test_mzm_sprites.c` adds a full Mella swoop (delay, flag,
  facing, stop above the floor, return half a block under the ceiling), a
  full Holtz cycle (warning, dive speed, slide, climb, bonk 40 subpixels
  under the ceiling, idle) and the Multiviola's slot-based frame, speed and
  floor bounce.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0251 - Zero Mission Rinkas

- `mzm_sprites` ports the orange and green Rinkas: spawn, hidden respawns,
  on-screen reveal and vulnerability delay, the straight flight along a
  unit vector computed with the BIOS integer square root, respawn far from
  Samus, and `DEATH_NO_DEATH_OR_RESPAWNING_ALREADY_HAS_DROP`.
  `scripts/mzm_sprites.py` exports both Rinkas.
- Checked in tourian_003: the three Rinkas leave their spawn points, fly at
  Samus and a green one hits her for 20 (the ROM damage).
- Tests: `tests/test_mzm_sprites.c` adds the first spawn, the level flight (4
  subpixels a frame from the spawn point), the respawn delay, reveal and
  vulnerability, and a diagonal flight following the integer square root.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.


## Patch 0252 - Zero Mission Space Pirates

- `src/runtime/mzm_space_pirate.c` ports `SpacePirate` and
  `SpacePirateLaser` for the normal, three waiting and second pirate
  sprites: grid line-of-sight detection, walking, idling on stop-enemy
  blocks, obstacle jumps (the four ROM jump tables), crawling, climbing,
  wall pauses and wall jumps, alerted walking, charging and two-laser
  volleys on the ground and on walls, lasers hurting Samus and pirates, the
  hit-by-laser death and the Chozodia alarm (`gAlarmTimer`, set on sight,
  counted down by `mzm_sprites_end_frame`, stopped by the last pirate's
  death). The pirates' scratch use of `gSpriteDrawOrder[0..4]` and
  `gBossWork.work2` live in `MzmSpriteGlobals`.
- The sprite helpers shared by the AIs moved behind the internal header
  `src/runtime/mzm_sprite_ai.h` (`ai_*`); `SSC_SPACE_PIRATE` and
  `SSC_SPACE_PIRATE_LASER` contacts are handled; the OAM table limit per
  family rises to 40.
- `scripts/mzm_runtime_room.py` exports each block's clipdata movement
  (`M` lines from `sMovementClipdataValues`: zonelines, wall jump points,
  stop-enemy blocks) and the runtime reads it; `scripts/mzm_sprites.py`
  exports the five pirate sprites and the jump tables. The pirate laser's
  `CAA_BOMB_PISTOL` reaches the runtime's hatches through a new hook.
- Not ported: `SpawnWaitingPirates`, the alarm music and palette, sounds,
  freezing, the power-bomb, black and escape-ship pirates.
- Checked in captures: in chozodia_002 and chozodia_013 pirates see Samus,
  ring the alarm (480 frames), charge 24 frames and fire two lasers 20
  subpixels a frame (52 and 120 subpixels above their feet) that hit Samus
  for 100 (the ROM damage) or kill another pirate after its 33-frame flash;
  chozodia_013's pirates idle on stop-enemy blocks and turn around. All 47
  rooms with pirates ran 300 frames without fault (climbing, wall jumps,
  waiting pirates leaving their doors with the alarm); chozodia_057 shows a
  pirate pausing at a wall.
- Tests: `tests/test_mzm_sprites.c` adds the pirate placement and walking
  acceleration, detection, alarm, charge and laser spawn, laser movement,
  hatch action, Samus contact and explosion, the friendly-fire death ending
  the alarm (re-armed after Mecha Ridley), zoneline removal, climbing down
  to a wall jump, wall jump points and waiting pirates;
  `tests/test_mzm_runtime_room.py` covers the movement table and `M` lines.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite (407 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0253 - Zero Mission waiting pirates at doors

- `mzm_spawn_waiting_pirates` ports `SpawnWaitingPirates` with
  `SpriteSpawnPrimary`: after a door transition with the alarm on (forced on
  once Mecha Ridley is dead) a waiting pirate (the third one when the
  spriteset has it, else the first) appears at Samus's arrival point, half a
  block back toward the door, with room slot `0x80` (`0x81` through a door
  without hatch) and graphics slot 5 in spritesets 88 and 103; a spriteset
  without pirates stops the alarm.
- `scripts/mzm_runtime_room.py` writes the room's default spriteset as `S`
  lines (`scripts.mzm_sprites.room_spriteset`); the runtime reads them and
  finds the arrival door's type from its position in the new room.
- Not covered: event spritesets, the debug teleport (no door), the alarm's
  music and palette.
- Tests: `tests/test_mzm_sprites.c` covers no spawn without the alarm, the
  alarm stopped by a spriteset without pirates, the first free slot, room
  slot `0x81` and graphics slot 5, the pirate's placement two blocks and an
  eighth back, its wait, the hidden laser through the hatch, its 10-frame
  exit and the alarm forced after Mecha Ridley;
  `tests/test_mzm_runtime_room.py` covers the `S` lines and the default
  spriteset.
- Checked: the 315 game rooms load with their `S` lines (the 15 Test area
  rooms are refused by the existing `--room` alias syntax, unchanged).
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite (409 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0254 - Zero Mission Chozodia security systems

- `src/runtime/mzm_chozodia_security.c` ports `Searchlight`,
  `SecurityLaser`, `SearchlightEye`, `SearchlightEyeBeam` and
  `SearchlightEyeProjectile`; the engine handles `SSC_CHECK_COLLIDING`
  contacts and `SP_IMMUNE_TO_PROJECTILES` (invincible impact, 2-frame stun,
  no contact damage). `scripts/mzm_sprites.py` exports the ten sprites.
- Checked in captures: in chozodia_003 Samus walking into a vertical
  security laser rings the alarm, every laser of the room fades out and
  holds it, and both waiting pirates come out of their doors (one jumps at
  her); in chozodia_016 the four searchlights sweep and flicker and Samus in
  a light brings out the waiting pirate; in chozodia_030 the eyes shoot.
- Divergences: searchlights drawn opaque above the backgrounds, no BG0
  transparency fades (`PrimarySpriteB3`), missiles hitting an immune sprite
  removed instead of tumbling, no sounds.
- Tests: `tests/test_mzm_sprites.c` adds the searchlight sweep, flicker,
  turn, alarm and removal under alarm, the vertical laser size, trip,
  pirate marking, fade and alarm hold, and the eye placement, immunity,
  beam, alarm, shot timing and projectile speed.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite (409 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0255 - Zero Mission room backgrounds

- `scripts/mzm_bg_layers.py` rebuilds BG VRAM and palette RAM as
  `RoomLoadTileset` and `RoomLoadBackgrounds` leave them (common tiles,
  tileset, background graphics, common and tileset palette rows) and draws
  a per-tileset metatile sheet (with the 208 common metatiles), the BG3 and
  LZ77 BG0 text layers; `scripts/mzm_runtime_room.py` exports them with the
  BG0/BG1/BG2 block maps (`L`, `G`, `R`, `T` lines).
- `fusion_room_runtime` draws the native layers: BG3 with `ScrollBg3`
  (including rooms starting from the bottom), BG2, BG1 and BG0 by the
  priorities of the room's transparency value around Samus and the sprites
  (BG1 now passes in front of them as in the game), the BG0 blend
  (`BLDALPHA`: additive at EVA/16, or alpha) and a black backdrop; block
  edges are snapped to output pixels. Rooms without the new lines keep the
  old composite.
- Checked in captures: brinstar_000 shows its BG3 vegetation behind the
  cave, norfair_005 its BG0 bubble columns blended over BG3, chozodia_016 its
  layers.
- Not reproduced: animated tiles and palettes, BG movement effects,
  gradients, windows, the mother ship common tiles.
- Tests: `tests/test_mzm_bg_layers.py` covers the sheet indices, tile
  drawing (flips, banks, transparent color 0), the 32-bit BMP, the
  `BgProperty` values and the room lines.
- Validation on 2026-10-11: warning-clean build, CTest 26/26, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0256 - Zero Mission hatches as native blocks

- `src/runtime/mzm_hatches.c` ports `ConnectionUpdateHatches`,
  `ConnectionUpdateHatchAnimation`, `ConnectionHatchFlashingAnimation`,
  `ConnectionHatchStartLockAnimation`, `ConnectionOverrideOpenedHatch` and
  `BgClipCheckOpeningHatch`. Hatches change their four BG1 blocks and
  Clipdata (collision from the common tilemap types); the runtime builds
  them like `ConnectionLoadDoors`, closes the hatch Samus comes through,
  remembers opened hatches per area (`ConnectionSetHatchAsOpened`) and
  routes beam, missile and pirate laser hits to them. The diagnostic tints
  remain only for rooms without the layered backgrounds.
- Rooms export `sHatchesTilemapValues` (`V`),
  `sHatchesAnimationDurationCounter` (`A`), `sHatchBehaviors` (`W`) and the
  common tilemap collision types (`K`).
- BG palette rows 1-2 now come from `sDoorTransitionPal`, which follows the
  one-row `sCommonTilesPal` in ROM and is read by `RoomLoadTileset`'s copy:
  the doors and hatches are drawn with their colors.
- Checked in brinstar_000: the door frame and its blue hatch are drawn and
  a beam opens it through the grey frames.
- Tests: `tests/test_mzm_hatches.c` (opening, block values per frame,
  super missiles, closing behind Samus, override, locked hatches) and the
  runtime hatch test in `tests/test_runtime_steep_slopes.c`.
- Validation on 2026-10-11: warning-clean build, CTest 27/27, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0329 - native Zero Mission escape timer graphics

- Replaced the provisional host-drawn countdown with ParticleEscape's native
  OBJ path inside the PPU-backed HUD. It copies both 352-byte tile rows from
  `sEscapeTimerDigitsGfx` to OBJ VRAM at `0x7800` and `0x7c00`.
- The six digits and two separators now use the native eight 8x16 objects,
  palette row 3, tile indices and absolute positions from
  `sParticleEscapeOam_Frame0`. The objects remain available independently of
  the regular HUD OAM visibility.
- Removed the duplicate SDL glyph renderer. Synthetic-ROM tests verify both
  VRAM copies, all eight OAM entries, digit ordering, separators, and cleanup
  after the countdown ends.
- Remaining boundary: escape failure is requested but not yet connected to
  Game Over; successful take-off still needs the exit fade and the full
  Tourian escape cutscene before the Chozodia continuation.
- Validation on 2026-10-14: warning-clean build, CTest 35/35, Python 471/471,
  regenerated native parity and dashboard data, fresh generated-file checks,
  and the proprietary-file guard passed.

## Patch 0330 - Zero Mission failed-escape transition

- `MzmEscapeFailure` ports `ColorFadingProcess_EscapeFailed`: its initial
  transition frame, the eight native white brightness levels, the completed
  fade frame, and the 61-frame white hold before `GM_GAMEOVER`.
- Expiration now freezes Samus and the countdown, issues the native 60-frame
  `FadeAllSounds` and `FadeMusic` requests, keeps the `00:00.00` OBJ timer
  visible under the fade, then enters the already ported native Game Over
  screen with the current suit.
- Tests verify both audio requests, every brightness level, stage boundaries,
  the exact hold duration, and the terminal Game Over request.
- Remaining boundary: successful take-off still needs
  `BACKGROUND_EFFECT_EXIT_ZEBES_FADE`, `COLOR_FADING_TOURIAN_ESCAPE`, and the
  full Tourian escape cutscene before the Chozodia continuation.
- Validation on 2026-10-14: warning-clean build, CTest 35/35, Python 471/471,
  regenerated native parity and dashboard data, fresh generated-file checks,
  and the proprietary-file guard passed.

## Patch 0331 - Zero Mission successful-exit fades

- The gunship's `EFFECT_CUTSCENE_EXITING_ZEBES` request now starts a native
  42-frame transition: all fifteen
  `BACKGROUND_EFFECT_EXIT_ZEBES_FADE` levels at their alternating command/wait
  cadence, followed by the four stages and eight brightness updates of
  `COLOR_FADING_TOURIAN_ESCAPE`.
- Gameplay continues during the background effect, then freezes Samus when
  the global color fade begins. At completion the runtime leaves gameplay at
  the `GM_TOURIAN_ESCAPE` boundary and deliberately keeps the final white
  frame; it does not fabricate a direct jump to Chozodia.
- Tests verify every background brightness value, all phase boundaries, the
  eight global color updates, the exact 42-frame duration, and the terminal
  Tourian-escape request.
- Remaining boundary: `TourianEscapeInit`, its twelve PPU scenes, the
  `CUTSCENE_COULD_I_SURVIVE` handoff, and Chozodia room 0 are not connected.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 471/471,
  regenerated native parity and dashboard data, fresh generated-file checks,
  and the proprietary-file guard passed.

## Patch 0332 - Tourian escape first PPU stage

- The successful exit no longer stops on its white boundary. The room runtime
  now enters a dedicated `GM_TOURIAN_ESCAPE` state whose PPU is rendered by the
  same non-gameplay screen path as the title, menus and story cutscenes.
- `TourianEscapeInit` loads the native Tourian room, explosion/debris graphics
  and OBJ palette. `TourianEscapeZebesExploding` follows the native 513-frame
  boundary, including the room changes to Tourian 5 and Crateria 1, the four
  brightness flashes, sound requests, cutscene music handoff, and the switch at
  frame 248 to the native Zebes background with the departing ship and moving
  explosion objects.
- ROM-backed screen tests verify the three room identities, display modes,
  audio requests, planet rendering, OAM output and exact first-stage endpoint.
- Remaining boundary: the exploding-room block tilemaps and their explosion
  objects are not drawn yet. Stages 2-12, beginning with Samus in her ship,
  remain to be ported before `CUTSCENE_COULD_I_SURVIVE` and Chozodia room 0.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0333 - Tourian cockpit and room explosions

- `TourianEscapeProcessOam` now drives the first scene's four native explosion
  animations and five falling debris objects directly from the ROM `FrameData`
  tables. Their position cycling, animation timers and two-pixel room shake
  follow the native handler through all three room views.
- The 373-frame `TourianEscapeSamusInHerShip` stage is connected after Zebes:
  Power/Varia palette selection, suited and suitless graphics, the expanding
  brightness window at frame 56, three removal sounds, the BG0/BG1 alpha fade,
  and the opened-eyes tilemap at frame 352 all run on the shared GBA PPU.
- `--tourian-escape` starts this sequence directly for captures. Manual captures
  verified the room explosions, the centred suited cockpit image and the
  suitless cross-fade; the ROM-backed screen test covers their registers,
  objects, audio requests, palettes and exact stage boundaries.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 3-12 beginning with Samus looking around are not ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0334 - Tourian looking-around scene

- The 57-frame `TourianEscapeSamusLookingAround` stage now follows the cockpit
  sequence. It loads the shared native BG/OBJ graphics, both cockpit tilemaps
  and the seven palette rows directly from the private ROM bank.
- Its two four-part, alpha-blended OAM images start above and below the screen,
  converge by eight pixels per frame on Samus's face, and stop together at the
  vertical centre. BG0 changes from the left-looking to the right-looking
  tilemap on native frame 32.
- The ROM-backed screen test verifies the register setup, palette, eight OAM
  parts, convergence cadence, tilemap switch and exact transition to stage 4.
  Manual diagnostic captures verified the left, converged and right views.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 4-12 beginning with `TourianEscapeSamusSurrounded` are not ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0335 - Tourian surrounded affine scene

- The 113-frame `TourianEscapeSamusSurrounded` stage now follows the
  looking-around scene. It loads the native starfield/cockpit BG0 and the
  256-colour affine BG2 view directly from the private ROM bank.
- BG2 starts centred at the native 42/256 matrix scale, then zooms out between
  frames 32 and 47 by the exact fixed-point increments until its 256/256
  matrix reveals Samus's ship encircled by the pirate formation.
- The shared PPU's register indexing no longer aliases the `0x20-0x3F` BG2/BG3
  affine register range over `0x00-0x1F`. `DISPCNT` now remains independent
  while `BG2PA`, `BG2PD`, `BG2X` and `BG2Y` are updated every frame.
- The ROM-backed screen test verifies the two layer configurations, full BG
  palette, starting/intermediate/final affine state, rendered output and exact
  transition to stage 5. Manual captures verified the close, moving and fully
  revealed compositions.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 5-12 beginning with `TourianEscapeSamusFlyingIn` are not ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0336 - Tourian ship flying-in scene

- The 33-frame `TourianEscapeSamusFlyingIn` stage now follows the surrounded
  view. It reuses the intro's native starfield, palette and ship graphics from
  the private ROM bank and plays the dedicated flying-in sound.
- The six- and nine-part ship OAM poses alternate every four frames. Their
  double-size affine coordinates and four flip matrices follow
  `ProcessComplexOam` and `CalculateOamPart4`, while the ship's vertical speed
  eases through the native 8/6/3/1/-1 pixel steps and its scale rises from 32
  to 480.
- The ROM-backed screen test verifies both OAM poses, affine parameters,
  palette over-read used by the original DMA, blend/display registers,
  position and scaling boundaries, sound request and exact transition to
  stage 6. Manual captures verified the small, intermediate and full-size
  ship over the starfield.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 6-12 beginning with `TourianEscapeSamusChasedByPirates` are not
  ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0337 - Tourian chased-by-pirates scene

- The 257-frame `TourianEscapeSamusChasedByPirates` stage now follows the
  flying-in shot and reaches the firing-stage boundary. It loads the native
  scrolling starfield, separate BG/OBJ palettes and ship graphics from the
  private ROM bank.
- Six particles use the native alternating OAM tables, four-frame animation
  cadence, speed table and off-screen recycling order. Samus and the three
  pirate ships follow their exact activation frames, coordinates, diagonal
  velocities, pose changes and four sound cues in both directions.
- The runtime Tourian object capacity now matches the native ten entries;
  earlier room-explosion loops retain their intentional nine-entry limit.
  The ROM-backed screen test covers palette separation, background scrolling,
  particle recycling, all ship waves and sounds, animated pirate OAM and the
  exact transition to stage 7.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 7-12 beginning with
  `TourianEscapeSamusChasedByPiratesFiring` are not ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0338 - Tourian pirates-firing scene

- The 385-frame `TourianEscapeSamusChasedByPiratesFiring` stage now follows
  the chase and reaches the getting-shot boundary. It reuses the starfield and
  the six projectile states inherited from the previous scene, switches them
  to vertical motion and loads the dedicated native ship/projectile graphics
  and OBJ palette from the private ROM bank.
- Samus follows all eight native velocity and acceleration paths, including
  the capped fixed-point movement, pauses and direction changes. Her ship and
  the pirate formation use their complete four-frame OAM pose cadence with
  independent complex-affine matrices; the pirates shrink first slowly and
  then twice as quickly before disappearing.
- The display, vertical background drift, three sound cues, off-screen exit
  and exact 385-frame boundary match the source. The ROM-backed test also
  verifies carried projectile state, affine parameters, trajectory phases,
  scaling thresholds and the invisible tail after frame 192.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 8-12 beginning with `TourianEscapeSamusGettingShot` are not
  ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0339 - Tourian getting-shot scene

- The 401-frame `TourianEscapeSamusGettingShot` stage now follows the firing
  chase and reaches the going-to-crash boundary. It loads the dedicated
  background, tilemap, ship graphics and five palette rows from the private
  ROM bank while retaining the six projectile states carried between scenes.
- The scene reproduces the native all-layer brightness fade, horizontal
  starfield drift and projectile recycling. Samus enters on the exact
  fixed-point deceleration curve, then follows the independent capped X/Y
  oscillators after the hit, including the native alias between the vertical
  pause counter and projectile 4's animation counter.
- Frame 336 removes the ship, transfers the palette to BG memory, switches to
  the dedicated BG1 impact view and plays the decisive-shot sound. The
  ROM-backed regression test covers carried state, fade intervals, OAM,
  trajectory and oscillator snapshots, both rendered views and the exact
  transition after frame 400.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 9-12 beginning with `TourianEscapeSamusGoingToCrash` are not
  ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0340 - Tourian going-to-crash scene

- The 81-frame `TourianEscapeSamusGoingToCrash` stage now follows the
  decisive shot and reaches the crashing boundary. It restores the Zebes
  background, loads the damaged ship graphics and copies the native BG and
  OBJ palettes from the private ROM bank.
- The ship and foreground wake use the native semi-transparent OAM path with
  the 9/7 alpha coefficients. Its one-pixel vibration and both smoke trails
  follow the exact timing and cumulative eight-step ROM tables, including
  their independent resets and mirrored vertical movement.
- The fire palette alternates every four frames, the background drifts every
  six frames and the free-falling sound starts with the visible scene. The
  ROM-backed regression test covers both smoke activations and recycling
  points, palette phases, blended OAM, scrolling, rendering and the exact
  transition after frame 80.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 10-12 beginning with `TourianEscapeSamusCrashing` are not ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0341 - Tourian crashing scene

- The 289-frame `TourianEscapeSamusCrashing` stage now follows the falling
  ship and reaches the looking-at-sky boundary. It loads both native BG
  layers, the complete 256-colour OBJ palette and the 24-part ship from the
  private ROM bank.
- A second object enters at frame 10, crosses the screen on the exact -4/+2
  path and cycles through its four ROM OAM compositions every eight frames
  before disappearing at frame 64. Both objects retain the native priority 2
  override independently of their authored OAM priorities.
- The collision at frame 104 swaps the ship composition, reveals the BG1
  explosion, changes the seven BG palette rows and starts the collision sound.
  Brightness then reaches 16 on frame 135 and remains fully white through the
  native frame-288 boundary.
- The ROM-backed regression test covers asset palettes, OAM counts and
  priorities, every debris pose, both sounds, both rendered phases, brightness
  cadence, the white hold and the exact transition state.
- Remaining boundary: the first scene's block-based room layers are absent,
  and stages 11-12 beginning with `TourianEscapeSamusLookingAtSky` are not
  ported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0342 - Tourian looking-at-sky scene

- The 433-frame `TourianEscapeSamusLookingAtSky` stage now follows the crash
  and reaches the final mother-ship-view boundary. It loads the three native
  tilemaps, their two character blocks, the pirate ship graphics and the ten
  shared BG/OBJ palette rows from the private ROM bank.
- The stage preserves the full-white state inherited from the collision and
  reveals the new view on the native every-other-frame cadence. Two pirate
  ships enter at frames 16 and 24, move +2/-3 per frame with priority 2 and
  leave independently at frames 96 and 104.
- Frame 160 begins the long pan: BG0 and BG1 move immediately, while BG2 waits
  until their offset reaches 96 and stops the phase at 160. The resulting
  159/159/160 offsets remain fixed until frame 432 clears the backdrop and
  display to black.
- The ROM-backed regression test covers palettes, BG configuration, reveal
  cadence, both OAM passages, sound, rendered output, every pan threshold and
  the exact final transition state.
- Remaining boundary: the first scene's block-based room layers are absent,
  and only stage 12, `TourianEscapeSamusLookingAtMotherShip`, remains unported.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0343 - Tourian final scene and Chozodia handoff

- `TourianEscapeSamusLookingAtMotherShip` completes the twelfth native escape
  stage. It loads both ROM-backed background images and tilemaps, their ten
  palette rows, and ten delayed rain objects with the native alternating OAM
  frames and diagonal movement.
- `TextStartStory(STORY_TEXT_THE_TIMING)` and the fullscreen branch of
  `TextProcessStory` now render the English message directly into OBJ VRAM.
  At frame 224 its native twelve-part semitransparent composition fades over
  the scene while the backgrounds stop drifting and the brightness decrease
  reaches level four.
- Frame 448 remains held until a newly pressed A or B. The following update
  ends the Tourian mode and starts `CUTSCENE_COULD_I_SURVIVE`, which is now a
  supported story-text cutscene rather than a diagnostic rejection.
- Closing the monologue preserves the campaign state, activates the suitless
  Power Grip plus Long/Charge Beam subset, and loads `chozodia_000` at door 0.
  The native suitless item-acquisition map animation and paralyzer pistol are
  still absent, so this handoff enters the room directly.
- The ROM-backed regression test covers asset and palette destinations, rain
  delay/OAM state, both background offsets, text generation, blending and
  brightness thresholds, the exact held frame, edge-triggered dismissal and
  selection of the post-Tourian story text.
- Remaining boundary: supply the first Tourian scene's block-layer tilemaps,
  then port the suitless acquisition screen and paralyzer pistol.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  all native rebuild tasks up to date, regenerated parity and dashboard data,
  fresh generated-file checks, and the proprietary-file guard passed.

## Patch 0344 - Suitless paralyzer pistol

- `SamusCheckFirePistol` and the suitless branch of `SamusCheckNewProjectile`
  now run in the weapon controller. The pistol charges automatically, fires a
  normal shot before frame 112 or a charged shot afterwards, wraps from 127
  back to 112, and pauses or clears the counter in the native pose groups.
- `ProjectileProcessPistol` and `ProjectileProcessChargedPistol` use their
  native six/two shot limits, seven-frame cooldown, directional hitboxes,
  22-subpixel speed, unlimited lifetime, sounds and `CAA_BOMB_PISTOL` block
  action. The charged shot emits `PE_CHARGED_PISTOL_TRAIL` every eight frames.
- The weak shot never damages a sprite. The charged shot deals 12 damage to
  compatible enemies, while a Space Pirate instead loses its standing state
  and is paralysed for 60 suitless ticks with the native black/purple warning
  flicker.
- The projectile exporter now stages the pistol palette and graphics for its
  muzzle effects and charged trail. A private rebuild produced 566 projectile
  sequences and confirmed the pistol OAM, shot flash and trail entries.
- ROM-free tests cover automatic charging and wrap, pose resets, spawn type,
  movement, trail, hitboxes, block action, harmless hits, charged damage and
  the Space Pirate-specific freeze and palette timing.
- Remaining boundary: port the suitless item-acquisition map sequence between
  the post-Tourian monologue and Chozodia, then capture and compare the pistol
  against the original game in a pirate room.
- Validation on 2026-10-15: warning-clean build, CTest 35/35, Python 436/436,
  every rebuild task up to date, regenerated parity and dashboard data, fresh
  generated-file checks, and the proprietary-file guard passed.

## Patch 0345 - Zero Mission event-driven room sprites

- Room export now retains all three native sprite configurations: the default
  placements and spriteset plus the first- and second-event variants. The
  runtime parses the new `F`, `J` and `N` records and applies the native
  second-event, first-event, default priority whenever a room is loaded.
- Save difficulty is synchronized to `EVENT_EASY` and `EVENT_HARD`, allowing
  the same mechanism to select difficulty-specific placements. Waiting Space
  Pirates also consult the selected spriteset instead of the default one.
- The projectile-only charged-pistol trail no longer disables the entire
  enemy sprite engine: effect timing first uses the enemy sprite library and
  may use the pistol variant from the projectile library, with an exact
  missing-table diagnostic.
- ROM-free tests cover variant extraction, private room serialization, event
  priority and difficulty flags. A direct runtime capture of `brinstar_009`
  contains no sprite by default and loads the native
  `DESSGEEGA_AFTER_LONG_BEAM` placement when
  `EVENT_VIEWED_STATUE_ROOM` is active.
- Remaining boundary: port the statue-room interaction that sets
  `EVENT_VIEWED_STATUE_ROOM` during ordinary play, then validate the early
  Morph Ball-to-Long Beam progression without debug events.
- Validation on 2026-10-16: warning-clean build, CTest 35/35, Python 438/438,
  every private rebuild task up to date, direct default/event room captures,
  regenerated parity and dashboard data, fresh generated-file checks, and the
  proprietary-file guard passed.

## Patch 0346 - Natural Long Beam encounter trigger

- The Kraid and Ridley statues are now exported and instantiated in
  `brinstar_010` with their native idle OAM, room placements, half-block
  horizontal offsets, draw ranges, collision-free state and proximity-based
  priority.
- `KraidStatueInit` now performs its early-game progression side effect:
  entering the room sets `EVENT_VIEWED_STATUE_ROOM` in the session event
  flags. A same-process capture then teleported back to `brinstar_009` and
  loaded `DESSGEEGA_AFTER_LONG_BEAM` automatically, without `--event`.
- The sprite exporter treats both boss statues as non-freezable, X-flip-only
  objects while reserving the three flashing palette rows for Chozo statues.
  A private rebuild staged their ten native animation tables without emitting
  spurious boss-statue palette variants.
- Scripted captures can now display the debug menu safely: host input polling
  accepts the intentionally absent gamepad used by capture mode. This also
  makes the event-persistence room-chain regression possible.
- ROM-free tests cover both statue placements, exporter registration, the
  Kraid-only event, native offsets, idle animation, draw parameters and
  priority range.
- Remaining boundary: traverse the early Morph Ball-to-statue-to-Long Beam
  route through ordinary doors and complete the Dessgeega encounter and Long
  Beam acquisition without debug navigation. The statues' post-boss opening
  sequences remain a later progression milestone.
- Validation on 2026-10-16: warning-clean build, CTest 35/35, Python 438/438,
  fresh private room and sprite rebuilds, direct statue-room and same-session
  encounter captures, regenerated parity and dashboard data, generated-file
  checks, and the proprietary-file guard passed.

## Patch 0347 - Assertable early-game capture routes

- Headless Zero Mission captures can now declare their required final state:
  repeatable `--expect-event N` and `--expect-item NAME` checks plus
  `--expect-room ALIAS`. A failed expectation reports the missing state and
  exits with status 3; successful capture logs include the final room and an
  explicit confirmation.
- `--item` and `--expect-item` share one item-name parser, preventing their
  command-line spellings from drifting. Acquired unknown items satisfy an
  item expectation even while they remain inactive before the Fully Powered
  Suit.
- Private route probes now verify three early Long Beam segments: entering
  `brinstar_010` sets event 42 and the real right hatch reaches
  `brinstar_009`; killing the event Dessgeega sets event 43, unlocks the
  hatch and reaches `brinstar_030`; the acquired Bomb hint state can be
  crossed normally and its right hatch reaches `brinstar_003` while event 9
  and Morph Ball remain present.
- The full Bomb hint interaction was also replayed from its standing state:
  Morph Ball grab, hint map, sitting animation, refill, banner dismissal and
  release all complete. This room is a required progression interaction, not
  an optional shortcut around the Dessgeega encounter.
- Remaining boundary: descend `brinstar_003`, cross `brinstar_004`, enter
  `brinstar_005`, and make a capture expectation pass only after the Long
  Beam ball has been revealed and collected.
- Validation on 2026-10-16: warning-clean build, CTest 35/35, Python 438/438,
  passing and deliberately failing capture expectations, three private
  early-route smoke captures, refreshed dashboard data, generated-file
  checks, and the proprietary-file guard passed.

## Patch 0350 - Zero Mission Gadora hatch encounters

- Added the Kraid and Ridley Gadora primaries, their invisible vulnerable eye
  and Ridley's beam to the native sprite engine. Both actors periodically
  detect Samus in front, warn, open their eye for the native duration and
  close immediately after a strong hit.
- Gadora placement now suppresses the matching closed hatch through a room
  hook. Killing the eye transfers the death to the body; its forty-frame
  flashing sequence restores the hatch and persists event 28 or 36 so the
  passage remains open after reloading the room.
- The Kraid variant uses its high-priority graphics and one-health eye. The
  Ridley variant selects the alternate palette, stays open longer and emits
  the delayed horizontal beam, including collision explosion and sounds.
- Normal death keeps the native two-drop behavior: the main actor may become
  its weighted energy or missile drop and then creates the follower drop.
  `scripts/mzm_sprites.py` exports all nine shared OAM tables while private
  graphics and ROM data remain outside Git.
- The focused sprite test covers both variants, eye exposure, forced body
  death, persistent hatch restoration, the extra drop and beam delay/motion.
  Native BG priority is represented by the runtime draw order, and direct
  emulator comparison remains open.
- Validation on 2026-10-16: warning-clean build, CTest 35/35, Python 473/473,
  focused Kraid/Ridley Gadora assertions, regenerated parity/dashboard data
  and the proprietary-file guard passed; both `kraid_029` and `ridley_011`
  runtime room checks loaded their native collision successfully.

## Patch 0351 - Zero Mission Imago larva encounter

- Added both `norfair_042` Imago larva primaries and their shared secondary
  shell, claws and three light actors. The left and right bodies keep the
  native independent `gSubSpriteData2` and `gSubSpriteData1` state while all
  six native multi-sprite tables drive their exact part offsets and OAM.
- The left larva now locks the room and starts the second Caterpillar battle
  track. The right larva detects Samus from its cocoon, warns, withdraws its
  lights, lunges back to the passage and retreats at one, four or eight
  subpixels per frame according to shell damage. Each retreat extends the
  following stun up to the native limit.
- Body damage selects the taking-damage choreography and its 47-frame
  counterattack. The 100-frame death synchronizes the parts and their native
  weighted drops, persists event 31, unlocks the doors after one second and
  starts the boss-killed music. Reloading the completed room removes both
  larvae.
- `scripts/mzm_sprites.py` exports the 23 Imago larva OAM tables, their palette
  rows and the six multi-sprite tables. The runtime now also retains native
  secondary-sprite drop probabilities instead of accidentally consulting a
  primary sprite row with the same numeric id.
- The focused sprite scenario covers both bodies, independent shared state,
  shell retreat speed and delay, warning immunity, body damage, synchronized
  death, drops, event persistence, door timing and music. The encounter still
  needs a frame-by-frame emulator comparison for palette and part alignment.
- Validation on 2026-10-17: warning-clean full build, CTest 35/35 and Python
  438/438 passed. The focused sprite scenario passed, and the real
  `norfair_042` room check loaded its native 784x224 geometry and 540 collision
  cells. Parity/dashboard data were regenerated and the proprietary-file guard
  passed.

## Patch 0352 - Zero Mission Imago right-side passage remnant

- Added the separate `IMAGO_LARVA_RIGHT_SIDE` actor used by `norfair_046` to
  the sprite exporter and native runtime. Its full 34-frame OAM loop is now
  loaded from the private ROM-derived library without adding proprietary data
  to Git.
- The remnant keeps the native one-pixel hitbox, extended draw distances,
  projectile immunity and disabled Samus collision. It remains present
  independently of the Caterpillar event, matching the original actor rather
  than incorrectly treating it as another combat larva.
- Animation-frame starts now emit the native shaking cue at frame 0 and the
  six core-heartbeat cues at frames 11, 15, 19, 23, 27 and 31. The focused
  sprite regression covers the actor setup, all sound points and the exported
  frame-duration pattern.
- Consolidated the actor into the existing Imago parity record and dashboard
  family instead of creating a second feature document. The real
  `norfair_046` room check loaded its native 784x224 geometry and 554 collision
  cells.
- Validation on 2026-10-17: warning-clean full build, CTest 35/35 and Python
  438/438 passed. Generated parity/dashboard data were fresh and the
  proprietary-file guard passed.

## Patch 0353 - Zero Mission Imago tunnel discovery and cocoon remains

- Added the invisible `norfair_043` passage trigger with its native collision
  box. Touching it persists event 32, removes the actor and exposes the door
  route into Ridley's tunnel; an already discovered passage suppresses the
  trigger when the room loads.
- Added the `ridley_028` post-fight cocoon remnant and its defeated cocoon and
  broken ceiling-vine secondary actors. They retain the native point hitboxes,
  projectile immunity, collision behavior, OAM and heartbeat sound timing.
- `scripts/mzm_sprites.py` now exports the complete 21-table Imago cocoon
  graphics family and its body palette variants to the ignored private sprite
  library. The active fight is deliberately not dispatched yet: its six
  destructible vines, spores, fall, floor destruction and battle sequence are
  the next implementation slice.
- Raised the private runtime sprite-library capacity from 16,384 to 32,768
  sequences. The complete current export contains 17,085 sequences and
  101,548 frames, so the previous limit silently rejected the whole library
  once the cocoon tables were added.
- The focused regression covers trigger setup, event persistence, collision
  activation, remnant setup and both heartbeat points. A real headless capture
  in `norfair_043` reached the trigger, set event 32 and removed it; both that
  room and `ridley_028` loaded their native geometry and collision data.
- Validation on 2026-10-17: warning-clean full build, CTest 35/35 and Python
  438/438 passed. The focused sprite scenario and real trigger capture passed,
  parity/dashboard data were regenerated, and the proprietary-file guard
  found no tracked ROM-derived payload.

## Patch 0354 - Zero Mission active Imago cocoon and spores

- Dispatched the active `IMAGO_COCOON` placement in `norfair_043`. The actor
  now locks the arena and drives its body, two decorative growths, central
  ceiling vine and six destructible vines through the exact idle and dying
  multi-sprite tables exported from the pinned decompilation.
- The six combat vines use their native individual hitboxes, draw orders,
  50 health, missile and power-bomb weakness from the secondary stats, damage
  sound and huge death explosion. The cocoon turns to palette row 1 after the
  third vine and row 2 after the sixth, persists event 33 and begins its native
  falling animation with the vine-cracking sound.
- The left decorative actor alternates the native left and right nests every
  256 frames. Each nest contains eight synchronized spores; after the spawn
  animation and one-second hold they become vulnerable, harmful projectiles
  moving in the eight native directions, explode against solid Clipdata and
  use the native spawn and release sounds.
- The idle sound and two-stage encounter music trigger are connected. Reloading
  event 33 restores the cocoon in the tunnel with the broken ceiling vine until
  the Ridley entrance demo has played.
- A real frame-3 capture in `norfair_043` now contains the visible cocoon and
  all nine synchronized vine actors (11 room actors including the passage
  trigger). The private graphics library remains 17,085 sequences and 101,548
  frames; only the two compact multi-sprite tables were added to private
  metadata.
- Still open: the Winged Ripper, the falling movement, ceiling debris, five
  floor blocks, crash, final two passage blocks, door unlock and boss-killed
  music sequence.
- Validation on 2026-10-17: warning-clean full build, CTest 35/35 and Python
  438/438 passed. The focused encounter scenario, real room capture,
  regenerated parity/dashboard data and proprietary-file guard all passed.

## Patch 0355 - Zero Mission Imago cocoon collapse and passage unlock

- Completed the active cocoon's second phase from the pinned native routines.
  Once the dying multi-sprite animation exposes the body, it accelerates down,
  emits the native ceiling-debris pattern, breaks the centre floor block, then
  removes the two middle and two outer blocks at their original timings. The
  lower-floor collision aligns the static body, restores its palette and scale,
  shakes the screen, plays the crash and starts the 85-frame music fade.
- After 90 visible frames the cocoon removes both right-side passage blocks,
  sets the native negative door timer, plays the boss-killed music and enters
  its four-frame breathing cycle. Decorative growths and the dying ceiling vine
  now burst and disappear at the crash, while event 33 continues to restore the
  persistent in-ground state on reload.
- Added the native Winged Ripper actor. It uses the ROM-exported 320-value
  `sSineTable`, native turn animations, direction flags and elliptical route;
  the falling cocoon crushes it after the original delay and its death releases
  a standing Samus. The shared standing bridge now carries both Samus
  coordinates, so she follows the Ripper across the complete ellipse.
- Extended the private sprite-table loader buffer to 4 KiB. The previous 1 KiB
  line buffer truncated `sSineTable` and rejected it plus every following AI
  table; a real load exposed the issue before capture.
- The focused ROM-free scenario now covers the Ripper, staged five-block floor
  destruction, landing, screen shakes, sounds, 90-frame delay, final two-block
  opening, door timer and victory music. A real frame-3 `norfair_043` capture
  loaded all tables and rendered 12 actors, including the moving Ripper beside
  the complete cocoon.
- Validation on 2026-10-17: warning-clean full build, CTest 35/35 and Python
  438/438 passed. The private sprite rebuild, focused encounter scenario, real
  room capture, regenerated parity data and proprietary-file guard passed.

## Patch 0356 - Zero Mission Kraid and Ridley boss statues

- Completed both Brinstar boss-statue state machines. Their matching boss-death
  events now activate the eyes, lock the doors and wait for the statue room to
  be centred before Kraid opens immediately and Ridley starts his native
  one-second delay.
- Reproduced the shared temporary six-block wall, each statue's interior
  Clipdata removal and final three-block floor writes. Kraid keeps the route
  locked when Ridley is dead but not yet open; Ridley's completion persists the
  second opened event, removes the wall and starts the native delayed unlock.
- Added both opening debris streams, vertical screen shakes and eye/opening
  sounds. Kraid owns the native room camera lock before the gate and releases it
  outside the smaller range after both statues open. The host's optional
  320-pixel-wide room view also accepts its 160-pixel centre alongside the
  original 120-pixel GBA trigger.
- Reloading `brinstar_010` with either opened event restores that statue's final
  animation and collision immediately without replaying the sequence. The
  focused test covers idle initialization, activation, both timings, every
  Clipdata phase, shared door logic, camera lock and persistent reload state.
- Remaining visual divergence: the original Kraid actor moves every active
  projectile to low OAM priority while it exists; the runtime renderer does not
  yet expose that per-projectile layer flag.
- Validation on 2026-10-17: the warning-clean full build, CTest 35/35 and
  Python 473/473 passed. A 600-frame real `brinstar_010` capture with boss
  events 30 and 37 completed both animations, set opened events 53 and 54 and
  rendered the cleared route. Generated-data checks were fresh and the
  proprietary-file guard passed.

## Patch 0357 - native low-OAM projectile pass

- Added the native `PROJ_STATUS_LOW_OAM_PRIORITY` state to projectile slots.
  Kraid's statue marks every active shot on every update, and a reused slot
  clears the state through the existing projectile initialization path.
- Split the SDL projectile renderer into the native high- and low-priority
  passes. Ordinary low-priority shots are painted before the other OBJ actors,
  while Wave, Wave-Plasma and tumbling shots retain their above-BG1 band and
  are ordered before its regular projectiles.
- Extended the statue regression with multiple active projectile slots and an
  inactive control slot. A real 600-frame `brinstar_010` capture fired normal
  beams throughout both openings, completed events 53 and 54, crossed the
  unlocked right hatch and reached `brinstar_009`.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 473/473 passed. Generated-data checks were fresh and the
  proprietary-file guard passed.

## Patch 0358 - native Deorem body choreography

- Replaced Deorem's fixed body layout with the native staged construction.
  The first six segments now descend from the initial head, the opposite six
  rise only after the underground crossing, and the linked three-piece neck
  appears with the eye before both columns grow their dedicated tail caps.
- Each segment now consumes `sDeoremSegmentXVelocity` as a velocity stream,
  follows the head vertically during attacks and reverses with the retracting
  head. Thorn volleys still originate from the four correct segments on the
  selected side.
- Restored the retreat chain instead of deleting the body at once. The lower
  column sinks, the upper column climbs and rolls its frames, the neck plus
  three trailing pieces follow the centred head into the floor, and each part
  uses its native despawn delay. Death likewise keeps the body and sequences
  its explosions from the neck through both columns and tail.
- Added a ROM-free encounter regression covering the ordered two-column
  emergence, segment coordinates, neck linking, both movement directions,
  tail caps, first-location retreat event, three-piece leaving chain, burrow
  transition and staggered death timers.
- Remaining divergence: the head attack uses compact host timing rather than
  the original diagonal boundary choreography. Native music fades, minimap
  updates and emulator comparison also remain open.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 473/473 passed. Real frame-260 captures in `brinstar_012` and
  `brinstar_019` each reached the open-eye phase with all 17 body pieces and
  the eye inside the real 24-slot sprite budget. Generated-data checks were
  fresh and the proprietary-file guard passed.

## Patch 0359 - native Deorem head attack

- Replaced the compact Deorem lunge with the native head state machine. The
  intact eye forces a straight plunge after two warning-animation cycles and
  an eight-frame pullback; a damaged eye can instead select the accelerating
  diagonal branch, retain Samus's initial destination and decelerate inside
  the native arena bounds.
- Restored the exact vertical ranges and health-dependent speeds, the
  half-second exposed-eye pause, the short diagonal correction toward Samus,
  both body-column reversals and the changing head hitbox during retraction.
  The thorn branch now uses the original health and RNG selection, 255-frame
  duration, health-dependent jaw-closing thresholds and one-second recovery.
- The retreat now waits for the closing animations instead of a host timer.
  Deorem starts the Worms Battle track on emergence and fades back to
  Brinstar over 50 frames when retreating or dying. The earlier checklist
  reference to missing minimap changes was removed: the native Deorem source
  does not perform any minimap update.
- Expanded the ROM-free encounter regression across the full-health plunge,
  right and left diagonal attacks, warning mosaic, jaw vulnerability,
  retraction, thorn selection, body direction changes and both music calls.
- Remaining divergence: the eye still uses compact tracking, pulsing and
  death movement. The random debris choreography and the retreat wait for
  active item drops are also incomplete; emulator comparison remains open.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed. A real frame-400 `brinstar_012` capture reached the
  retracting head with the eye exposed and all 17 body pieces active after
  the native plunge. Every reconstruction task completed or was up to date,
  generated-data checks were fresh and the proprietary-file guard passed.

## Patch 0360 - native Deorem eye choreography

- Replaced the eye's four-quadrant approximation with `DeoremEyeMove`. It now
  selects the native eight angular sectors from Samus's adjusted centre and
  rotates two units per frame along the original wraparound branches while
  remaining anchored four subpixels left and 28 above the head.
- Restored the separate idle-init frame and the eye's animation scheduler.
  Each completed pulse has the native 10/16 chance to rest, with its RNG value
  selecting a 32-to-68-frame delay before pulsing resumes. The 900/1800-frame
  encounter timer still begins only after damage or a previous encounter.
- Missile hit stun now transfers to the head on the exact 16-frame edge,
  closes its jaw immediately, makes the eye temporarily projectile-immune,
  plays the fast closing sound and selects the low-health body palette at 20
  health or less.
- Restored the death choreography: the eye follows and spins with the falling
  head, doubles its spin during the final half-second, then spends 120 frames
  moving toward the arena centre, rising for the final 99, alternating the
  native stun palette and finally creating the Charge Beam with its sound.
  Primary spawning also preserves the GBA's current-sprite copy semantics, so
  the disappearing eye cannot recycle its own slot during that frame.
- The ROM-free regression now covers initialization, two opposed tracking
  sectors, pulse/rest timing, fight-timer activation, hit transfer, both spin
  rates, palette flashing, recentering, ascent and the separate Charge Beam
  actor. A real `brinstar_012` capture reached the thorn cycle with the new eye
  logic and the complete 17-piece body active.
- Remaining divergence: the encounter's timed debris patterns and its wait
  for active item drops before retreat are still compact. Emulator comparison
  also remains open.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed. The frame-700 real-room capture completed the native
  thorn transition without losing the eye or any body actor. Every
  reconstruction task completed or was up to date, generated-data checks were
  fresh and the proprietary-file guard passed.

## Patch 0361 - native Deorem debris and retreat gate

- Replaced the compact encounter debris approximation with the native fixed
  entrance and exit groups plus `DeoremRandomSpriteDebris`. The exact type,
  position, RNG, frame-counter and timer formulas now drive debris while
  Deorem waits, descends, rises, exposes its head and leaves underground.
- Restored the matching medium-dust particles, movement sounds and vertical
  screen shakes for both directions of the body construction and the retreat.
  The underground phase also refreshes the native Samus-collision guard until
  its four-second timer expires.
- `DeoremStartLeaving` now scans all live non-secondary actors for the five
  native energy and ammunition drop types. Its one-second retreat countdown
  remains paused until every drop is collected or despawns, then removes the
  eye on the exact zero edge and starts the leaving segments on the following
  frame, preserving the original current-sprite timing.
- The ROM-free regression covers deterministic random debris, all four fixed
  debris timer groups, the entrance dust, the active-drop gate, delayed eye
  removal and the following-frame retreat transition. A real frame-40
  `brinstar_012` capture retained the complete staged body and correct room
  composition with the new effects active.
- Remaining divergence: the two placements have not yet been compared frame
  by frame with an emulator.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed. Dashboard generation reported 126 features, 61
  functional or better and 114 with passing tests. Every reconstruction task
  completed or was up to date, generated-data checks were fresh and the
  proprietary-file guard passed.

## Patch 0362 - native Acid Worm body and acid cycle

- Replaced the interpolated head and body curve with
  `AcidWormHandleRotation`. The runtime now loads the native sine, swinging
  and horizontal-wiggle tables from the private ROM export, moves the shared
  pivot and calculates the head plus all nine actors with their original
  rotation offsets, phase starts, radii, hitboxes and draw order.
- Restored the health-dependent lunge speeds, eight-frame warning delay,
  Samus-contact abort, hooked hold timers and all three crash variants with
  their exact debris groups, particles, sounds and screen shakes. The distant
  attack now emits its three projectiles from the native animation frames;
  close, middle and far spit use their separate ROM velocity tables.
- Added the non-easy late-fight acid cycle. Alternate retractions below half
  health test Samus's native vertical and horizontal ranges, wait 40 frames,
  raise the surface five blocks with horizontal shakes, then lower and retain
  the new three-block base. The entrance now fades the room music and starts
  the Worms Battle track at the native trigger.
- The entrance destroys the central column in its original two-block stages
  at frames 0, 4 and 8. Body pieces use native room-slot identities 1 through
  9, expose the weak point only while rotated and transfer damage plus both
  low-health palette stages to the main actor.
- The ROM-free regression covers the staged column, native body identity,
  pivot rotation, weak-point transfer, acid raise/lower transition and the
  first far-spit velocity. A real frame-360 `kraid_005` capture triggered the
  fight from the zipline and rendered the complete curved body from the new
  ROM tables with all thirteen room actors active.
- Remaining divergence: the entrance's detailed block particles and upper
  debris, the body-by-body death and final liquid/music choreography, spit
  explosion BG priority and emulator comparison remain open.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed. Dashboard generation reported 126 features, 61
  functional or better and 114 with passing tests. Every reconstruction task
  completed or was up to date, generated-data checks were fresh and the
  proprietary-file guard passed.

## Patch 0363 - native Acid Worm destruction and death

- Restored the three native entrance-destruction effects. Each removed block
  pair now emits its original explosion, and the upper pair launches all eight
  debris pieces with their exact types and offsets.
- Replaced the compact boss removal with `AcidWormDeathGfxInit`,
  `AcidWormDeathFlashingAnim` and `AcidWormPartDeath`. The curved body keeps
  rotating through the 80-tick palette flash, then its nine actors explode in
  room-slot order at eight-tick intervals.
- The final tail explosion now owns the native encounter side effects: it
  removes the floor pair, records event 27, opens the door and starts the
  4.5-second music fade. The hidden main actor continues lowering the acid,
  emits dust above the pit, shakes both axes and plays the death and
  boss-killed cues at their native stages.
- The ROM-free regression now verifies all eight entrance debris types, the
  three block particles, lethal weak-point transfer, shared death flash, the
  56-frame weak-point-to-tail interval, floor removal, persistent event, door
  timer, acid descent and both music requests.
- Remaining divergence: spit explosions do not yet inherit BG1's priority,
  and the encounter has not been compared frame by frame with an emulator.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed. Dashboard generation reported 126 features, 61
  functional or better and 114 with passing tests. Every reconstruction task
  completed or was up to date, generated-data checks were fresh and the
  proprietary-file guard passed.

## Patch 0364 - native sprite background priority and Acid Worm impact

- Added an explicit native OBJ background-priority override to `MzmSprite`
  while retaining priority 2 for zero-initialized actors. `MzmSpriteWorld`
  now supplies BG1's live priority from the room transparency setup.
- Split the foreground renderer into priority 1 and priority 0 bands. A
  priority-1 actor is now painted after BG1 but before BG0, matching the GBA
  tie and allowing sprite AI to select either foreground band without
  disturbing the existing priority-2 and Kraid priority-3 paths.
- Ported `AcidWormSpitExplodingInit`: a spit that strikes a wall copies
  BG1's priority before starting its explosion, while an impact in the acid
  deliberately keeps the native default sprite priority.
- The regression verifies the normal moving priority, the wall transition,
  the copied BG1 priority and the explosion sound. A real frame-420
  `kraid_005` capture retained the layered acid room, Samus and all thirteen
  live encounter actors after the renderer split.
- Acid Worm is now functionally complete in the native parity checklist.
  Remaining validation: compare the full encounter frame by frame with an
  emulator.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed. Dashboard generation reported 126 features, 62
  functional or better and 114 with passing tests. Every reconstruction task
  completed or was up to date, generated-data checks were fresh and the
  proprietary-file guard passed.

## Patch 0418 - conservative Aria category suggestions

- Extended the generated Aria backlog with `suggested_category` and
  `suggestion_evidence` columns. Suggestions are restricted to unclassified
  routines whose known outgoing calls, address references and member callbacks
  all resolve to one category; propagation runs to a fixed point.
- This identifies 498 additional triage candidates without changing any
  canonical inventory category: 395 technical, 52 audio, 18 inventory, 15
  rooms, 15 saves, two menus and one input routine. Ambiguous and disconnected
  functions remain blank rather than receiving a guess.
- The generator test enforces that suggestions only annotate unclassified
  rows and are explicitly labelled `exclusive_callees`. No runtime or audio
  implementation changed.
- Validation on 2026-10-20: all 485 dashboard-recorded checks passed, the
  proprietary-file guard passed and every generated file was fresh.

## Patch 0417 - exhaustive Aria routine backlog

- Added `data/native_parity/aos_unassigned_routines.tsv`, a deterministic
  generated backlog of every cvaos C or assembly function not yet attached to
  a reviewed Aria parity feature. Each row carries its stable inventory id,
  symbol, category and classification evidence, kind, ROM address when known,
  source location and direct caller/callee counts.
- The initial exhaustive pass finds 3,573 native routines: 48 are attached to
  the existing 11 Aria features and 3,525 are not. Of the latter, 2,555 are
  assembly and 970 C functions; 2,902 have a source address. The largest
  queues are enemies (1,478), unclassified (1,375), environment (249),
  abilities (137) and audio (101). This is classification coverage, not a
  claim that 3,525 independent gameplay features must be ported.
- The native parity generator now writes and checks the backlog alongside the
  Markdown checklist. The incremental rebuild graph owns both outputs, and a
  changed cvaos source or annotation invalidates both. Tests prove that the
  report is exactly the set difference between discovered and attached
  routine ids.
- The development dashboard now exposes attached and unassigned routine
  counts for both games. The tooling registry and architecture documentation
  describe the new tracking boundary. No runtime, ROM-derived asset or audio
  implementation changed in this inventory-only cycle.
- Validation on 2026-10-20: all 485 recorded checks passed (448 Python and 37
  CTest), the proprietary-file guard passed, and the checklist, Aria backlog
  and dashboard generated files were fresh.

## Patch 0416 - final MZM partial-feature audit

- Promoted the last three partial Zero Mission entries to `functional` after
  checking their actual native boundary; audio was deliberately left untouched:
  - the private Samus library contains all 832 native body/cannon variants,
    1,072 sequences, 4,670 frames and all 67 real pose rows. Its 662 semantic
    bindings have no missing entry; the 18 former misses are suitless states
    that cannot occur because they require the Speed Booster, Screw Attack or
    shinespark;
  - `UpdateInput` now has one tested helper for the native held/previous/rising
    edge calculation, used by gameplay after SDL keyboard or gamepad mapping;
  - the PPU covers every mode used by the pinned game. A source audit confirms
    that MZM never selects the unimplemented bitmap modes 3 to 5, while the
    existing screen tests and captures cover its text, affine, object, window,
    blend and brightness paths and the private ROM bank.
- Added regression coverage for all native Samus pose-table rows, the input
  edge sequence and the display-mode boundary. Hardware-cycle details that do
  not affect an MZM-selected display path remain documented as divergences.
- Zero Mission now has 92 functional entries and no partial entries. The 11
  remaining partial entries in the joint inventory all belong to Aria.
- Validation on 2026-10-20: the warning-clean build, Python 448/448 and CTest
  37/37 passed; the proprietary-file guard passed, all generated files were
  fresh, and the dashboard reported 140 features, 105 functional or better
  and 128 with passing tests.

## Patch 0415 - clipdata audit

- Audited `clipdata.c` against the runtime:
  - every routine is ported, except `ClipdataSetupCode`, which only
    copies `ClipdataConvertToCollision` into IWRAM for speed;
  - the special blocks are covered by the breakable block and room gadget
    entries;
  - the hazards and ground effects have recorded captures: `norfair_022`
    (death in the lava) and `crateria_007` (water).
- `mzm.collision.clipdata` moves to `functional`; its vague "incomplete"
  note is replaced by the inventory. The parity inventory now contains 14
  partial and 89 functional entries.

## Patch 0414 - escape timer and gate graphics

- Checked the last escape ship difference against the ROM. The escape
  timer and the gates share OBJ VRAM 0x7800 natively:
  - `SpriteLoadAllData` copies `sEscapeTimerDigitsGfx` there, then the
    first gate copies `sEscapeGateAndTimerGfx` over it;
  - the eleven digit tiles the timer draws are byte-identical in the two
    graphics;
  - the timer uses OBJ palette row 3, not the gates' row 15.
  The runtime draws them apart, which gives the same picture.
- `mzm.enemies.escape_ship` moves to `functional`. The parity inventory
  now contains 15 partial and 88 functional entries.

## Patch 0413 - Time Attack

- New module `src/runtime/mzm_time_attack.c`, ported from `time_attack.c`:
  - the save file validity: the first four bosses beaten in order, each
    after its anti-cheat time;
  - the unlock value of `CheckUnlockTimeAttack`;
  - for a Time Attack run, `TimeAttackCheckSetNewRecord`: the Any% and
    100% records and their 20-character password (`TimeAttackGenerateSeed`,
    its eight shuffles and `TimeAttackRandomizeSeed`). The native 62-byte
    seed array is written past its end and read back, so it is a full
    88-bit seed here.
- The port was cross-checked against the decompilation's own
  `time_attack.c`, compiled with stub globals: identical results on
  200 000 random cases. Three of those vectors are now a unit test
  (`tests/test_mzm_time_attack.c`).
- The ending sets the Time Attack unlock and, for a Time Attack run,
  `ENDING_FLAG_FIRST_TIME_ATTACK_CLEAR` and
  `ENDING_FLAG_NEW_TIME_ATTACK_RECORD`.
- Save formats:
  - the save file moves to version 4 with `SaveFile.timeAttack`;
  - the unlock file moves to version 2 with the record. A version 1 file
    is still read, with the empty 0xFF record; this was checked with a file
    written by the previous code.
- The file select ports the Time Attack code (UP, LEFT, RIGHT, DOWN on a
  completed file once Time Attack is unlocked). It opens Continue / New /
  New Time Attack. Fixed while testing: the screen's init reset the unlock
  value.
- Status changes: `mzm.cutscenes.chozodia_escape` moves to `functional`.
  The Options screens, where the records are viewed, and the runtime's
  missing soft reset stay listed. The parity inventory now contains 16
  partial and 87 functional entries.

## Patch 0412 - Tourian escape room layers and exit fade

- Ported `LoadRoomCutscene`'s `RoomSetInitialTilemap(0 to 2)` as
  `mzm_room_gfx_initial_tilemaps`:
  - each room's own BG0 to BG2 block data is RLE-decompressed from the ROM
    (the BG form of `RoomRleDecompress`, without a size byte);
  - its block values become sheet indices (0x400 marks a common block);
  - the 21 x 16 blocks around the view are written.
- The Tourian escape's first stage now shows Tourian rooms 4 and 5 and
  Crateria room 1 with their block layers. Before, it showed garbage
  tiles.
- `BACKGROUND_EFFECT_EXIT_ZEBES_FADE` no longer whitens Samus. The native
  mask 0xFFFC spares her OBJ rows, so the overlay is now drawn just before
  her. The colour fade that follows still covers everything.
- Captures: `--tourian-escape` at frames 40, 130 and 200, before and after
  (ignored `captures/fp/te*.bmp`).
- Bank-backed test: Tourian room 4's empty RLE BG1 layer is filled by
  `mzm_room_gfx_initial_tilemaps`.
- `mzm.events.escape_sequence` moves to `functional`. Its remaining
  differences are the explosion actor's RNG slot and the overlay standing
  in for per-row brightness. The parity inventory now contains 17 partial
  and 86 functional entries.

## Patch 0411 - liquid splashes and dusts

- Ported `SpriteDebrisSetSplash`: a falling or bouncing debris piece that
  enters the room's liquid leaves a small splash at its own position. As
  natively, a type 5 to 8 piece that collides before 20 frames neither
  bounces nor splashes.
- Ported the remaining `SpriteUtilCheckInRoomEffect` /
  `CheckOutOfRoomEffect` splashes:
  - the Dragon rising and sinking (small);
  - the Geruta diving (big);
  - the Squeept leaving (small) and entering (big) its lava;
  - the Parasites falling (small);
  - the Acid Worm (huge).
- Fixed the Acid Worm: only its body plays `SOUND_ACID_WORM_SPLASHING`.
  Its parts no longer play it, as natively.
- Ported `ParticleSmallDust` and `ParticleSecondSmallDust`. The game never
  spawns them, but they are now exported with `sParticleSmallDustOam`.
- The second dusts now take the native low BG priority.
- Unit tests: the debris splash, the Dragon's splash and the dusts'
  priorities.
- `mzm.animation.particles` moves to `functional`. Its remaining
  differences are structural: the shot and charge effects do not use
  particle slots, and the splash is placed at the surface. The parity
  inventory now contains 18 partial and 85 functional entries.

## Patch 0410 - every sound of the bank checked

- New bank-backed test: each of the 708 `sSoundDataEntries` is played
  through the engine.
  - 612 are audible within four seconds.
  - The 95 silent entries are silent in the ROM itself:
    - the empty song shared by the unused entries;
    - songs whose tracks set `VOL 0`, such as
      `SOUND_ATOMIC_CHARGE_SILENT`;
    - `MUSIC_33`, `MUSIC_38` and `MUSIC_47`, at `VOL` 6 or 7 with velocity
      12, below the 8-bit mixer's resolution.
- The audio test now runs from the source directory.
- `mzm.audio.sound_engine` moves to `functional`, on this test and the
  user's review of the in-game playback. The parity inventory now
  contains 19 partial and 84 functional entries.

## Patch 0409 - native fades before the blue ship and the Chozodia escape

- `COLOR_FADING_SAMUS_IN_BLUE_SHIP` replaces the CANCEL stand-in. Its
  `ColorFadingProcess_BeforeBlueShip` uses the CANCEL colours (colour set
  0, `COLOR_FADING_FUNCTION_2`, to black) with one more stage, which stops
  the room's animated palette.
- `COLOR_FADING_CHOZODIA_ESCAPE` replaces the CANCEL stand-in. Its
  `ColorFadingProcess_ChozodiaEscape` has no colour fade: the screen is cut
  to black at once, then the ending save stages run.
- Unit tests: the blue ship fade is one frame longer than CANCEL and stops
  the animated palette; the Chozodia escape cut is black on its first frame
  and ends on its fourth.
- Checked in `chozodia_092` (event 74, energy 1299, `--spawn 362 420`,
  input `0:70,17:25,1:15`):
  - Samus boards the ship;
  - the fade leads to the blue ship cutscene;
  - the ship lifts off;
  - the screen's average brightness drops from 168 to 0 in one frame
    (1112), and the Chozodia escape starts two frames later.
- `mzm.cutscenes.samus_in_blue_ship` moves to `functional`. As natively
  (`playRoomMusic` FALSE), the room music keeps playing. The parity
  inventory now contains 20 partial and 83 functional entries.
- Captures: ignored `captures/fp/bs300.bmp` to `bs900.bmp` and
  `ce1104.bmp` to `ce1120.bmp`.

## Patch 0408 - ProcessComplexMenuOam

- Ported `ProcessComplexMenuOam` as `mzm_complex_menu_oam_process`. It
  works as `ProcessMenuOam`, except that an object with `rotationScaling`
  is placed by `ProcessComplexOam` (double-sized, matrix 0) with the
  current rotation and scaling.
- The game over screen, its only native user, now draws its head through
  it. The head is never rotated, so the output is unchanged.
- Bank-backed test: the unrotated head matches `ProcessMenuOam`, and a
  rotated one becomes a double-size affine object.
- `mzm.technical.non_gameplay_screens` moves to `functional`. Its helpers
  run on every captured non-gameplay screen. The parity inventory now
  contains 21 partial and 82 functional entries.

## Patch 0407 - boss timers and the hard stun threshold

- Ported `gInGameTimerAtBosses`:
  - Kraid, Mother Brain, the Ruins Test and Mecha Ridley record the
    in-game timer at their native points (slots 0 to 3);
  - the runtime copies the timer into `MzmSpriteGlobals.igt` every frame;
  - the save file moves to version 3 and stores the five timers. Versions
    1 and 2 are still read, with zero timers.
- `SpriteUtilIsSpriteStunned`'s threshold now follows the difficulty:
  `CONVERT_SECONDS(.24f)` on hard, so the stun ends sooner. This applies
  to the 21 AIs that check it.
- Corrected a stale divergence: the difficulty's contact damage was
  already applied, and no native drop depends on the difficulty.
- Unit tests:
  - the save round trip of the boss timers and the version check;
  - the Ruins Test despawn's timer;
  - a Zoomer stunned 3 frames on hard against 8 on normal.
- Status changes: `mzm.enemies.ruins_test` and `mzm.technical.save_file`
  move to `functional`. The save file's in-game check is the brinstar_033
  save and reload. The parity inventory now contains 22 partial and 81
  functional entries.

## Patch 0406 - dynamic Ruins Test Samus reflection

- Ported `ParticleSamusReflection`, the persistent mirror image that replaces
  the opening reflection two frames after Samus turns away from the
  background.
- The particle follows `gSubSpriteData1`, mirrors Samus's current body frame
  around the ghost's horizontal anchor, uses her live suit palette, remains
  alive outside the camera and keeps the native low-background priority and
  X-flip status.
- Its visibility now follows the shootable-symbol timer: the mirror is visible
  while no symbol is available, hidden while one can be shot and destroyed
  when the fourth symbol is hit.
- ROM-free coverage checks the actor-to-particle handoff, target following,
  native status bits, visibility changes and terminal removal. A real
  `chozodia_042` capture after turning and running right shows the mirrored
  running pose following the ghost at the arena centre.
- The Ruins Test remains partial only for its `gInGameTimerAtBosses` record and
  an emulator comparison.
- Validation on 2026-10-20: the warning-clean full build, CTest 36/36 and
  Python 446/446 passed. Dashboard generation reported 140 features, 92
  functional or better and 128 with passing tests. Generated-data checks were
  fresh and the proprietary-file guard passed.

## Patch 0405 - native palette background flashes

- Added a shared `mzm_background_effect` implementation of
  `BackgroundEffectStart`, `BackgroundEffectUpdate` and
  `BackgroundEffectProcess` for all three palette-based
  `MakeBackgroundFlash` variants.
- The processor snapshots BG palette rows 0 to 14, executes the native
  behavior tables one command per frame, interpolates each 5-bit RGB
  component toward white, blue or yellow, and restores the exact starting
  palette at stage zero. Animated BG row 15, OBJ palettes, sprites and the
  HUD remain untouched.
- Connected the quick flash to Ruins Test lightning and both points in
  Mother Brain's death sequence. Connected the slight-yellow flash to the
  normal and Long Beam Chozo hint sequence; the Long Beam hint's separate
  long transparency is still not ported.
- Background effects pause during power bombs and transitions, reject a new
  request while one is active, and reset on room load like the native global.
- Added `--background-flash quick|slight|heavy` capture staging. A
  `chozodia_042` sequence shows the quick table at white 16/16, blue 16/16,
  white 14/16, white 6/16 and the original palette restored.
- ROM-free tests cover the three tables, timing, exclusion of palette row 15,
  palette restoration, request arbitration and each connected sprite AI.
- Validation on 2026-10-19: the warning-clean full build, CTest 36/36 and
  Python 446/446 passed. Dashboard generation reported 140 features, 92
  functional or better and 128 with passing tests. Generated-data checks
  were fresh and the proprietary-file guard passed.

## Patch 0404 - Ruins Test ghost alpha blending

- Corrected the native sprite status split: high priority is bit 4, while
  bit 13 marks a semi-transparent OBJ. This removes the earlier accidental
  alias between two independent hardware flags.
- Ported the ghost's exact `BLDALPHA` sequence: its spawn progresses from
  16/0 through 16/8 to 9/8 every four frames, its return progresses from
  10/16 to 16/16 every ten frames, and its despawn reaches 0/16. Damage
  flashes temporarily disable semi-transparency like the native routine.
- The SDL renderer now reproduces the independent coefficients with the
  custom equation `OBJ * EVA / 16 + target * EVB / 16`, rather than treating
  the native alpha flag as ordinary source-alpha blending.
- Added `--ruins-test-ghost` capture staging so the visual routine can be
  checked without scripting or bypassing the whole encounter. Captures in
  `chozodia_042` at EVA/EVB 16/0, 16/4, 14/8 and 9/8 show the expected
  progressive blend over the arena.
- Unit coverage verifies every fade phase, the native frame intervals, the
  damage-flash alpha toggle and the transition into despawn.
- Still partial: `MakeBackgroundFlash`, the `PE_SAMUS_REFLECTION` particle,
  the in-game boss timer record and emulator comparison remain.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 446/446 passed. Dashboard generation reported 140 features, 92
  functional or better and 128 with passing tests. Generated-data checks
  were fresh and the proprietary-file guard passed.

## Patch 0403 - Ruins Test native reflection graphics

- Ported every graphics DMA used by the Ruins Test reflections:
  - the opening reflection switches to the native turning-right or
    turning-left tiles when Samus turns away from the background;
  - the closing reflection starts with the suitless tiles, is hidden while
    the native full-suit tiles are copied, then reappears as the fully
    powered suit.
- Ported `sRuinsTestPal_SamusReflectionFullSuit`: OBJ row 15 starts at row
  0, then advances through rows 1 to 9 every 12 frames.
- `scripts/mzm_sprites.py` now bakes the ROM-derived copies at OBJ VRAM
  `0x4280` and `0x4680` as `/tr`, `/tl`, `/u` and `/f` variants. The runtime
  selects them with `MzmSpriteGlobals.ruins_test_reflection_gfx` and the
  existing reflection palette state.
- Unit tests cover both VRAM ranges, every source symbol and the runtime
  transitions through the hidden copy and full-suit palette.
- Checked in `chozodia_042` with `--ruins-test-end`: captures at frames 140,
  310, 350 and 430 show the suitless silhouette, the deliberately hidden
  copy, the pale full suit and its final colours.
- Still partial: the ghost's alpha blending, `MakeBackgroundFlash`, the
  reflection particle and the in-game timer record remain unported.

## Patch 0402 - Ruins Test reflection palette

- Ported the reflections' OBJ row 15 copies of
  `sRuinsTestPal_SamusReflection`:
  - the starting reflection fades through its 15 rows, one a frame, then
    returns to `sRuinsTestPal`'s row 7;
  - the closing reflection pulses through rows 0 to 10 every 8 frames.
  The state is held in `MzmSpriteGlobals.ruins_test_reflection_palette`,
  and the exporter bakes `/r1` to `/r15` for
  `sRuinsTestSamusReflectionOam`.
- Not ported:
  - the full suit reflection's palette;
  - the reflections' graphics DMA.
- Unit test: the reflection's first two rows. No capture recorded yet.

## Patch 0401 - Ruins Test symbol palette

- Ported the Ruins Test's dynamic palette (`RUINS_TEST_TRANSFER_DYNAMIC_PAL`:
  the first eight colours of OBJ row 14) in `src/runtime/mzm_ruins_test.c`:
  - the four symbols lighting up, merging and disappearing;
  - the shot symbol's flight, a 4-frame pulse through the base and the six
    shot palettes;
  - the placed symbol's flash.
  Each copy sets `MzmSpriteGlobals.ruins_test_palette`, which is reset
  with the spriteset.
- The exporter bakes `/s1` to `/s6` variants, only for the tables whose
  pixels read those colours: the four symbols, merging, disappearing, the
  shootable symbol's turning, the placed symbol and the shot symbol. The
  renderer picks the variant the AI last selected.
- Corrected a stale note: the symbols have been drawn since patch 0396's
  graphics owners; only their palette changes were missing.
- Unit tests: a symbol lighting frame selects its palette, and the
  flying symbol cycles through all seven palettes.
- Checked in `chozodia_042`: the four symbols turn from purple to light
  blue as they light up, then merge.
- Captures: ignored `captures/fp/rs30.bmp` to `rs140.bmp`.

## Patch 0400 - in-game check for the paralyzer pistol

- Checked in `chozodia_000`, reached through the Tourian escape and the
  suitless status screen:
  - a tap of B fires the uncharged pistol shot;
  - 130 frames later, a second tap fires the charged shot. The pistol
    charges by itself and B does not need to be held, as natively;
  - the charged shot leaves its trail particles as it flies left, and the
    counter restarts from 0.
- Corrected a false divergence: the charged trail never had a native low
  BG priority. `ParticleChargedPistolTrail` never sets
  `PARTICLE_STATUS_LOW_BG_PRIORITY`, so the runtime's priority 0 matches.
- The capture diagnostics now print the charge counter and the active
  projectiles.
- `mzm.weapons.paralyzer_pistol` moves to `functional`. The parity
  inventory now contains 24 partial and 79 functional entries.
- Captures: ignored `captures/fp/x4322.bmp`, `x4334.bmp` and `x4346.bmp`.

## Patch 0399 - item acquisition and suitless status screens

- Ported in `src/runtime/mzm_pause_screen.c`:
  - `StatusScreenSuitlessItems`;
  - the whole of `StatusScreenToggleItem`;
  - the new-item form of `StatusScreenGetSlotForNewItem`;
  - the suitless case of `UpdateSuitType`.
- The pause screen opens on its own for two flags:
  - `PAUSE_SCREEN_ITEM_ACQUISITION`: the new item is deactivated, then
    toggled back on with its sound (a suit fades the wireframe; a missile or
    bomb row lights up), then described until a key is pressed;
  - `PAUSE_SCREEN_SUITLESS_ITEMS`: the same, after Samus is turned
    suitless with the pistol's items.
- A native quirk is kept: the toggle result (`u32` 0xFF) is stored in an
  `s32` and tested below 0, which never holds, so the toggle stage always
  runs.
- The message banner emits `MZM_BANNER_ITEM_ACQUISITION` when a new item's
  or first tank's banner is removed. The runtime then:
  - fades the game out and opens the screen;
  - retrieves player 2 at the end of the fade back in. Before this patch,
    track 2 was never retrieved after a new item.
- `RoomLoad`'s Varia Suit return is ported: Samus faces the foreground and
  `IGC_GETTING_VARIA` plays through the suit upgrade module.
- The post-Tourian monologue is now followed by the suitless status screen
  before Chozodia room 0.
- Unit tests on the ROM bank: the Varia Suit, a first missile tank and the
  suitless sequence.
- Checked in game:
  - in `brinstar_027`: the Varia Suit taken from its statue; the banner,
    then the status screen ("Decreases damage taken from enemies."), then
    the suit flash and Samus in the Varia Suit;
  - after the Tourian escape: the suitless status screen describing the
    pistol, then `chozodia_000` with the Power Grip and the Long and Charge
    Beams active.
- `mzm.interface.message_banner` moves to `functional`. The status screen,
  escape sequence, Chozo statue and fully powered entries are updated. The
  parity inventory now contains 25 partial and 78 functional entries.
- Captures: ignored `captures/fp/w200.bmp`, `w520.bmp`, `w780.bmp`,
  `w900.bmp`, `t3800.bmp` and `t3950.bmp`.

## Patch 0398 - fully powered suit items on the status screen

- Ported `StatusScreenFullyPoweredItems` and the routines it needs in
  `src/runtime/mzm_pause_screen.c`:
  - `PauseScreenSuitChangingStart`;
  - `StatusScreenFindUnknownItemSlot`;
  - `StatusScreenUpdateUnknownItemPalette`;
  - `StatusScreenEnableUnknownItem`;
  - the suit-change form of `StatusScreenGetSlotForNewItem`;
  - `PauseScreenFadeWireframeSamus`;
  - the fully powered case of `UpdateSuitType`.
- The pause screen now opens directly on the status screen for
  `PAUSE_SCREEN_FULLY_POWERED_SUIT_ITEMS`, with the native blending and the
  wireframe option 0 (suitless before the fade).
- During the sequence, Plasma Beam, Gravity Suit and Space Jump stay
  unknown rows and the missile rows stay inactive, as natively.
- The sequence:
  - the normal items are enabled one by one;
  - each unknown item flashes, is enabled and is described until A;
  - the Gravity Suit fades the wireframe to its palette;
  - every item is activated at the end.
- The getting-fully-powered chain now runs this screen between the
  cutscene and the room return. It replaces the previous stand-in, which
  is kept only without the ROM bank.
- The runtime tracks `gCurrentItemBeingAcquired` from the item and tank
  banners and clears it when a file is loaded.
- Unit test: the whole sequence on the ROM bank, with three unknown items.
  It checks the equipment, the sounds and the palette flash.
- Checked in `chozodia_042` (`--ruins-test-end` with test items):
  - the Plasma Beam and the Space Jump shown unknown;
  - the Plasma Beam revealed with its description;
  - the Space Jump revealed with "Somersault continually";
  - back in the room with every item active.
- Registry changes: `mzm.menus.status_screen`,
  `mzm.cutscenes.getting_fully_powered` and `mzm.items.chozo_statues` are
  updated.
- Captures: ignored `captures/fp/r975.bmp`, `r1160.bmp`, `r1330.bmp` and
  `q1500.bmp`.

## Patch 0397 - getting the fully powered suit

- Ported the "getting the fully powered suit" cutscene
  (`GettingFullyPoweredSuit*`) in `src/runtime/mzm_cutscene.c`:
  - Samus and the backdrop on BG2 and BG3;
  - the rising ring with its palette cycled through OBJ row 10;
  - the sparkles around the ring and the ones rising from below;
  - window 1 following the ring, with Samus brightened below it;
  - the scroll down to Samus, then the fade to white.
- Supporting changes in the cutscene module:
  - the sprite half of `CutsceneUpdateSpecialEffect` (BLDY steps);
  - `CutsceneStartSpriteEffect` and `CutsceneFadeScreenToWhite`;
  - 14 cutscene objects, with the per-object counters
    (`MzmCutsceneOamData`) kept beside the menu objects.
- New module `src/runtime/mzm_in_game_cutscene.c` for the suit upgrade
  (`InGameCutsceneUpgradingSuit` and its start, process, init and
  flash-offset helpers):
  - BG0 faded out and hidden;
  - window 1 opened on Samus and grown to the whole screen, with BG0's
    tile 0xC0 blended over it;
  - the Gravity Suit activated at stage 11;
  - the window shrunk back and control given back.
- The runtime chain from the Ruins Test, as request 6:
  - its despawn requests the cutscene;
  - the room fades out (the CANCEL fade stands in for the white fade);
  - at the fade's end: the fight stage becomes STARTING_CUTSCENE, the Varia
    events are set and Varia is marked obtained;
  - the cutscene plays;
  - stand-in for `StatusScreenFullyPoweredItems`, which is not ported: the
    suit is refilled and every obtained item is activated at once, except
    the Gravity Suit;
  - Samus is placed facing the foreground and the suit upgrade is queued.
- The fully powered suit banner's removal now emits
  `MZM_BANNER_START_SUIT_ANIM`. The runtime then:
  - sets the suit animation stage;
  - spawns the falling Chozo pillar through the new
    `mzm_sprites_spawn_primary`.
- New capture staging: `--ruins-test-end` sends the initialized Ruins Test
  straight to its despawn, so the fight is skipped.
- The capture diagnostics now print:
  - the fight stage;
  - the suit and items;
  - the upgrade stage and its window.
- Checked in `chozodia_042`:
  - the cutscene at frame 700;
  - the return with the fully powered suit and the banner;
  - after B, the window growing from x 120-122 and the purple flash at
    stage 13. Tileset 56's tile is solid colour 0x7C19, so purple is the
    native colour;
  - the Gravity Suit active (items 0x60);
  - the fight stage reaching 8;
  - with Samus walking right, the pillar sinking 64 pixels into its shaft.
- Unit tests:
  - the whole cutscene on the ROM bank (`test_mzm_screens`);
  - the suit upgrade's stages, window and offsets.
- Registry changes:
  - new `mzm.cutscenes.getting_fully_powered` (functional);
  - `mzm.enemies.chozodia_scripted` moves to `functional`: the pillar now
    falls in game;
  - the Ruins Test, banner and Chozo statue notes are updated.
  The parity inventory now contains 26 partial and 77 functional entries.
- Captures: ignored `captures/fp/f700.bmp`, `f1000.bmp`, `g1360.bmp`,
  `g1420.bmp`, `g1600.bmp`, `h1700.bmp` and `h2000.bmp`.

## Patch 0396 - graphics owners of secondary sprites

- Fixed a rendering bug: `mzm_sprite_graphics_owner` had no mapping for the
  secondary sprites of five families, so they fell back to the Skree
  graphics and were not drawn at all:
  - the hive roots;
  - the vine Geruta's parts;
  - the elevator statue debris;
  - Imago's parts, needles, damaged stinger and eggs;
  - the Ruins Test's symbols and ghost.
  They now use their family's graphics.
- A regression now checks every secondary sprite of a ported family: none
  may fall back to Skree except the Skree explosions themselves.
- Correction to earlier notes: the Imago capture of patch 0377 described
  eggs that were in fact background decoration, and the elevator statue
  capture of patch 0391 showed no debris. The notes are corrected.
- New captures:
  - in `ridley_019`, the glowing eggs; once Samus walks into the last one,
    Imago descends with its wings and stinger and strikes her (energy 99
    to 89);
  - in `brinstar_008`, the Kraid statue's head debris falling and turning
    to dust.
- `mzm.enemies.imago` moves to `functional`; the parity inventory now
  contains 27 partial and 75 functional entries.
- Captures: ignored `captures/enemies/imago-fight.png`,
  `imago-eggs-before-after.png` and `elevator-statue-debris.png`.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 444/444 passed; generated parity and dashboard data are fresh.

## Patch 0395 - in-game check for enemy drops

- With the runtime's input masks, a capture in `brinstar_000` follows a full
  drop in game:
  - crouched shots kill a yellow Zoomer;
  - it leaves a large energy drop;
  - Samus walks over it and collects it (energy 50 to 70).
- Mecha Ridley is ported, so its grouped energy drops now have a natural
  source; their death sequence is covered by the sprite regression.
- `mzm.items.enemy_drops` moves to `functional`; the parity inventory now
  contains 28 partial and 74 functional entries.
- Capture: ignored `captures/enemies/drop-collected.png`.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 444/444 passed; generated parity and dashboard data are fresh.

## Patch 0394 - Chozodia alarm presentation

- `DecrementChozodiaAlarm` and `DisableChozodiaAlarm` now follow the native
  code:
  - while the alarm rings, it enables the room's animated alarm palette
    and, before the Fully Powered Suit, plays the alarm music at priority
    0x40 every frame;
  - when it stops, the palette is disabled after its cycle (-1) and the
    music returns through `UpdateMusicAfterAlarmDisable`;
  - Mecha Ridley's death keeps it ringing.
- The sprites now relay their writes of `gDisableAnimatedPalette` (value
  plus a written flag) to the room's animated palette: the alarm, Mecha
  Ridley's death, battle start and escape, and the zipline generator.
  This replaces the generator-only flag.
- Ported the stealth and alarm music branches: `unk_378c` in
  `CheckSetNewMusicTrack`, `unk_38d8` in `CheckPlayTransitionMusicTrack`,
  and `UpdateMusicAfterAlarmDisable` as a new sound request.
- A new game (not a loaded file) clears the alarm before the room's sprites
  load, as `SpriteLoadAllData` does.
- The pirate lasers get draw order 3. Their burst takes BG1's priority, and
  the invisible hatch shot no longer plays the firing sound.
- Capture output now prints the room's animated palette, its row and state,
  and the alarm timer.
- Tests:
  - the alarm countdown (palette, music, disable, Fully Powered Suit);
  - the laser's draw order and burst priority;
  - in the audio test, the alarm priority holding the track through a room
    change.
- A capture in `chozodia_013` (event 67) shows the black pirates firing with
  the alarm ringing and the room's palette 10 cycling (rows 2, 3, -7, 5).
- `mzm.enemies.space_pirate` moves to `functional`; the parity inventory now
  contains 29 partial and 73 functional entries.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 444/444 passed; generated parity and dashboard data are fresh.

## Patch 0393 - in-game check for the worker robot

- The earlier attempts had used the GBA key bits in `--input-script`. The
  runtime's script masks are its own key set: right 1, left 2, up 4,
  down 8, A 16, B 32. With B as 32, a capture in `chozodia_051` shows a
  worker robot asleep, woken by two beam shots (`WAKING_UP`), then walking
  away along the corridor.
- `mzm.enemies.worker_robot` moves from `partial` to `functional`; the
  parity inventory now contains 30 partial and 72 functional entries.
- Capture: ignored `captures/enemies/worker-robot-wake.png`.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 444/444 passed; generated parity and dashboard data are fresh.

## Patch 0392 - in-game checks for Gadora and Deorem

- A capture in `kraid_029` shows Gadora's eye closed on its hatch, then
  opening once Samus comes within seven blocks in front of it.
- A capture in `brinstar_012` shows Deorem waking:
  - its trigger needs missiles and Samus standing under its centre;
  - it spawns, its head emerges between its spiked segments, then it
    retracts.
- Both move from `partial` to `functional`; the parity inventory now
  contains 31 partial and 71 functional entries.
- The worker robot stays `partial`. Its sleeping is confirmed in
  `chozodia_014`, but the spawn points left Samus crouched in a low passage
  and no captured shot reached it. Its immune-projectile wake path matches
  `ProjectileHitSpriteImmuneToProjectiles` and is unit tested.
- Captures: ignored `captures/enemies/gadora-eye.png` and
  `deorem-gadora.png`.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 444/444 passed; generated parity and dashboard data are fresh.

## Patch 0391 - in-game checks for four bosses and gadgets

- Baristute (`kraid_007`), the hives with their Mellows (`brinstar_015`)
  and the Tangle Vines (`norfair_024`) already had ported AIs, unit tests
  and recorded in-game captures. Their remaining notes only say that they
  were not compared with an emulator, so they move to `functional`.
- A new capture in `brinstar_008`, with Kraid's event set, shows the Kraid
  elevator statue crumbling (poses 0x9, 0xB then 0xF) with its debris and
  settling into rubble. The elevator statues move to `functional`.
- Gadora (`kraid_029`) and Deorem (`brinstar_012`) stay `partial`: their
  captures only showed them waiting for Samus to come close.
- The parity inventory now contains 33 partial and 69 functional entries.
- Capture: ignored `captures/enemies/elevator-statue-crumbling.png`.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 444/444 passed; generated parity and dashboard data are fresh.

## Patch 0390 - in-game checks for eight common enemies

- Recorded in-game captures for enemies that were already ported and unit
  tested but had no in-game check. The runtime output tracks each sprite's
  pose and position between two frames, and the images confirm the
  behaviour:
  - the Zoomers (`brinstar_000`, `brinstar_001`) crawl along floors, walls
    and ceilings;
  - the Ripper crosses `brinstar_001`;
  - the Wavers fly their wave paths (`brinstar_017`);
  - the Violas crawl around their rock (`ridley_002`);
  - the Sidehoppers hop on floor and ceiling (`kraid_001`);
  - a Holtz dives from the ceiling and climbs back (`ridley_003`, y 42 to
    130 and back);
  - the Mellas swarm from their ceiling (`norfair_001`);
  - the Multiviola bounces (`norfair_020`).
- These eight entries move from `partial` to `functional`; the parity
  inventory now contains 37 partial and 65 functional entries. Their
  divergences are unchanged (Zoomer's on-screen check keeps the GBA screen
  size; the unused Multiviola secondary sprite).
- The worker robot stays `partial`: it sleeps in `chozodia_014` as it should,
  but the capture did not land a shot to wake it.
- Captures: ignored `captures/enemies/zoomer-ripper.png` and
  `simple-enemies.png`.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 444/444 passed; generated parity and dashboard data are fresh.

## Patch 0389 - Mecha Ridley palettes and 100% rule

- Mecha Ridley is now exported as 8-bit OBJ palette indices. The exported
  frames cover the base look, the damage tints, the frozen and stun rows,
  the destroyed graphics alone and tinted (`/d`, `/d/rowN`) and the seven
  weapon stages, together with their OAM bounds. The room's OBJ palette is
  written as an `objpal` line, with the six palette DMA tables as AI
  tables.
- The renderer rebuilds Mecha Ridley's live palette each frame from the
  DMAs the AI records, and caches the colorized textures by palette
  version. Those DMAs are:
  - the entrance fade through the seven `sMechaRidleyFadingPal` rows, with
    `SOUND_2AD`;
  - the head's `MechaRidleyPartGreenGlow` cycle, in its current palette
    row;
  - the death's destroyed graphics and colors;
  - the eye dimming during the drops, then its green and red blinking.
- `TransparencySpriteUpdateBldalpha`'s entrance blend dims BG2 and BG3 from
  10/16 back to full brightness as the battle starts.
- `SpriteUtilGetFinalCompletionPercentage` is ported: at 100% the core and
  cover get triple health, and the contact damage is doubled.
- The missiles and fireballs start at Q8.8 scale 1. They are single-OBJ
  frames with exported bounds, so they rotate through the native affine
  window.
- The sprite-table loader no longer limits itself to 64 AI tables: it now
  tracks them with a flag array instead of a 64-bit mask.
- A ROM-free regression drives each palette DMA from its native pose and
  timing, plus the 100% health and contact rule. Python tests cover the
  index shifts and the destroyed-graphics copies. A capture in
  `chozodia_072` shows the dark, faded body crawling in and its full colors
  once the battle starts (ignored `captures/chozodia/mecha-ridley-fade.png`).
- `mzm.enemies.mecha_ridley` moves from `partial` to `functional`; the
  parity inventory now contains 45 partial and 57 functional entries.
  Remaining divergences:
  - the seven one-frame destroyed-graphics copies show their final result;
  - the Chozodia alarm toggle waits for the room animated palettes.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 444/444 passed; generated parity and dashboard data are fresh.

## Patch 0388 - native per-OBJ rotation for Skree explosions

- Extended the ROM-local Zero Mission sprite extraction with separate OBJ
  art for both Skree explosion tables. Only the affine-whole tables opt in,
  so normal sprite sequences keep the existing compact representation; all
  palette variants still resolve through the same content-addressed object
  store.
- Replaced the composed-frame approximation for those tables with
  `SpriteDraw`'s per-object transform: centers are scaled and rotated through
  the extracted 320-value sine table, negative Q8.8 values use the GBA's
  arithmetic-shift rounding, OAM priority is preserved and every part is
  clipped independently to its double-size affine window. The affine use of
  the original flip bits is also respected, including the ignored Y flip.
- Restored the explosion sprites' native Q8.8 scaling value. A capture in
  `brinstar_004` follows a green Skree through its dive and crash, observes
  all four rotating secondary sprites and their animation-table removal.
  `mzm.enemies.skree` therefore moves from `partial` to `functional`; the
  parity inventory now contains 46 partial and 56 functional entries.
- Added regressions for affine-part extraction, ignored Y flip, signed Q8.8
  center rounding, quarter-turn placement and the native Skree scaling
  initialization. No ROM or extracted asset is tracked.
- Validation on 2026-10-19: the warning-clean build, CTest 35/35 and Python
  442/442 passed. The dashboard reports 139 features, 69 functional or
  better and 127 with passing tests; generated-data checks and the
  proprietary-file guard pass.

## Patch 0387 - native affine clipping for Zeela eyes

- Added a ROM-local `oam_bounds.tsv` sidecar to the Zero Mission enemy
  sprite extraction. It records the full native OBJ rectangles after global
  flips, including transparent edge pixels that the tightly cropped BMPs
  intentionally omit; the shared bitmap index remains backward compatible.
- The runtime now loads and validates those bounds and uses the native OBJ
  center for single-part affine sprites. Rotation and Q8.8 scaling happen
  around that center, and SDL output is clipped to the original hardware
  window (or its double-size window) while preserving any existing renderer
  clip.
- This completes the thrown red-Zeela eyes: their 8x8 OAM object no longer
  grows outside the hardware box while spinning. A capture in `kraid_017`
  destroys a red Zeela with Screw Attack and shows both secondary eyes in
  their moving state, so `mzm.enemies.zeela` moves from `partial` to
  `functional`. The parity inventory now contains 47 partial and 55
  functional entries.
- Added a ROM-free regression for full OAM bounds, transparent edges and X/Y
  global flips. The ignored private sprite library was rebuilt successfully;
  no extracted data is tracked.
- Validation on 2026-10-19: the warning-clean build, CTest 35/35 and Python
  441/441 passed. The dashboard reports 139 features and 68 functional or
  better, with 127 passing tests; generated-data checks and the
  proprietary-file guard pass.

## Patch 0386 - supported unlock scope and native liquid splashes

- Removed the bundled NES game from the runtime scope. A first clear now
  shows only the Gallery unlock panel; the historical fallback flag can no
  longer start an unsupported second panel. The supported Hard and future
  Time Attack messages are unchanged.
- Renamed the global unlock byte to `difficulty_clears` while preserving its
  serialized offset and version, so existing `options.sav` files remain
  readable without presenting the excluded game as a runtime feature.
- Added all nine native small, big and huge water, lava and acid splash OAM
  tables to `scripts/mzm_sprites.py`. The ignored local sprite library was
  rebuilt successfully with 20,403 sequences, 119,663 frames and 29,128
  unique BMPs; no extracted data is tracked.
- Ported `SpriteUtilSetSplashEffect` selection and connected it to Reo room
  effect crossings and the falling paths of Sova and Zeela. The particles
  spawn on the live liquid surface, use the correct hazard family and carry
  the native low-BG-priority flag.
- Added regressions for the splash table, priority, Sova's big water splash,
  Reo's small strong-acid splash and the single first-clear panel. Reo and
  Sova move from `partial` to `functional`; Zeela remains `partial` only
  because its thrown eyes still lack native affine clipping. The parity
  inventory now contains 48 partial and 54 functional entries.
- Validation on 2026-10-19: the warning-clean build, CTest 35/35 and Python
  440/440 passed. The dashboard reports 139 features, 67 functional or
  better and 127 with passing tests; generated-data checks are fresh and the
  proprietary-file guard passed.

## Patch 0385 - ending unlock pop-ups and persistence

- Completed `GM_CREDITS` with `UnlockedOptionsInit`,
  `UnlockedOptionsPopUp` and `UnlockedOptionsVBlank`. The native blue panel
  expands through window 0, reveals its English text eight characters per
  frame, waits for A at frame 128 and contracts before leaving the ending.
- At the time of this patch, a first clear showed the native two-step unlock
  sequence. Patch 0386 deliberately removed its bundled NES-game half from
  project scope, leaving only `Gallery`. A later first Hard clear selects
  `Sound Test`, and the Time Attack Records message is ready for the future
  Time Attack completion flag. The ending result image also renders the
  animated `NEW RECORD` OAM badge when that native flag is supplied.
- Added a separate versioned, checksummed `options.sav` for the global
  `FileScreenOptionsUnlocked` state. Ending the game records the gallery
  image and cleared difficulty there, while the current game slot receives
  its completed-difficulty bit before the Chozodia escape starts.
- Preserved completion flags through later save-station writes. The previous
  runtime save builder cleared this already-supported field whenever it
  rebuilt a loaded save.
- Extended the bank-backed screen regression through both first-clear
  panels, their exact window geometry, rendered pixels, A gate and the
  Sound Test and Time Attack Records priorities. The save regression checks
  the global unlock round trip and rejects a corrupted checksum.
- A real runtime capture shows the correctly colored `Gallery has been
  added to the OPTIONS screen` panel. Time Attack validity and record
  generation and the actual Options entries remain future work.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35
  and Python 440/440 passed. The dashboard reports 139 features, 65
  functional or better and 127 with passing tests; generated-data checks
  are fresh and the proprietary-file guard passed.

## Patch 0384 - ending result image

- Extended `GM_CREDITS` past Samus's final pose into the native ending image.
  The runtime selects one of the eight illustrations from the ending number,
  loads both background halves and their palette, and slides them into place.
- Ported `EndingImageLoadIGTAndPercentageGraphics`: the saved in-game hours,
  minutes and seconds and the item count encoded by
  `ChozodiaEscapeGetItemCountAndEndingNumber` now populate the native upper
  and lower number tiles in OBJ VRAM.
- Ported the English text choreography of `EndingImageDisplay`: `CLEAR TIME`,
  the timer, `YOUR RATE FOR COLLECTING ITEMS IS`, the percentage and
  `NEXT MISSION` appear through the native per-letter delay and palette
  sequence, then remain as their full-line OAM frames.
- The screen waits at frame 1,376 for A, B or START, starts the native
  256-frame music fade and ends at frame 1,664. The runtime then returns to
  the intro because the unlocked-options pop-up remains the final missing
  post-credits screen.
- The bank-backed regression checks image 4, a `03:04:05` clear time, a 100%
  collection rate, animated and permanent text, the input gate, music fade
  and exact ending frame.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed. The dashboard reports 139 features, 64 functional or
  better and 127 with passing tests; generated-data checks are fresh and the
  proprietary-file guard passed.

## Patch 0383 - unused native monster prototypes

- Ported the complete native scope of the unused Crocomire prototype: all 25
  OAM tables, six composition tables, the primary head and five secondary
  body actors, native hitboxes, draw order, immunity and dying-state
  propagation. The ROM contains no attack or movement state for this actor,
  so the runtime deliberately does not invent a Crocomire encounter.
- Ported Gekitai Machine's complete native one-line AI, which immediately
  removes the unused actor.
- Added a direct Red Garuta regression covering its native health, pose,
  animation, collision and hitbox. Red Garuta was already routed through the
  Tangle Vine family; it is now explicitly represented in the final monster
  coverage proof.
- Corrected the native `GARUTA` identifier and the dashboard classification;
  the bestiary now lists Red Garuta, Crocomire and Gekitai Machine under their
  implemented families instead of leaving either prototype or Red Garuta out.
- Every named retail primary monster id now has a runtime route. The two
  remaining Zero Mission deliverables are both post-credits screens: the
  ending image with clear time and item rate, then the unlocked-options
  pop-up and its save flow.
- The private sprite rebuild now contains 20,394 sequences, 119,591 frames
  and 29,062 unique BMPs.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed. The dashboard reports 139 features, 64 functional or
  better and 127 with passing tests; generated-data checks are fresh and the
  proprietary-file guard passed.

## Patch 0382 - Credits and end screen

- Ported the first half of `ending_and_gallery.c`'s GM_CREDITS in
  `mzm_credits.c`, on the non-gameplay screen from the ROM bank, with the
  credits music:
  - the US credit lines scroll over the dark Chozo wall. Each line is
    built from the credits characters in white, blue or red, with
    two-row names and the copyright lines. The V-blank writes it into BG0,
    or into BG1 half a tile aside when its width is odd;
  - after the last line, the wall fades in and the text fades out;
  - the wall scrolls down to its bottom (513 frames), then zooms into the
    Chozo drawing and fades to black (961 frames);
  - the end screen slides and blends Samus's poses over the drifting
    space background. The last pose depends on the ending: full suit,
    helmet off, or suitless after the white palettes. It then fades out.
- `ChozodiaEscapeGetItemCountAndEndingNumber` is ported: the ending number
  comes from the tank maximums of the difficulty, the abilities, the
  difficulty and the in-game hours.
- In game, the Chozodia escape now continues into the credits.
- Not ported: the ending image with the clear time and rate, and the
  unlocked options pop-up. The runtime returns to the intro after the end
  screen.
- A bank-backed test checks:
  - the ending number in five cases;
  - where the first credit line is placed;
  - the lengths of the credits stages;
  - the end screen through to its end.
- Captures of the credit lines, the wall, the zoom and the poses are in the
  ignored `captures/chozodia/credits-lines.png` and `credits-end.png`.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0381 - Chozodia escape sequence

- Ported `chozodia_escape.c` (GM_CHOZODIA_ESCAPE) in
  `mzm_chozodia_escape.c`, on the non-gameplay screen from the ROM bank.
  After a fade in, the five stages play with their native lengths:
  - the mother ship's door opens and the blue ship grows out of it as a
    double-sized affine object, dives, then climbs away;
  - the mother ship reddens and shakes ever faster, then a white explosion
    grows over it as an ellipse;
  - the crater, the decaying mother ship and its explosions, with the flash
    fading by alpha and then to black;
  - the ship over Zebes, far away and then past the camera, with the escape
    music;
  - Samus in her ship turning grey, the English "MISSION ACCOMPLISHED!"
    letters appearing one by one, and the fade out.
- Supporting changes:
  - the PPU takes a per-line WIN0H table, standing in for the native
    H-blank handler that draws the explosion ellipse;
  - `ProcessComplexOam` and `CalculateOamPart4` moved from the Tourian
    escape into `mzm_screen.c` so both escapes share them.
- In game:
  - when the escape ship has flown off, its request freezes the room, fades
    it out and starts the sequence; `--chozodia-escape` starts it directly;
  - at the end the runtime returns to the intro, because the credits are
    not ported.
- Not ported:
  - the ending flags, the gallery, difficulty and time attack unlocks and
    their save;
  - the soft-reset lock;
  - the non-English letters;
  - the credits.
- Approximation: the H-blank's one-line delay is mirrored, but line 0 takes
  its own value.
- A bank-backed test checks the fades, the stage lengths, the sounds, the
  explosion window, the grey palette and the letters.
- Captures of every stage, and of the sequence reached from the escape ship
  in `chozodia_092`, are in the ignored `captures/chozodia/chozodia-escape.png`
  and `chozodia-escape-from-ship.png`.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0380 - Samus in the blue ship cutscene

- Ported `cutscenes/samus_in_blue_ship.c` in `mzm_cutscene.c`, with the
  cutscene utilities it needs:
  - the COLOR_FADING_CANCEL case of `CutsceneStartBackgroundFading` and the
    fade-in stages of `CutsceneUpdateFading`, on the EWRAM palette copies;
  - `CutsceneFadeScreenToBlack`, `CutsceneReset`, `UpdateCutsceneOamDataId`;
  - `ProcessCutsceneOam` through the existing menu object routine.
- The cutscene reads the ROM bank: BG3 holds Samus at the controls, and the
  control panel object is alpha-blended over it. In order:
  - it fades in from black;
  - the panel lights up with its sound, then flickers;
  - BG3 shakes with the powering-up sound;
  - it ends after 1.5 seconds.
- The escape ship's request now freezes the room (no sprite update, as in
  `SUB_GAME_MODE_LOADING_ROOM`) while the game fades out. After the
  cutscene, the game fades back in as it was, as after the pause screen
  (`PAUSE_SCREEN_PAUSE_OR_CUTSCENE`).
- Divergence: the native blue-ship fade with its BG gradients is
  approximated by the pause screen's COLOR_FADING_CANCEL fade.
- Not ported: the Chozodia escape request at the end of the ship's flight.
  It is still reported as not ported.
- A bank-backed test follows the stages, the sounds, the fade length, the
  flicker, the shake and the end.
- Captures in `chozodia_092` (event 74, energy 1299, a short jump into the
  open ship) show the cockpit with the panel lighting up, then the ship
  lifting off (ignored `captures/chozodia/blue-ship-cutscene.png` and
  `escape-ship-lift-off.png`).
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0379 - Chozodia escape ship and gates

- Ported `sprites_ai/escape_ship.c` and `sprites_ai/escape_gate.c` in
  `mzm_escape_ship.c`, and the escape-ship pirate
  (`sprites_ai/escape_ship_pirate.c`) as its own first poses in
  `ai_space_pirate`:
  - once Mecha Ridley is dead and Samus is close, the ship opens and
    spawns its pirate, which jumps out;
  - landing on the open ship sets `EVENT_ESCAPED_CHOZODIA`, clears Samus's
    invincibility and puts her in the cockpit, where the ship holds her
    while it closes, lifts off swaying (`sEscapeShipHoveringY/XMovement`,
    exported), hovers and flies off right;
  - while Samus is inside, the ship keeps sprites from touching her,
    bursts pirate lasers, draws the pirates behind it and, once it flies
    fast, destroys the pirates in its way;
  - the ship's top goes in front of Samus, its tail flashes and its flames
    hide once it is gone;
  - the first gate loads its own graphics, spawns the two black pirates,
    opens once they are dead and Samus is near, then spawns the pirate
    chasing the ship; the second gate opens once the ship hovers.
- The exporter bakes the ship's and the gate's flashing palette rows as
  `/f0`-`/f2` variants, the gate drawn from `sEscapeGateAndTimerGfx` at
  OBJ VRAM 0x7800 with its palette in the last OBJ row.
- Not ported: the blue-ship and Chozodia escape cutscene effects. The
  runtime receives both requests, reports them as not ported and keeps the
  room running.
- A ROM-free regression follows the ship from closed to escaped (the
  pirate's first poses, Samus's capture, the timings, the sway, the
  cutscene requests, laser and pirate collisions) and both gates through
  their black pirates, blocks and openings.
- A capture in `chozodia_092` (event 74) shows the first gate, its two
  black pirates and the ship's nose (ignored
  `captures/chozodia/escape-gate.png`).
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0378 - Black Space Pirates

- Ported `sprites_ai/black_space_pirate.c` into `mzm_space_pirate.c`,
  reusing the pirate routines. The black pirates appear after the Fully
  Powered Suit:
  - they use palette row 2, baked as a variant, and wait until they are on
    screen;
  - only the Plasma Beam and missiles hurt them; other beams are absorbed;
  - the other pirates' lasers burst on contact with them;
  - they charge and shoot twice as fast;
  - they hop over the pirate in front, or jump after shooting, using their
    own jump table (exported) and a higher top speed when alerted;
  - they die without the alarm bookkeeping.
- A ROM-free regression covers the waiting, the beam absorption and the
  Plasma damage, the laser bursting, the double-speed charge, the shot
  timing, and death after a laser hit with the alarm left untouched.
- A capture in `chozodia_013` (event 67) shows two black pirates on the
  walkway (ignored `captures/chozodia/black-pirates.png`).
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0377 - Imago

- Ported the adult Imago boss (`sprites_ai/imago.c`) in `mzm_imago.c`,
  together with its parts, needles, damaged stinger and five eggs. It was
  the last major boss not yet ported.
- **Waiting.** Imago sits in the ceiling until the last egg is broken.
  Then the battle starts and its body becomes vulnerable.
- **Movement.** It sweeps the room low, hovering over the floor and its
  slopes, and gets faster as it is hurt. It climbs back at either end and
  dives at Samus near that end, otherwise it turns around.
- **Needles and colours.** Below two thirds of its health, or when Samus
  has no missiles, it shoots needles, faster below one third. Its body
  reddens and its core flashes through the ROM-exported row table.
- **Death.** When killed it drops its stinger and keeps flying while
  burning. It waits for Samus at the Super Missile, then breaks through
  the wall column by column and explodes. Event 35 is set and the doors
  unlock once the Super Missiles are collected.
- **Contact and assets.** Added the big-knockback contact (`SSC_HURTS_BIG_KNOCKBACK`
  and its stop variant). Exported the four multi-sprite tables, the
  sway, stinger and core-palette tables, and the palette rows 1, 2, 4
  and 5.
- **Tests.** A ROM-free regression covers the eggs and the wake-up, body
  vulnerability, the descent to the floor, the sweep, needles, body tint,
  the climb with a dive or turn, death with the stinger, the wait at the
  Super Missile, the wall charge (block counts and timing), the
  explosions, the conditional event and the reload.
- **In game.** A capture in `ridley_019` shows the eggs before the fight
  (ignored `captures/imago/eggs.png`).
- Follow-up fix: the u8 `sImagoDynamicPaletteData` moved from `AI_TABLES`
  (whose s16 declaration test it broke in the first Imago commit) to a
  dedicated `BYTE_PAIR_TABLES` export with its own declaration test.
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0376 - Elevator statues

- Ported `sprites_ai/elevator_statue.c` in `mzm_elevator_statue.c`,
  covering the Kraid and Ridley statues and their debris:
  - each statue stands as solid, non-grippable blocks;
  - once its boss is dead and Samus is off the elevator, it sets its event
    (31 or 38), removes its blocks with a horizontal shake and sound, drops
    the head debris, lands with dust and a vertical shake, and leaves two
    rubble blocks;
  - on reload it is already rubble.
- The ROM-free regression covers the standing blocks, the elevator and
  boss conditions, the crumbling event, sound and blocks, the debris, the
  landing, the rubble and the reload.
- A capture in `brinstar_008` shows the Kraid statue standing, with Samus
  on its column (ignored `captures/elevator-statue/kraid-standing.png`).
- Validation on 2026-10-19: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0375 - Falling Chozo pillar and the Power Bomb theft

- Ported the scripted Chozodia sprites in `mzm_chozodia_scripted.c`:
  - the falling pillar is drawn behind BG1. It appears once the Ruins
    Test suit animation ends and it is on screen, then sinks 256
    subpixels with a shake and dust every 16 frames, a final shake and its
    landing sound;
  - the pirate carrying the Power Bomb shows once in each of its two rooms
    (events 71 and 72), walks right with its footsteps and vanishes at the
    next pirate zoneline;
  - the Power Bomb on its pedestal and its invisible trigger are removed
    once Samus touches the trigger (event 70).
- The pillar depends on the unported getting-fully-powered cutscene, so it
  does not fall in game yet.
- A ROM-free regression covers the pillar's wait, fall, timing and sounds,
  the thief's once-per-room event, walk and zoneline exit, and the theft
  event and reload removal.
- A capture in `chozodia_032` shows the Power Bomb on its statue.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0374 - Ruins Test, second slice

- Ported the rest of the Ruins Test's actors:
  - the ghost fades in (with the blend timing), follows the main actor and
    flashes with it, then fades back and out at the center;
  - the outline appears before the ghost and also warns of lightning;
  - static lightning bolts explode on the ground, while ground bolts split
    into two waves that run along the floor and up the walls, after which
    the ghost moves again;
  - the shootable symbol either despawns with a sound fade or, when hit,
    flies to its corner of the arena as a placed symbol. The fourth
    placed symbol sends the ghost away, and all four light up again;
  - the closing reflection turns Samus, if she stands on it, to face the
    background, runs the suited-reflection timing, fades the music and
    starts the cutscene fade, and later shows the Fully Powered Suit
    banner.
- Not ported: the palette and graphics DMA, the ghost's alpha blending,
  the background flash and the getting-fully-powered cutscene. Without
  that cutscene the fight stops at the fade.
- The ROM-free regression covers the ghost's fades and idle following,
  the placement and counting of the symbols, the fourth symbol's
  hand-off, the shootable symbol's lifecycle, the lightning warning,
  ground split, wall climb and static explosion, and the closing
  reflection up to the banner.
- A capture in `chozodia_042` shows the fight started, with the ghost
  under its cover after Samus turns (ignored
  `captures/ruins-test/fight-start.png`).
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0373 - Ruins Test, first slice (and Baristute random fix)

- Ported the Ruins Test's main actor, its symbol intro, the starting
  reflection and the reflection cover in `mzm_ruins_test.c`:
  - Samus is frozen facing the background, below the actor;
  - the four symbols light up with the battle music and merge, turning
    the ghost into her reflection, and Samus is freed once she turns;
  - the shootable symbol appears after random delays drawn from
    `sRandomNumberTable`. An uncharged pistol shot on it hurts Samus by
    25, 50 or 100 energy (by difficulty), with the Ruins Test forced
    movement; a charged shot keeps the symbol out;
  - the ghost moves in the widening circle and then the atom pattern,
    triggers ground and standing lightning, and returns to the center
    after the fourth symbol;
  - its despawn removes the ceiling and sets event 67.
- The sprite link to Samus now carries her invincibility timer and the
  forced movement of a hurt request.
- `sRandomNumberTable` is now exported. The Baristute's second drop now
  reads that table instead of the sprite random table.
- Not ported yet: the ghost, outline, lightning, shootable symbol and
  closing reflection actors (their spawns are cleared), and the dynamic
  palettes and graphics DMA. The symbols are therefore invisible, and the
  fight cannot be finished in game.
- A ROM-free regression covers the intro, the symbol merge, the
  reflection, freeing Samus, the cover, the reflected pistol damage, the
  symbol timing and shot, the ghost spawning, the circle start, the return
  to the center, the despawn, the ceiling removal and the event.
- A capture in `chozodia_042` shows Samus frozen under the ghost during
  the merge (ignored `captures/ruins-test/intro.png`). In that capture
  suitless Samus is drawn with suit colours, which is a separate Samus
  rendering issue.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0372 - Tangle Vines

- Ported `sprites_ai/tangle_vine.c` in `mzm_tangle_vine.c`, which covers
  three kinds of vines:
  - the four plain vines (tall, medium, curved and short) stand on the
    ground, or hang from the ceiling a block higher, with their native
    hitboxes. They are solid, hurt on contact and explode at their native
    heights;
  - the red Geruta vine and the two larva vines keep their offsets and
    hitboxes;
  - the vine Geruta animates its grip and its Geruta through the
    ROM-exported multi-sprite table. Killing the Geruta leaves a bare
    vine; killing the vine frees the Geruta as a red Geruta.
- Exported the fifteen vine tables and the vine Geruta multi-sprite table.
- A ROM-free regression covers ground and ceiling placement, the damage
  sound, and the vine Geruta's parts, multi-sprite offsets, Geruta death
  and vine death.
- A capture in `norfair_024` shows the vines, the red vines and a Geruta
  perched on a vine.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0371 - Baristutes

- Ported `sprites_ai/baristute.c` in `mzm_baristute.c`, covering the
  Norfair Baristutes and Kraid's locked pair:
  - after idling they face Samus and, with room in front, either jump
    along the ROM-exported arc or walk six (or twelve) blocks;
  - walls turn them around, and ledges make them fall along the falling
    table;
  - they land with a short screen shake;
  - their health quarters set damage rows 1 to 3, which are baked as
    palette variants, and switch them to the faster walk animations and
    speeds;
  - death leaves two extra energy or missile drops.
- Kraid's pair behaves differently: the upper Baristute always walks and
  the lower one always jumps. They lock the room until both are dead, then
  set event 52 and unlock the doors.
- A ROM-free regression covers the init, the jump arc and landing, the
  hurt walk speed, wall stops, damage rows, falling, and the pair's lock,
  event and reload removal.
- A capture in `kraid_007` shows the pair walking and idling.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0370 - Brinstar hives and Mellows

- Ported `sprites_ai/hive.c` in `mzm_hive.c`, with four AIs:
  - the hive spawns its roots and four Mellows, replaces any Mellow it
    loses, and shrinks twice (under half, then a quarter of its health);
  - its roots follow it and vanish at the first shrink;
  - when the last hive dies, event 44 is set and its idle Mellows flee
    and despawn off screen;
  - Mellows hover on the ROM-exported idle tables, chase Samus around
    four waypoints with the native acceleration, and push overlapping
    Mellows apart;
  - Mellow swarms keep five Mellows (or 3 to 15, by energy) diving from
    above the camera.
- Added a shared `SpriteSpawnPrimary` helper. Added the `SSC_HURTS_SAMUS_SOLID`
  contact (solid side stop plus damage) and `SSC_MELLOW` contact.
- Exported the hive graphics (seven tables) and the two Mellow idle tables.
- A ROM-free regression covers the spawn, the idle hover, Samus detection
  and the chase start, Mellow replacement, both shrinks, root removal,
  death, event 44, fleeing, event-gated spawns and both swarm variants.
- A capture in `brinstar_015` shows the hives on their pillars, with their
  Mellows hovering and chasing Samus.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0369 - Mecha Ridley cover, core, death and escape start

- Ported `BlackSpacePirateProjectileCollision` for the Mecha Ridley parts:
  they test projectile positions inside their own hitboxes. Beams other
  than the Plasma Beam are absorbed, immune parts make missiles tumble, and
  the cover takes the damage. The cover and the core are weak only to
  missiles. The projectile hit routines were extracted into one shared
  function, without changing the common pass.
- Below three quarters of its health the cover enables the fireballs. At
  zero it explodes, exposing the core with the core's own health; its
  quarters set the health threshold and the parts' damage tints. The
  exporter bakes the damage tints (palette rows 2 to 4). Tables whose rows
  would overflow the eight-row palette keep their untinted look.
- The death sequence runs from the boss explosion and music fade through
  the timed explosions, the collapse, ten grouped large energy drops and
  the eye's last looks (the missing `EyeDying` table is now exported). It
  ends by setting event 74, showing the Chozodia escape banner (which starts
  the escape), unlocking the doors and playing the escape music. The
  palette and destroyed-graphics copies keep only their timing.
- The ROM-free regression covers the beam absorption, missile damage and
  tumbling, the cover threshold and explosion, core exposure, health
  thresholds and tints, and every death stage up to the escape start.
- A scripted super-missile fight in `chozodia_072` damaged the cover and
  triggered the high-stance fireball attack in game (ignored
  `captures/mecha-ridley/fireballs.png`). The scripted Samus died before
  winning, so the core, death and escape are not yet confirmed in game.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0368 - Mecha Ridley laser, missiles and fireballs

- Ported the three Mecha Ridley weapon actors. The eye's laser attack
  glows, picks one of four random directions (never straight up) and
  spawns a laser that flies 20 subpixels a frame until it meets a wall.
  The launcher's three missiles rise and slow down, then home on Samus with
  the native acceleration and turn toward her by PI / 64 a frame. High and
  low fireballs follow their native trajectories. Missiles and fireballs
  explode on walls and on contact, and keep the live counters that gate
  the next attack.
- Added the shared `SpriteUtilMakeSpriteFaceSamusRotation` and
  `SpriteUtilMoveSpriteTowardsSamus` helpers.
- The exporter now bakes the seven weapon graphics stages that
  `MechaRidleyLoadMissilesGfx` and `LoadFireballsGfx` copy to OBJ VRAM
  0x4280. The renderer draws the stage the launcher or head copied last.
  Before this patch, both weapons rendered as empty 1x1 frames.
- The ROM-free regressions cover the laser attack and actor, the missile
  rise, homing, rotation and explosion, the fireball flight and explosion,
  both graphics cycles, and the weapon staging in the exporter.
- In `chozodia_072` with no ammunition, the user saw the missiles on the
  third attack. A frame-1850 capture (ignored
  `captures/mecha-ridley/missiles.png`) shows all three homing on Samus
  with their animated exhaust.
- Remaining boundary: the cover/core projectile handoff, the damage
  palette rows, the head glow, death, drops and the escape start. The laser
  and fireballs need a damaged or broken cover, so in game they still
  depend on that handoff.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 440/440 passed; generated parity and dashboard data are fresh.

## Patch 0367 - Mecha Ridley attack selection, claw and stances

- Mecha Ridley now tracks Samus's height with the native hysteresis (high
  at once above three blocks, low or middle after two seconds) and moves
  between its three stances through the six native transition tables.
- Every 64 frames the battle idle chooses a claw swipe half of the time
  (never twice in a row), the fireball attack once the cover is damaged and
  Samus has missiles, or the laser. The claw lunges a pixel per swing frame,
  waits forward with the native health-scaled delay, raises the arm and
  curls; damage taken while standing or curled makes it retract and crawl
  back to its spawn x. The fireball choreography (rise, open mouth, a
  fireball every 16 frames alternating low and high, close, retract) and
  the 170-frame missile timer are wired.
- After the entrance the arms keep their own animations with the native
  per-frame hitboxes, swipe sounds and ground shake; the neck has its
  per-animation hitboxes and sounds; the eye blinks and only glows for the
  laser once the cover is broken; the missile launcher opens only below 150
  energy or without missiles. The parts now apply their multi-sprite offset
  without their animation as SpriteUtilSyncCurrentSpritePositionWithSubSprite1Position does.
- The exporter adds sixteen multi-sprite tables (stances, transitions,
  backward crawl, high fireball mouth and the dying stances); a private
  rebuild produced 17,856 sequences and 105,052 frames.
- The ROM-free regression covers the claw selection, lunge, ground shake,
  standing up, curling, retreat and crawl back, laser fallback, launcher
  cancellation and opening, height hysteresis, neck sound and the whole
  fireball choreography with its spawn.
- Private captures in `chozodia_072` (ignored
  `captures/mecha-ridley/claw-swipe.png` and `standing-up.png`) show the
  high-stance swipe hitting Samus at frame 1000 and the raised arm in the
  forward position at frame 1250.
- Remaining boundary: the laser, missile and fireball actors and their
  graphics swaps, the head glow, the cover/core projectile handoff, the
  damage palette rows, death, drops and the escape start. Until the cover
  can be damaged, the fireball and retreat branches are only reachable in
  the regression.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed; generated parity and dashboard data are fresh.

## Patch 0366 - Mecha Ridley entrance and multipart body

- Added the native `chozodia_072` Mecha Ridley actor and its ten secondary
  body actors. Their room slots, hitboxes, contact types, health, draw order
  and graphics ownership now match the pinned Zero Mission implementation.
- Exported all 63 Mecha Ridley OAM tables and the crawling-low and
  standing-low multi-sprite tables. A private rebuild produced 17,856
  sequences and 105,052 frames without tracking any ROM-derived output.
- The encounter now honors event 74, locks the arena, waits five seconds,
  crawls eleven blocks in from the right using both arm animation frames,
  settles into its low stance and starts the battle music after the native
  seven-stage entrance timing.
- Added `SSC_MECHA_RIDLEY` contact damage and a ROM-free regression covering
  the complete actor assembly, part metadata, crawl displacement, battle
  transition and defeated-event removal.
- A private frame-1000 capture in `chozodia_072` confirmed that all eleven
  layers assemble at the native target position. The ignored local capture
  is `captures/mecha-ridley/battle-idle.png`.
- Remaining boundary: attacks, height transitions, the special cover/core
  projectile handoff, death, grouped drops and the Chozodia escape start are
  not connected yet. The next slice starts with the low-stance selector and
  claw swipe before adding the three weapon actors.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed. Generated parity/dashboard data is fresh, the
  proprietary-file guard passed, and both entrance and battle-idle captures
  completed with all eleven actors present.

## Patch 0365 - grouped energy drop and Long Beam encounter audit

- Added `PSPRITE_MULTIPLE_LARGE_ENERGY`, the grouped pickup emitted by Mecha
  Ridley. Its follower part selects one of the three native OAM tables, and
  collection restores three large drops (60 energy) with the native sound
  and maximum-energy clamp.
- Enemy drops now copy BG1's live priority and use native draw order 1. The
  private common-sprite export includes all three grouped-drop tables; a full
  rebuild produced 17,088 sequences and 101,560 frames without tracking any
  extracted graphics.
- The focused regression covers all three follower parts, energy refill and
  clamping, sound, draw order and background priority. The native sprite ID,
  OAM tables and dashboard item classification are checked from tracked
  sources.
- Audited the preceding Long Beam route instead of duplicating it: the Chozo
  statue already grants Long Beam and event 8, while `CAA_REMOVE_SOLID`
  already clears both BG1 graphics and Clipdata for the Dessgeega's three
  floor blocks. A real `brinstar_009` capture confirmed the open floor and
  landed actor, so Dessgeega is now functional in the parity checklist.
- Remaining boundary: Mecha Ridley is not ported, so the grouped pickup has no
  natural in-game source yet. Port that boss encounter before claiming an
  ordinary-play capture of this drop.
- Validation on 2026-10-18: the warning-clean full build, CTest 35/35 and
  Python 438/438 passed. Every reconstruction task completed or was up to
  date, the private library contained all three grouped-drop animations,
  generated-data checks were fresh and the proprietary-file guard passed.

## Patch 0349 - Zero Mission Acid Worm encounter

- Added the `kraid_005` Acid Worm primary actor, its nine body actors and its
  spit projectile to the native sprite engine. The actor now waits for the
  activated zipline and for Samus to ride into range before locking and
  opening the arena.
- The entrance breaks the central column and raises the complete segmented
  body through the acid. The combat alternates a hooked lunge with a
  three-projectile spit volley; the exposed weak-point actor transfers damage
  to the 600-health main actor while the mouth and remaining body stay immune.
- The room effect surface now accepts the native-style subpixel offset used by
  the entrance, attacks and death. Killing the worm sequences the body
  explosions, lowers the acid, removes the two floor blocks, sets event 27 and
  unlocks the doors. Reloading the completed room restores that open state.
- `scripts/mzm_sprites.py` exports the fifteen Acid Worm OAM tables. The
  runtime maps primary id 119 and secondary ids 37 and 38 to the new family;
  private graphics and ROM data remain outside Git.
- The current body curve is a compact interpolation rather than the native
  sine/rotation tables. The optional late-fight acid raise cycle, exact
  collision-abort motion, native debris layout, music changes and emulator
  comparison remain open.
- Validation on 2026-10-16: warning-clean build, the focused sprite test
  replayed the zipline trigger, entrance, nine-part spawn, weak-point damage,
  death event, arena opening and liquid movement; a `kraid_005` capture loaded
  all thirteen room actors with their native graphics.

## Patch 0348 - Zero Mission Deorem encounter

- Added Deorem's two native room actors and its secondary segment, eye and
  thorn actors to the sprite engine. The encounter now wakes when an
  missile-equipped Samus reaches the arena centre, locks the camera and doors,
  forms both body columns, exposes the missile-vulnerable eye, lunges and
  retracts, throws four-thorn volleys and changes palette at low health.
- The eye carries the native 60 health and encounter timer. A surviving
  Deorem retreats between `brinstar_012` and `brinstar_019`, toggling events
  24 and 25; killing it sets both encounter events (and event 26 at the second
  location), removes the temporary arena walls, unlocks the doors and spawns
  the collectible Charge Beam after the death sequence.
- `scripts/mzm_sprites.py` now exports Deorem's fifteen OAM tables and its two
  movement tables from the private ROM. The runtime maps both primary ids and
  all three secondary ids to one graphics family without tracking any ROM
  payload.
- The long body-segment burrowing choreography is compacted while preserving
  the encounter's visible phases and collision actors. Native music fades,
  minimap updates and emulator comparison remain open.
- Validation on 2026-10-16: warning-clean sprite and room-runtime builds, the
  sprite unit binary passed, a direct `brinstar_012` capture reached the open
  eye with all 20 room actors, and a missile capture killed Deorem, set event
  24 and left the Charge Beam and its glow active.

## Patch 0328 - Zero Mission Zebes escape countdown and destruction

- `src/runtime/mzm_escape.c` ports `EscapeDetermineTimer`,
  `EscapeCheckHasEscaped`, `EscapeSetTimer` and `EscapeUpdateTimer`. The event
  route selects Tourian or Chozodia, the difficulty selects the native limit,
  and the hundredths/seconds countdown freezes outside gameplay and while a
  message prevents movement.
- Mother Brain now starts the invisible `PSPRITE_EXPLOSION_ZEBES_ESCAPE`
  behavior at its native offset. Its camera-relative particles and debris,
  Samus-height correction, four sounds and alternating two-axis shakes follow
  `ExplosionZebesEscape`.
- The Zebes and Chozodia banners emit `EscapeStart` when their spawning
  animation reaches the static table. The room runtime starts the clock there,
  draws the countdown at the native absolute HUD position and marks the escape
  failed at zero.
- Brinstar and Chozodia save platforms now consult the same event-driven
  escape selection and initialize in their native disabled state during
  either evacuation.
- Remaining divergence: the failed-escape and successful-exit cutscenes are not
  connected, and the explosion state is outside the native sprite slots.
- Tests cover event selection, all timer boundaries relevant to startup and
  failure, movement prevention, the banner trigger, Mother Brain's trigger,
  and a deterministic explosion particle/debris/shake/sound frame.
- Validation on 2026-10-14: warning-clean build, CTest 35/35, Python 471/471,
  the 119-feature evidence dashboard regenerated, native parity regenerated,
  and the proprietary-file guard passed.

## Patch 0327 - evidence-aware development dashboard

- Completed the dashboard evidence migration started in `1f03c78`: the 118
  registered features now carry separate automated-test, manual in-game,
  emulator-comparison and playthrough evidence. The generated data reflects
  the current 70 Zero Mission records (45 functional, 25 partial) without
  presenting that registered subset as game-completion or fidelity coverage.
- Feature implementation files, tests and registry declarations are clickable.
  They resolve locally when the dashboard is opened through `file://` and to
  the repository when the `dashboard/` directory is published by itself.
- The in-game view reports unfiltered evidence counts while search and status
  filters report their displayed subset. Bestiary rows inherit the status and
  evidence of their sprite-family annotation; tests cover the family-to-
  annotation mapping and fail closed on an unregistered family.
- Fixed the native-parity malformed-row fixtures for the 15-column evidence
  schema and removed a duplicate dashboard registry field.
- Visual checks on 2026-10-14 covered Firefox and Chromium through `file://`,
  a 390-pixel Chromium viewport, and a Chromium standalone HTTP directory.
  No JavaScript errors occurred, and standalone source links resolved to the
  repository.
- Audio validation retained the SDL callback and fractional PSG/FIFO fixes:
  the ROM-free regression measured no silent FIFO samples at frame boundaries,
  and covers concurrent music/SFX, pause/reset, room-track fade/transition and
  jingle/resume. A normal intro runtime smoke opened the real audio device for
  eight seconds with isolated empty save paths and no runtime error; this is
  not a listening comparison against hardware or an emulator.

## Patch 0326 - Zero Mission Mother Brain

- MotherBrain and its part, beam, block and glass sprites
  (`src/runtime/mzm_mother_brain.c`): the glass that missiles crack in
  three BG0 stages and shatter, the block Samus can't grab, the eye that
  opens after the glass and closes after enough damage, the charged beam
  that ignores invincibility, and the death that sets its event and shows
  the escape banner with the escape music.
- Engine: a background block hook (BgClipSetBgBlockValue),
  SSC_HURTS_SAMUS_IGNORE_INVINCIBILITY, the sprites' charge palette rows.
- Checked by capture in tourian_004 (missiles from a platform crack the
  glass to its second stage) and by a test of the whole fight; the escape
  sequence itself is not ported yet.

## Patch 0325 - Zero Mission Ridley

- Ridley, RidleyPart, RidleyTail and RidleyFireball (`src/runtime/mzm_ridley.c`):
  the Gravity Suit condition, the drop when Samus faces left on the
  ground, the landing that breaks the ground, the hovering and sliding,
  the turns, the small, big and grab fireballs, the tail's diagonal and
  vertical swipes (gSubSpriteData2), the claw that grabs, lifts and drains
  Samus until she is freed, the health palettes, the camera lock and the
  death with its event and music.
- Engine: multi-sprite tables of up to 24 frames, gSubSpriteData2,
  gBossWork.work1/work3, gChangedInput in the sprite frame,
  SSC_RIDLEY_CLAW and SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT_NO_KNOCKBACK, the
  claw's invincibility write; `--energy` above the maximum raises it (a
  diagnostic of long fights).
- Checked by capture in ridley_012 (the drop and fireballs, the grab
  lifting Samus in front of its mouth, her release) and by a test.

## Patch 0324 - Blocks removed by sprites vanish from the screen

- Fix: the sprites' ClipdataProcess(CAA_REMOVE_SOLID) only cleared the
  collision, so the platform blocks Kraid's spikes break stayed drawn and
  Samus seemed to fall through a solid floor. The hook now goes through the
  block engine's CAA_REMOVE_SOLID (BlockApplyCcaa), which clears the BG1
  block too unless a sprite made it solid; without the block tables only
  the collision changes as before.
- Checked by capture in kraid_030: the middle spike reaching the wall
  clears the platform above it and Samus falls into the gap.

## Patch 0323 - Kraid checked in game

- The runtime draws Kraid's sprites with their health palette rows
  (/row1 to /row3): the head, arms and holes kept their first colours while
  the BG2 body changed, leaving light boxes around the holes.
- Played in kraid_030 from the right platforms: standing shots hit the
  left arm and bounce off, jumping shots reach the mouth (a super missile
  took Kraid from 300 to 200 health), and the body and sprites change
  colour together.
- The test now follows the death to its end: the sinking to the floor,
  the two seconds of explosions, the boss music after a fade, BG2 hidden
  and every part exploding (the arms after their 400 frames).

## Patch 0322 - Zero Mission Kraid

- Kraid, KraidPart, KraidSpike and KraidNail (`src/runtime/mzm_kraid.c`):
  the rise, the steps with the multi-sprite tables and the BG2 body, the
  mouth that missiles and charged beams hurt, the health palettes (sprite
  rows and BG row 10), the swipe, the nails, the spikes that break the
  platforms and carry Samus, and the death (event, sinking, explosions,
  PE_MAIN_BOSS_DEATH, the boss killed music, BG2 hidden, the parts'
  explosions and drops).
- Engine: twelve-part multi-sprite tables (sKraidMultiSpriteData_*, the
  exporter's MultiSpriteData parser now takes a trailing comma),
  gSubSpriteData1's health and work bytes, SSC_HURTS_KNOCKBACK_IF_INVINCIBLE
  and SSC_HURTS_RIGHT_CAN_STAND_ON_TOP, SpriteUtilSpriteDeath's playSound,
  gLockScreen for the camera, BG2 hidden, a BG palette row request,
  BGCNT_LOW_PRIORITY sprites drawn behind the backgrounds above BG3,
  FadeCurrentMusicAndQueueNextMusic and the main boss death particle.
- Checked by capture in kraid_030 (the rise, the parts, the spikes) and by
  a test of the parts, the rise, the mouth's damage, the shut mouth
  opening, the palette, a bouncing missile and the death's event; the
  fight itself has not been played through.

## Patch 0321 - Zero Mission Metroids

- Metroid and MetroidShell (`src/runtime/mzm_metroid.c`): the room lock
  and event, the growth on screen (sMetroidSpawning tables, Q8.8 scaling,
  palette rows 3 to 1), the chase with its inertia and bounces, the grab
  (SSC_METROID: the drain with its suit-dependent invincibility, the
  sucking sounds, the grab palette cycle), the bomb release, the frozen
  id that missiles hurt, and the death of body and shell that clears the
  room.
- gEquipment.grabbedByMetroid slows Samus outside liquids; the sprite
  frame carries her horizontal moving direction; the renderer scales
  ROTATION_SCALING_SINGLE sprites by their scaling (around the frame's
  centre) and bakes the grab palette (/gN) and growth rows (/rowN).
- Checked by capture in tourian_014 and by a test of the lock, growth,
  chase, grab, drain, bomb release, freeze, thaw, death and event.

## Patch 0320 - Zero Mission Mother Brain Rinkas

- RinkaMotherBrain (the six Tourian Rinka ids) in the Rinka engine:
  the cannon's graphics, frozenPaletteRowOffset 1 (a new
  `frozen_row_offset` the freeze and stun rows subtract), respawns that
  wait off screen and pick a second spawn point 20 to 40 blocks left when
  Samus is far enough left of the first, flights from the point in use,
  and the end with Mother Brain (EVENT_MOTHER_BRAIN_KILLED).
- Every Rinka now has RinkaInit's draw order of 3, and
  ProjectileCheckHittingSprite visits the hittable sprites by draw order,
  then slot, from a list taken before the pass, as the game does (it used
  slot order, assuming one draw order).
- Checked by capture in tourian_004 (the rings flying at Samus) and by a
  test of the second spawn point, the flight from it, the return to the
  first and the removal with Mother Brain.

## Patch 0319 - Zero Mission Worker Robots and the projectile hit order

- WorkerRobot (`src/runtime/mzm_worker_robot.c`): solid, immune to
  projectiles, asleep until a hit's stun flash wakes them; they stand half
  a second, walk until Samus is in front of them or a ledge or wall stops
  them, lie down the other way, carry Samus standing on them, are pushed
  away by a missile (half a second) or a super missile (a second) on
  their side, and fall with sSpritesFallingSpeed or, asleep,
  sWorkerRobotSleepingFallingSpeed.
- ProjectileCheckHittingSprite now runs where ProjectileUpdate calls it,
  after the new projectile's spawn and before the projectile loop
  (MzmWeapons.hit_sprites); it ran after the loop, which made the robots'
  own missile pass miss every missile. The bomb's sprite flag is set when
  the explosion starts instead of a frame later.
- Checked by capture in chozodia_014 (a missile wakes the robot, it walks
  away from Samus, a second missile pushes it 28 pixels) and by tests
  (waking, walking, both pushes, Samus in front, sleeping fall, the
  bomb's single sprite pass).

## Patch 0318 - Zero Mission audio buzz

- Fix: mzm_audio_render reset the FIFO sample the DACs hold to zero at
  the start of every GBA frame, so the first three or four output samples
  of each frame were silent: 60 clicks a second, heard as a buzz. The held
  samples now persist across frames, and the PSG steps keep their
  fractional cycles (exact at the runtime's 65536 Hz, 0.4% flat at
  48 kHz before).
- Found by rendering the extracted Brinstar and title music offline: the
  output's second difference folded on the 59.73 Hz frame period peaked at
  ten times its mean at the frame boundary, while the mixer's FIFO stream
  did not. A ROM-free test checks that a playing DirectSound note has no
  silent samples; it fails on the old code.

## Patch 0317 - Zero Mission Zebetites and Tourian cannons

- Zebetite, Cannon, CannonBullet (`src/runtime/mzm_tourian.c`): the
  Zebetites heal while not hurt, show their damage phases and move 20
  blocks left after their first destruction, setting the Zebetite
  events; the cannons sweep their aims, shoot by chance, change place
  with Samus' position and disappear once Mother Brain is dead, their
  bullets flying until a block.
- The second Zebetite and the other cannons draw with the first one's
  graphics (the left Zebetite of tourian_004 was invisible).
- Tests cover the healing, both destructions, easy's slower healing,
  the cannon sweep and firing, and a diagonal bullet down to the floor.

## Patch 0316 - Zero Mission music back after the pause

- Fix: DecreaseMusicVolume queued sound 63 (MUSIC_IMAGO_COCOON_BATTLE)
  instead of SOUND_63 (0x63), so opening the pause replaced the room's
  music and leaving it brought nothing back. A ROM-free test plays the
  pause's volume change on a synthetic bank; every other sound constant
  of the runtime was checked against the decompilation's enum.

## Patch 0315 - Zero Mission Polyps

- Polyp and PolypProjectile (`src/runtime/mzm_polyp.c`): the lava plants
  spit an arcing projectile towards Samus every two seconds on screen.
- SSC_KNOCKS_BACK_SAMUS: the sprites can now request Samus's knockback
  pose (no damage).
- Checked by capture in norfair_011; tests in `tests/test_mzm_sprites.c`.

## Patch 0314 - Zero Mission Atomics

- Atomic and AtomicElectricity (`src/runtime/mzm_atomic.c`): hovering,
  fleeing, charging with their palette flashing, shooting their
  electricity at Samus, chasing her arm cannon while she charges.
- SSC_HURTS_SAMUS_A_LOT (five times the damage, stronger knockback); the
  sprites' contact damage now follows the difficulty.
- The exporter limits a sprite's row variants to chosen tables (the
  Atomic's body flashes, its electricity does not).
- Checked by capture in chozodia_058; tests in `tests/test_mzm_sprites.c`.

## Patch 0313 - Zero Mission Chozodia save platforms

- SavePlatformChozodia and its part share the save platform's code
  (`src/runtime/mzm_gadget_sprites.c`) with their own tables, part and
  sounds, and refill Samus before the save prompt (energy, missiles,
  super missiles, power bombs, the refill light and message).
- The refill helper (SpriteUtilRefill*) is shared with the Chozo statues.
- Checked by capture in chozodia_004 (refill, save, load); tests in
  `tests/test_mzm_sprites.c`.

## Patch 0312 - Zero Mission block tables load again

- Fix a regression of patch 0311: the block table reader parsed the
  normal `tanklist increase` row as a difficulty row (`%d` skipping the
  tab), rejected the whole file, and so disabled every crumble,
  breakable and reforming block and every tank. Brinstar's first shaft
  (crumble blocks around a speed booster block) trapped Samus at the
  start. The reader now names its bad line, and a test reads the tank
  rows.

## Patch 0311 - Zero Mission native new game and gamepad without debug

- A new game is the native one: 99 energy, no item and no ammunition (the
  Morph Ball, Bombs and Power Grip are collected in the adventure), no
  event or collected item, then the gunship landing, the intro text and
  Brinstar's door 0 (RoomLoad's placement from the door's own exit, now
  exported as X lines; SamusInit's pose facing the foreground). Direct
  `--room` starts use the same empty equipment; `--item`, `--ammo` and
  `--event` set test captures up (`--without` is gone).
- The debug suit preset only sets the suit type and the Varia and Gravity
  Suits: the collected items are kept, and Samus's graphics follow the
  equipment.
- Tanks follow the difficulty (sTankIncreaseAmount[gDifficulty]).
- F1 is the only debug input: the diagnostic keys (H, Enter, F2, F6, F7,
  M, T, G, R, Page Up/Down) and the gamepad's debug chord, Guide, North,
  stick and Start catalogue buttons are gone; the animation browser is in
  the F1 menu.

## Patch 0310 - Zero Mission Parasites

- Parasite and ParasiteMultiple (`src/runtime/mzm_parasite.c`): crawling,
  jumping, grabbing Samus (SSC_PARASITE: more than three leech her energy
  without the Varia or Gravity Suit) and the Gerons (four destroy one),
  thrown off by the screw attack and any shot, killed by bombs and power
  bombs; the bug room's Parasites lock the doors until the last one dies.
- gParasiteRelated and the Chozodia alarm now also run down while movement
  is prevented, as in SpriteUpdate's second branch.
- Checked by capture in brinstar_016, brinstar_023 and brinstar_024; tests
  in `tests/test_mzm_sprites.c`.

## Patch 0309 - Zero Mission rising Chozo pillar

- RisingChozoPillar, ChozoPillarPlatform and ChozoPillarPlatformShadow
  (`src/runtime/mzm_gadget_sprites.c`): after the Power Grip, Crateria's
  pillar rises in BG2 with dust, debris and shakes, then its six grippable
  platforms appear.
- BG2 in BG_PROP_MOVING rooms follows ScrollBg2 (BG1 plus gBg2Movement,
  kept within BG2); the room load clears gBg2Movement.
- The second dust particles reuse the dust tables.
- The runtime's `--event N` sets a game event in every room for test
  captures.
- Checked by capture in crateria_016 (`--event 16`); tests in
  `tests/test_mzm_sprites.c` and `tests/test_runtime_steep_slopes.c`.

## Patch 0308 - Zero Mission screen shakes, pistons and the glass tube

- Screen shakes (`src/runtime/mzm_screen_shake.c`): one shared state run
  once a frame, moving BG0 to BG2 by up to 2 pixels and BG3 by half; Samus's
  spark collisions, the power bomb, the super missile impact and the Long
  Beam Dessgeega start them, and crawlers on screen stop or fall during
  them (SpriteUtilShouldFall).
- Piston and GlassTube (`src/runtime/mzm_gadget_sprites.c`).
- Fix: the room gadgets' OAM tables past the 72nd (ziplines, gates)
  overflowed MzmSpriteKind and read zero durations; the limit is 128 with
  static asserts and a loader check.
- The projectile OAM renderer accepts up to the 128 hardware OAM entries
  (the glass tube uses more than 32).
- Checked by capture in chozodia_005 (power bomb shake), brinstar_028
  (piston) and chozodia_023 (glass tube); tests in `tests/test_mzm_sprites.c`
  and `tests/test_mzm_samus.c`.

## Patch 0307 - Zero Mission security gates

- SecurityGateDefaultOpen and SecurityGateDefaultClosed
  (`src/runtime/mzm_gadget_sprites.c`): Chozodia's gates close when the
  alarm rings, turn solid once Samus is out of them
  (SpriteCheckCollidingWithSamus) and reopen two thirds of a second after
  it stops; the sprites now see Samus's whole hitbox.
- Checked by capture in chozodia_005; tests in `tests/test_mzm_sprites.c`.

## Patch 0306 - Zero Mission ziplines

- Zipline, ZiplineButton, ZiplineGenerator and its parts
  (`src/runtime/mzm_gadget_sprites.c`): Samus in morph ball on the
  generator activates the ziplines (EVENT_ZIPLINES_ACTIVATED, the animated
  palette back on); a zipline grabbed from below carries her to the other
  stop-enemy block once lit and drops her; a shot button calls it.
- The sprite engine gains SSC_ZIPLINE (the SPOSE_ON_ZIPLINE and
  SPOSE_MORPH_BALL_ON_ZIPLINE grabs), SpriteUtilCheckOnZipline for frozen
  sprites, and SP_SOLID_FOR_PROJECTILES hits (`src/runtime/mzm_sprites.c`);
  the runtime applies the collision's SamusSetPose and gPreviousXPosition.
- Checked by capture in kraid_004; tests in `tests/test_mzm_sprites.c`.

## Patch 0305 - Zero Mission morph ball launchers

- MorphBallLauncher and its parts (`src/runtime/mzm_gadget_sprites.c`): a
  bomb set on the launcher is taken, held, and its explosion launches
  Samus in morph ball (ProjectileMorphballLauncherCheckLaunchSamus, the
  ballspark launch already ported), with the energy around her.
- The bombs gain the launcher stages (`src/runtime/mzm_projectiles.c`);
  the sprites see the projectiles (MzmSpriteFrame.weapons).
- Checked by capture in brinstar_000; tests in `tests/test_mzm_sprites.c`
  and `tests/test_mzm_samus.c`.

## Patch 0304 - Zero Mission area banners

- AreaBanner and LocationTextGetGfxSlot: entering a room through a door
  flagged DOOR_TYPE_DISPLAYS_ROOM_LOCATION shows the area's name at the
  bottom of the screen (BRINSTAR at a new game's start), and a save or map
  room's spriteset its name in the middle (SAVE ROOM), in sprite slot 0 at
  an absolute screen position, in front of every background.
- The exporter draws each location text in the banner's graphics
  (TextDrawLocation) as /ltN variants; the rooms export their location
  doors as Y lines.
- The gunship's sprites now draw with their entrance and flames palette
  rows (the variants baked in patch 0298 were not selected).
- Checked by capture; tests in `tests/test_mzm_sprites.c`.

## Patch 0303 - Zero Mission fades around the pause

- ProcessPauseButtonPress and COLOR_FADING_CANCEL: START, a map station or
  a hint statue first fade the game to black in 8 frames (Samus and the HUD
  included, the sprites still running, Samus waiting); leaving the pause
  fades the game back in over 8 frames before the music volume is reset.
- `mzm_door_fade_start_cancel` reuses the door fades' quick fade with every
  OBJ row.
- Checked by capture (brinstar_000's pause, brinstar_002's hint).

## Patch 0302 - Zero Mission death fades

- SamusDying's gMonochromeBgFading (ColorFadingApplyMonochrome): the hit
  that kills Samus fades the room to black in 8 frames and switches its
  backgrounds off; her dying animation's first flash fades the backdrop to
  white; the game over follows.
- SUB_GAME_MODE_DYING: the sprites, projectiles, particles, HUD and
  in-game timer stop and are not drawn; Samus is drawn alone.
- Checked by capture (the lava death in norfair_022 with `--energy 2`);
  `tests/test_mzm_samus.c` checks the black and white requests.

## Patch 0301 - Zero Mission Chozo statue hints

- `src/runtime/mzm_pause_screen.c`: PAUSE_SCREEN_CHOZO_HINT
  (pause_screen_sub_menus.c): the hint screen, the statue's target
  travelling to its item with a trail while the map scrolls to it, the
  elevators to the target's area, A or START to leave; the activated
  targets then show on the pause map (with the elevators towards the first
  one), the world map and as viewable areas.
- A hint statue now opens this screen when its flashing ends, then sits
  down (`src/runtime/mzm_chozo_statue.c`).
- Checked by capture (brinstar_002's Long Beam hint, then the pause map);
  test in `tests/test_mzm_screens.c`.

## Patch 0300 - Zero Mission hatch locks

- `src/runtime/mzm_hatches.c`: ConnectionLockHatches,
  ConnectionLockHatchesWithTimer and ConnectionCheckUnlockDoors with
  gDoorUnlockTimer and the gHatchesState lock masks.
- Gray hatches wait for the door timer; a sprite's LOCK_DOORS (the Long
  Beam Dessgeega) locks every hatch when the door fade starts, the arrival
  one closing; its countdown unlocks them with SOUND_DOORS_UNLOCKING and
  the flashing hatches, which then open to shots.
- The rooms export their sHatchLockEvents as Q lines (the hatch's door, the
  event, before or after, unlockable), which RoomLoad applies
  (ConnectionCheckHatchLockEvents): brinstar_009's right hatch stays locked
  until the statue room is viewed (checked by capture: shots do not open
  it), the gunship room's after Mother Brain.
- Tests in `tests/test_runtime_steep_slopes.c` (timer unlock, event lock,
  LOCK_DOORS).

## Patch 0299 - Zero Mission door fades

- `src/runtime/mzm_door_fade.c`: COLOR_FADING_DOOR_TRANSITION and
  COLOR_FADING_NO_TRANSITION (color_fading.c). A hatch fades the room to
  black but its hatches, the door transition tilemap's tube blends in on
  BG3 and covers the screen, slides to the arrival door once the room is
  loaded, then the room fades in; other doors fade to black and back.
  Samus, the pause and the in-game timer wait meanwhile.
- `src/runtime/mzm_room_gfx.c`: sDoorTransitionTilemap on BG3
  (RoomRleDecompress) and the faded room palette
  (ApplySpecialBackgroundFadingColor with the fade's row mask and
  sDoorTransitionPal); sprites fade by colour modulation.
- A door's room track now starts when its fade ends.
- The tube starts at the 320x224 view's edge and slides 336 pixels (56
  frames instead of 43 on the GBA screen).
- Checked by capture (brinstar_002 to brinstar_003, chozodia_007 to
  chozodia_037); tests in `tests/test_mzm_screens.c` and
  `tests/test_mzm_room_gfx.c`.

## Patch 0298 - Zero Mission gunship and the new game's landing

- `src/runtime/mzm_gunship.c`: the gunship and its parts (gunship.c): the
  landing of a new game with its flames, the entrance opening and the
  platform rising while Samus is near, Samus entering from the top, the
  refill, the save prompt and the save, Samus rising out of the hatch, the
  palette animations (baked as library variants), and the escape AI.
- A new game from the file select now lands in Crateria's first room (HUD
  hidden, pause disabled); half a second before touchdown the intro text's
  fades (approximated by a black veil) lead to the "Planet Zebes..."
  cutscene, then to the game in Brinstar. A file saved in the gunship
  resumes with Samus rising out of it.
- Sprites: SSC_CAN_STAND_ON_TOP, and draw orders 9 and above now draw
  behind Samus (SpriteDrawAll_LowPriority) instead of over her.
- Samus: the in-ship saving animation (sSamusAnim_256484 and its kin) is
  exported as the InShip table and used with lastWallTouchedMidAir.
- Checked by capture (landing, fade, cutscene, Brinstar; entering, refill,
  save, leaving, reload in crateria_000); tests in
  `tests/test_mzm_sprites.c` and `tests/test_mzm_samus.c`.

## Patch 0297 - Zero Mission story text cutscenes

- `src/runtime/mzm_cutscene.c`: CutsceneHandler for the non-gameplay
  cutscenes (CutsceneInit, the background blending effect, the backgrounds'
  positions, CutsceneVBlank) and the story text cutscenes: the English
  monologue drawn with the native font, the vertical centering, the fade in
  with the landing sound, A or B, the fade out.
- A new game from the file select now plays the "Planet Zebes..." intro
  text before the game; `--cutscene N` plays the intro text (1) or the
  mothership monologue (2) before the game.
- Natively the gunship's landing in Crateria leads to this cutscene; the
  landing is not ported yet.
- Checked by capture (title, new game, monologue, game); test in
  `tests/test_mzm_screens.c`.

## Patch 0296 - Zero Mission intro

- `src/runtime/mzm_intro.c`: the US intro (intro.c): the fade from black,
  the three typed texts with their letter sounds and blinking marker, the
  ship flying towards the camera as double-size affine sprites
  (ProcessComplexOam and CalculateOamPart4), Samus in her ship, the view of
  Zebes, Mother Brain's jar and the fuzz with its V-blank palette; A, B or
  START skips it.
- `mzm_sfx_fade_all` / `mzm_sound_fade_all` (FadeAllSounds).
- The runtime boots on the intro (no arguments or `--intro`; `--title`
  still starts on the title), and the file select's B and the game over's
  quit lead back to it, as GM_INTRO does. A skipped intro starts the title
  past its comets.
- Checked by capture (every stage, the end and the skip); the test in
  `tests/test_mzm_screens.c` runs the whole intro on the exported bank.

## Patch 0295 - Zero Mission game over

- `src/runtime/mzm_game_over.c`: the US game over screen
  (menus/game_over.c): the fade from white, the GAME OVER letters'
  breathing palette, the try again / title choice with Samus's head, the
  text palettes and the WIN1 window, the music, the fade to black.
- SamusDying flags the game over when its timer reaches 120
  (`MzmSamus.game_over`); the runtime opens the screen, then continues
  from the most recent save file (or the game's start without one) or goes
  back to the title screen.
- The runtime switches state through one path (a loaded file or a new
  game): the file select's continue and new game, and the game over's
  continue. A new game now restarts the starting state captured at boot.
- `--energy N` sets a diagnostic starting energy.
- Checked by capture in norfair_022; tests in `tests/test_mzm_screens.c`
  and `tests/test_mzm_samus.c`.

## Patch 0294 - Zero Mission file select copy and erase

- `src/runtime/mzm_file_select.c`: the copy and erase menus
  (FileSelectCopyFileHandler, FileSelectEraseFileHandler, their cursors,
  the copy arrow and the green destination markers, the overwrite and erase
  confirmations, the time attack refusal); the runtime copies and deletes
  the save files through callbacks.
- The panels' text buffers are cleared as the US binary does: the
  disassembly of mzm_us.elf adds the fill offsets in bytes, where the
  decompiled source's u32 pointers would multiply them by four (the copy
  panel's first message lost its second line before the fix).
- Checked by capture (copy panel with both messages and the arrow, erase
  panel); tests in `tests/test_mzm_screens.c` (copy A to B, erase B).

## Patch 0293 - Zero Mission file select screen

- `src/runtime/mzm_file_select.c`: the US file select screen
  (menus/file_select.c) on the native screen PPU, after START on the
  title: the three files (energy and tanks, timer, difficulty, area), the
  cursor and markers, the "Start game" panel with its native text, the
  continue / new game choice and overwrite confirmation of a completed
  file, the difficulty panel of a new file, the start animation, the fades
  and B back to the title. Copy, erase, the options and the time attack are
  not ported.
- The runtime keeps three save files (`--save-dir DIR`, replacing
  `--save-file`), the most recent one being selected; a continued file
  loads its room and state, a new game keeps its difficulty.
- The save format becomes version 2 (area, difficulty, in-game timer,
  completed flags); version 1 files are still read. The in-game timer runs
  each game frame (InGameTimerUpdate).
- Captures print the screen state (title, file select, pause).
- Checked by capture (files, panels, new game, continue); tests in
  `tests/test_mzm_screens.c` and `tests/test_mzm_save.c`.

## Patch 0292 - Zero Mission save platforms and save file

- `src/runtime/mzm_gadget_sprites.c`: the save platform and its tube,
  shadow, ray and top parts (save_platform.c). Samus standing on it opens
  it and is grabbed for the save prompt; YES plays the saving sequence (the
  palette row 2 flash, the extending top, the tube and the rising ray), then
  the save complete message and her release. The pause is disabled
  meanwhile (gDisablePause).
- `src/runtime/mzm_message_banner.c`: the save prompt's YES/NO cursor
  (SaveYesNoCursor) with its sounds; YES requests the save.
- `src/runtime/mzm_save.c`: the runtime's save file (a versioned
  little-endian format with a CRC-32): the room, Samus, the equipment, the
  events, the minimap progress, the collected items, the opened hatches and
  the never-reform blocks. `--save-file PATH` chooses it (default:
  `file_a.sav` in SDL's preference folder); `--load` starts from it, with
  the platform releasing Samus from its tube as after a file load.
- Exported: the save platform (with a baked palette row 2 variant) and the
  cursor's OAM table.
- Checked by capture in brinstar_033 (prompt, saving, message, release,
  reload); tests in `tests/test_mzm_sprites.c`, `tests/test_mzm_items.c`,
  `tests/test_mzm_save.c` and `tests/test_runtime_steep_slopes.c`.

## Patch 0291 - Zero Mission map stations and map download

- `src/runtime/mzm_gadget_sprites.c`: the map station and its two screens
  (map_station.c). Samus standing on the platform is grabbed, downloads,
  gets the area's map message and is released when the station retracts;
  a station whose area map is already downloaded stays retracted.
- The station sets gPauseScreenFlag to PAUSE_SCREEN_MAP_DOWNLOAD and the
  runtime opens the pause screen in its map download mode
  (`src/runtime/mzm_pause_screen.c`): the download BG layout, the area
  name, the line sweeping down and revealing the downloaded rows, then A,
  B or START to return. The HUD minimap shows the download afterwards.
- `MzmSpriteSamusLink` carries SamusSetPose requests and gSamusData writes
  from sprites (`mzm_samus_sprite_set_pose`); `MzmSpriteFrame` carries the
  banner's pose and the downloaded maps; the banner keeps its last pose
  once removed, as its native sprite slot does.
- The map station is exported with the ported sprites.
- Checked by capture in brinstar_032; tests in `tests/test_mzm_sprites.c`
  and `tests/test_mzm_screens.c`.

## Patch 0290 - Zero Mission easy sleep

- L on the pause map opens the native easy sleep screen
  (`src/runtime/mzm_pause_screen.c`, from pause_screen_sub_menus.c,
  pause_screen.c and text.c): BLDALPHA fades, the L prompt, the easy sleep
  tilemap, the text drawn into the equipment names graphics (backed up and
  restored), the YES/NO cursor with its sounds.
- YES sleeps after half a second: the screen blanks and the runtime shows
  nothing and pauses the audio device until SELECT, L and R are held
  together (the GBA's Stop syscall), then waits for the keys to be released.
- `src/runtime/mzm_text.c`: the mask mode character drawing
  (TextDrawMessageCharacter) used by the messages.
- Checked by capture in brinstar_000; tests in `tests/test_mzm_screens.c`.

## Patch 0289 - Zero Mission status screen

- R on the pause map opens the native status screen
  (`src/runtime/mzm_pause_screen.c`, from status_screen.c and
  pause_screen.c): the map fades out through BLDALPHA, the headers and
  Samus's wireframe slide in (the wireframe also drawn into the OBJ window,
  which shows only BG3 and the objects), the equipment rows are revealed one
  a frame, then the item cursor and the description text appear.
- The status tilemap is built from the runtime equipment: obtained and
  activated beams, suits and misc items (the plasma beam, gravity suit and
  space jump as unknown items before the fully powered suit), bombs and
  missiles with their ammunition, the energy and ammunition amounts, the
  pistol when suitless.
- The D-pad moves the cursor between the enabled slots with the native
  left/right memory; B, L or R goes back to the map.
- `src/runtime/mzm_text.c`: the native text engine for the descriptions
  (character widths, append mode drawing, message processing with delays,
  new lines and the A key).
- `MzmPauseGame` now carries the equipment instead of a suitless flag.
- Not ported: the item acquisition and suit change sequences with the item
  toggling (disabled in the normal pause), the debug status screen, the
  mask mode message drawing.
- Checked by capture in brinstar_000; tests in `tests/test_mzm_screens.c`.

## Patch 0288 - Zero Mission pause screen map

- `src/runtime/mzm_pause_screen.c`: START in the game opens the native
  pause screen's map (PauseScreenInit, fades, the area map with Samus's and
  the boss icons, scroll arrows, area name, minimap palette animation,
  D-pad scrolling with repeat, world map on A/B, area switching on SELECT,
  START/B to leave).
- `mzm_hud_area_map` gives the pause screen each area's map (obtained
  items, boss icons, the skipped Varia tile, downloaded tiles).
- `DecreaseMusicVolume` / `ResetMusicVolume` (unk_3058) lower the music
  during the pause; `ReplayQueuedMusic` keeps the lowered volume.
- Captures accept START as the 0x200 bit of `--input-script`.
- Not ported yet: status screen, easy sleep, Chozo hints, map download,
  completion info.
- Tests: `tests/test_mzm_screens.c`.

## Patch 0287 - Zero Mission audio pulled by the device

- The crackling came from the stream queue running dry: it was fed from
  the game steps and consumed by the device in chunks (measured with
  `MZM_AUDIO_DEBUG`: 0 to ~1400 queued samples, several empty frames a
  second). The device now pulls the sound through SDL's stream callback:
  each pull runs UpdateAudio and renders one GBA frame at a time; the game
  only calls the engine under the stream's lock.
- The mix is rendered at the 65.536 kHz PWM rate MZM selects in SOUNDBIAS
  and resampled by SDL; measured offline, the aliasing was negligible
  either way (about 0.07% of the energy above 16 kHz).
- The output-rate steering and its test are gone.

## Patch 0286 - Zero Mission elevators and area connections

- `scripts/mzm_runtime_room.py` resolves the area connection doors
  (`sAreaConnections`) to the connected area's room and door; the runtime
  now takes them (elevators, and passages such as brinstar_000's shaft up
  to Crateria).
- Elevator blocks work again where `ClipdataCheckCantUseElevator` allows
  them (`sElevatorRoomPairs` from the ROM bank, the escape condition);
  `BgClipCheckTouchingTransitionOnElevator` takes the ride's transition
  without offset and RoomLoad moves Samus three blocks on.
- Checked by capture: brinstar_026 down to norfair_000, landing on the
  elevator pad with the Norfair minimap.
- Tests: `tests/test_runtime_steep_slopes.c` (elevator door, placement).

## Patch 0285 - Zero Mission room gadgets

- `src/runtime/mzm_gadget_sprites.c`: elevator pads, water drops (common
  sprite graphics), steam vents (ground, ceiling, walls, diagonals),
  Gerons (Brinstar, Varia and Norfair plugs that block their passage) and
  the unknown item blocks (Plasma Beam, Gravity Suit, Space Jump) with
  their light.
- Exported by `scripts/mzm_sprites.py` (new `COMMON_SPRITES` for sprites
  drawn with the common graphics); the sprite frame now carries
  `samus_using_elevator`.
- Checked by capture in crateria_001, brinstar_021, kraid_012 and
  crateria_011.
- Not ported yet: the elevator ride, Parasites, map/save stations, area
  banners.
- Tests: `tests/test_mzm_sprites.c`.

## Patch 0284 - Zero Mission native room backgrounds: animations and liquids

- `src/runtime/mzm_room_gfx.c` renders the room backgrounds from the ROM
  bank on a GbaPpu as the game loads them: VRAM (animated, common and
  tileset tiles, BG3 graphics), BG palettes, BG3 and LZ77 BG0 maps, the BG
  control registers and priorities of the room's transparency, and the
  block layers' metatiles around the camera.
- Animated tiles (all five types), tank tiles, the animated palette (with
  the alarm and zipline conditions, paused during power bombs), the hatch
  flashing palette, and the liquid BG0 with its water wave now run every
  frame: lava, acid and water are visible and animated, and
  gEffectYPosition (wave included) feeds hazards and sprites.
- The runtime draws each native layer in place of the static sheets
  (which remain the fallback without the ROM bank), keeping its sprite
  interleaving and approximate BG0 blending.
- Checked by capture in norfair_022, crateria_007 and brinstar_000.
- Not ported yet: background effects, BG movement, haze, gradients,
  windows and fades; mother ship doors.
- Tests: `tests/test_mzm_room_gfx.c`.

## Patch 0283 - Zero Mission title screen

- `src/runtime/mzm_screen.c`: what the non-gameplay screens share
  (gOamData, the *_NonGameplay offsets and blending, the EWRAM work area,
  GBA-bit input) and `ProcessMenuOam`, `ResetFreeOam`,
  `UpdateMenuOamDataId`, `ClearGfxRam`, `ApplySpecialBackgroundFadingColor`
  over the ROM's OamArray/FrameData tables.
- `src/runtime/mzm_title_screen.c` ports the US title screen: init from the
  ROM bank, fade in, title music (with `UpdateMusicPriority`, now carried by
  mzm_sfx), comets and ground scrolling, START skipping them, the title
  alpha fade, animated palettes, sparkles, PRESS START, fade out.
- The runtime boots on it without arguments or with `--title` (Return or
  the gamepad's Start is START); START starts the game in brinstar_000.
  The idle demos re-enter the title past its comets instead of playing.
- Tests: `tests/test_mzm_screens.c` (fades, menu OAM pre-actions and
  positions, the title screen on the exported bank); the HUD test now runs
  its exported-bank checks in CTest too.

## Patch 0282 - Zero Mission HUD and minimap on a GBA PPU

- Foundation for the native screens: `src/runtime/gba_ppu.c` renders the
  GBA display memories (text and affine backgrounds, regular and affine
  objects with 1D/2D mapping, windows 0/1 and the object window, alpha
  blending, brightness, mosaic, forced blank; not the bitmap modes nor
  mid-frame register effects). `scripts/mzm_rom_bank.py` (rebuild task
  `metroid_rom_bank`) copies the verified ROM into a private bank with the
  14462 sized data symbols of `third_party/mzm/mzm_us.elf`; `mzm_rom.c` reads
  them by name and decompresses BIOS LZ77 and run-length streams.
- `src/runtime/mzm_hud.c` ports `hud.c`, `hud_generic.c` and `minimap.c`:
  energy digits and tanks with the low health flag, the missile, super
  missile and power bomb counters with their Inactive/Selected/Active
  symbols and refill stages, the suitless charge bar, the HUD OAM, and the
  minimap (LZ77 area map, obtained items, downloaded tiles, explored
  screens, area names, one 3-tile line a frame through the native color
  tables and flips). Room map positions come from `sAreaRoomEntryPointers`.
- The runtime draws the HUD over its 320x224 view (minimap at the right
  edge), runs `MinimapCheckOnTransition` on each new room, refuses tanks on
  unexplored minimap tiles (`MinimapCheckIsTileExplored`) and records
  tanks and abilities on the minimap (`MinimapUpdateForCollectedItem`,
  `BgClipFinishCollectingAbility` at the end of an item message's text).
- Checked in brinstar_000 (`--spawn 212 496`) by capture.
- Not ported yet: the pause screen, title, file select, cutscenes; saving
  the minimap progress; boss icon updates.
- Tests: `tests/test_mzm_hud.c` (PPU layers, priority, blending, windows,
  flips, wrap; LZ77/RL; the HUD on a synthetic ROM; the exported bank),
  `tests/test_mzm_rom_bank.py`, `tests/test_runtime_steep_slopes.c`.

## Patch 0281 - Zero Mission missile origin, audio output, Chozo hint music

- Shots take the arm cannon offsets of Samus's unarmed library key: the
  armed graphics variants have no offsets, so missiles fired standing left
  from her feet (brinstar_033: the missile now starts at the cannon).
- Audio output: the runtime renders one GBA frame (59.73 Hz, about 803.6
  samples) per 60 Hz step, so the stream queue kept growing until whole
  frames were dropped, which crackled. `runtime_audio_rate` now steers the
  output rate by at most 1% to hold about four frames in the queue; only a
  half-second stall still skips a frame. The mixer's measured peak (about
  11 500) does not clip.
- Hint statues: `ChozoStatueRegisterHint` fades the music for a second,
  `ChozoStatueHintFlashing` plays `MUSIC_CHOZO_STATUE_HINT` when the statue
  sits down and `ChozoStatueSleeping` replays the room track
  (`gMusicTrackInfo.currentRoomTrack`, carried by `MzmSpriteWorld`);
  `mzm_sfx` gains the `FadeMusic` request. The item jingle path was checked
  with the extracted bank: player 0 plays the jingle, then the room track
  resumes.
- Not ported: the hint map screen (the hint music plays over the statue).
- Tests: `tests/test_mzm_sprites.c` (fade, hint music, room track replay),
  `tests/test_runtime_steep_slopes.c` (the rate steering settles between
  four and eight queued frames).

## Patch 0280 - Zero Mission Dragons

- Dragons (`dragon.c`) rise from their lava toward Samus, turn and spit
  fireballs that arc with the ROM tables (`sDragonFireballYMovement`,
  `sDragonFireballOamRotation`, now exported) and explode on the surface
  or a block; the secondary sprites now take their health from
  `sSecondarySpriteStats`.
- Checked in norfair_022 (`--spawn 260 300`).
- Not ported: the splashes, the fireball's background priority.
- Tests: `tests/test_mzm_sprites.c`.

## Patch 0279 - Zero Mission liquids and hazards, Skulteras

- The runtime computes `ClipdataUpdateCurrentAffecting`'s hazard: the
  water, lava and acid blocks, else the room effect's hazard above or below
  its surface (`sHazardsDefinitions`, the damage effect of the exported
  `sHazeData`, only with a BG0); the room's `RoomEffect` is read from its
  `L` line.
- Samus: `SamusUpdatePhysics` slows her in liquids (halved speeds and
  gravity, no Gravity Suit) and `SamusTakeHazardDamage` takes energy at the
  native per-suit rates (the submerged sound, the cold knockback, death).
- Sprites see the hazard (`MzmSpriteWorld.hazard`); Skulteras (`skultera.c`)
  patrol, turn, chase Samus and rise only in water.
- Checked in crateria_007 (Samus at the slowed cap in the water, the
  Skulteras patrolling and hurting her).
- Not ported: the surface wave (`gWaterMovement`), the liquid enter and
  exit effects and sounds.
- Tests: `tests/test_mzm_samus.c` (water slowing, Gravity Suit, lava damage
  rates, death), `tests/test_mzm_sprites.c` (Skultera),
  `tests/test_runtime_steep_slopes.c` (the hazard rules).

## Patch 0278 - Zero Mission Morph Ball, Power Grip and Charge Beam pickups

- `src/runtime/mzm_item_sprites.c` ports the three item sprites outside the
  Chozo statues with their secondaries: the Morph Ball and its outline, the
  Power Grip and its glow, the Charge Beam with its spawning table, glow and
  float (`sChargeBeamIdleYMovement`, now exported).
- Touched, each gives its item and event, holds Samus with the item banner
  and flickers until the banner releases her; an owned item removes it.
- The runtime's `--without morph-ball|bombs|power-grip` removes a starting
  item so its pickup can be collected.
- Dashboard: the item sprites count as ported like the monsters (all
  Zero Mission items now have a ported pickup).
- Checked in brinstar_000 (the Morph Ball on its pillar, its banner) and
  crateria_012 (the Power Grip); the Charge Beam's event sprite set is not
  resolved by the export, so it is covered by the unit tests only.
- Tests: `tests/test_mzm_sprites.c`, `tests/test_dashboard.py`.

## Patch 0277 - Zero Mission native Samus controller

- `src/runtime/mzm_samus.c` is now a transliteration of `src/samus.c`: the
  67 native poses with `sSamusCollisionData`, every pose handler and
  graphics loop, `SamusSetPose` and its carries, the physics, the velocity
  update (`DIV_SHIFT` shifts, slope speeds) and the native point-probe
  collisions of `SamusUpdateHitboxMovingDirection` instead of the box sweep.
- Reported bugs, fixed the native way: corners (the side, top and landing
  probes with their nudges), the ledge climb with Up or the diagonal toward
  the ledge without A (`SamusAimingWhileHanging`), the shots while hanging
  (the hanging aim poses and their arm cannon offsets, no more shots from the
  feet), and the running diagonals (`SamusAimCannon`).
- Also ported: slopes, walking off ledges, the morph ball tunnel pull, the
  knockback pose, the suitless crouching and crawling, the elevator, save,
  zipline and background poses (not yet entered), the ground effect sounds.
- The runtime draws the body animation `SamusUpdateGraphicsOam` selects (its
  library key) and takes the arm cannon offset from it; the Samus export now
  includes the Running `[ACD_NONE - 2]` rows; the screw and speed booster
  block damage runs inside the collisions.
- Checked in brinstar_009 (hang, climb without A, shot up), brinstar_005 (a
  diagonal shot while running left) and a 900-frame random input run in five
  rooms.
- Not ported: liquids and hazards, the elevator pads and save platforms that
  enter their poses, the sprite knockback requests, the suitless pistol.
- Tests: `tests/test_mzm_samus.c` (new: running aim, climb without A,
  hanging shots and arm cannon hiding, morph ball tunnel, corners, wall stop,
  walk-off ledge, steep slope speed, suitless crouch and jump, knockback, the
  pose table; the dying drift), `tests/test_runtime_steep_slopes.c`.

## Patch 0276 - Zero Mission Gamets

- Blue and red Gamets and the blue Gamet leader with its followers run the
  pipe bug AI (`gamet.c` matches `zeb.c` and `zebbo.c`): their own hitbox
  and rising, moving and damaged sounds; the leader spawns two followers
  1.5 and 2.5 blocks below it and waits for both.
- All four sprites are exported and counted as ported monsters.
- Checked in norfair_001 (`--spawn 1300 140`): a Gamet leaves its pipe,
  stops near Samus's height and flies toward her at 3 pixels a frame.
- Not ported: the background priority changes (as for the other pipe bugs).
- Tests: `tests/test_mzm_sprites.c` (hitbox, rising sound, followers,
  speed, the leader waiting for its followers).

## Patch 0275 - Zero Mission Squeepts and the room effect height

- Rooms export their entry's `effectY` (`E` line); the runtime gives
  sprites `gEffectYPosition` from it (`MzmSpriteWorld.effect_y`), without
  the `gWaterMovement` wave.
- `SpriteUtilCheckOutOfRoomEffect` / `SpriteUtilCheckInRoomEffect` are
  ported as surface-crossing checks (their splash particles are not).
- Squeepts (`squeept.c`) hide in their lava until Samus is above them,
  jump with `sSqueeptRisingMovement`, turn at the table's end or at a
  ceiling, fall with `sSqueeptFallingMovement` and hide again, playing their
  leaving and entering lava sounds at the surface.
- Checked in norfair_010 (`--spawn 430 150`): the right Squeept leaves the
  lava, rises to y 64 and turns.
- Tests: `tests/test_mzm_sprites.c` (wait, detection, rise, turn, fall,
  hiding, both lava sounds, the early ceiling turn).

## Patch 0274 - Zero Mission Gerutas and Ripper IIs

- Red and green Gerutas (`geruta.c`) reuse the Holtz's movement probes:
  they wait under a ceiling until Samus is low and within 5 blocks (or
  attack at once without one), warn, launch, dive at 3 pixels a frame
  drifting toward her, bounce off the floor and settle back 52 subpixels
  under the ceiling, with their sounds.
- Ripper IIs (`ripper2.c`) patrol at two pixels a frame with their own
  tables, hitbox, wall probe and turning sound.
- Both are exported (`PORTED`) and counted as ported monsters.
- Checked in norfair_012 (Gerutas: dive, bounce and settle at y 77) and
  norfair_017 (`--spawn 420 80`: six Ripper IIs crossing and turning).
- Not ported: the liquid splash of `SpriteUtilCheckInRoomEffect`.
- Tests: `tests/test_mzm_sprites.c` (Geruta detection height, dive speeds,
  bounce, settle height, the ceiling-free attack; Ripper II speed, wall
  turn and reverse).

## Patch 0273 - Zero Mission item jingles

- The sound engine ports `InsertMusicAndQueueCurrent`, `ReplayQueuedMusic`,
  `DelayMusicStart`, `unk_34ac`, `unk_35d0`, `unk_2a8c`,
  `BackupTrackData2SoundChannels` and `RetrieveTrackData2SoundChannels`: an
  inserted music saves the channels of players 1, 3, 6 and 8, stops the
  others, and its `0xB6` end restores them and restarts the current music.
- `mzm_sfx` carries `PlayMusic`, the insertion and the player 2 backup
  requests; the runtime forwards them to the engine.
- The message banner follows the native `MessageBannerPopUp`: items and
  first tanks back up player 2 and insert the item (or unknown item)
  jingle, the fully powered suit plays the Brinstar remix behind its
  jingle, tank messages back up player 2 and retrieve it on removal.
- Checked with the extracted sound bank (engine only, captures are
  silent): the item jingle replaces Brinstar for 355 frames, then Brinstar
  restarts.
- Not ported: the volume down of `gUnk_Audio0x50` (`unk_3058`), the item
  acquisition screen, the save prompt.
- Tests: `tests/test_mzm_audio.c` (insertion and replay against a jingle
  without a queued music, player 2 backup and retrieve),
  `tests/test_mzm_items.c` (the banner's music requests per message kind).

## Patch 0272 - Zero Mission unknown item Chozo statues

- The Plasma Beam, Space Jump and Gravity Suit statues run the Chozo statue
  AI with their own tables: `UnknownItemChozoStatue*` OAM tables and unknown
  balls (anim indices offset by `MZM_CHOZO_UNKNOWN_BASE`), the
  `sUnknownItemChozoStatueMultiSpriteData_*` tables (the multi-sprite parser
  now reads positional initializers), and their part and refill sprites.
- Their item is obtained but inactive (`MzmEquipment.unknown_items`, as
  `UpdateSuitType` leaves it for the normal suit) with its obtained event
  and "Unknown Item" banner; the statue then refills like the others.
- Dashboard: the three unknown items count as ported statue items.
- Checked in kraid_033 (`--spawn 186 128`, B, Right): the Space Jump
  statue's ball reveals the unknown orb; touching it shows "Unknown Item".
- Not ported: activating the unknown items (the fully powered suit), the
  hint map screen.
- Tests: `tests/test_mzm_sprites.c` (the Plasma Beam statue's tables, parts,
  graphics owner, ball and inactive item), `tests/test_dashboard.py`.
- Validation on 2026-10-12: warning-clean build, CTest 31/31, the Python
  suite (430 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0271 - Zero Mission Chozo statue refills and hints

- `mzm_samus.c` adds `SPOSE_GRABBED_BY_CHOZO_STATUE` (SamusInactivity, the
  motionless Morph Ball animation from the Samus pipeline, no weapons) and
  `mzm_samus_chozo_grab`; the sprites see and move Samus through
  `MzmSpriteSamusLink`.
- `mzm_chozo_statue.c` ports the hint grab (Morph Ball or rolling at the
  hand's exact height), `ChozoStatueRegisterHint`, the eye opening, the
  hint flashing (`sChozoStatueFlashingPaletteRows`, the hint sound), sitting
  down along the Sitting table with `SpawnChozoStatueMovement` echoes, the
  seated delay, the refill grab, `ChozoStatuePartArmRefill` with
  `SpriteUtilRefill*` and the HUD counters, the refill glow sprite, the
  refill messages, sleeping, the eye closing and the release.
- The exporter renders the statues with their palette rows 1 to 3 (the
  runtime picks them from the palette row).
- Checked in brinstar_005 (`--spawn 126 112 --item long-beam`, Down twice):
  the statue grabs the Morph Ball, flashes gold during the refill, opens
  "Weapons and energy fully restored", sleeps and releases Samus. In
  brinstar_002 (`--spawn 808 112`, Down twice): the Long Beam hint statue
  grabs Samus, flashes, sits down with her and refills.
- Not ported: the hint map screen, its flashes and music, the unknown item
  statues.
- Tests: `tests/test_mzm_sprites.c` (the refill grab and its height check,
  the energy refill steps, the message, sleeping and release; the hint grab,
  eye, flashing, sitting and seating).
- Validation on 2026-10-12: warning-clean build, CTest 31/31, the Python
  suite (430 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0270 - Zero Mission item Chozo statues

- `src/runtime/mzm_chozo_statue.c` ports `ChozoStatue`, `ChozoStatuePart` and
  `ChozoBall` for the eight item statues (Long, Ice and Wave Beams, Bombs,
  Speed Booster, High Jump, Screw Attack, Varia Suit) and their hint
  statues: `gSubSpriteData1` with the Standing, Sitting and Seated
  multi-sprite tables, the direction, the seated or standing clipdata
  (`CAA_MAKE_NON_POWER_GRIP`, `CAA_MAKE_SOLID_GRIPPABLE`), the eye, arm, leg
  and glow parts, and the ball: shot, it reveals the item; touched, it stops
  Samus, registers the item and its events and opens the item's banner,
  flickering until the banner closes; the statue then idles and its arm
  glows.
- The sprite engine: `drawOrder` (painted from 16 to 1), `SP_ALWAYS_ACTIVE`
  under `gPreventMovementTimer`, the graphics owner of secondary sprites,
  `SSC_SOLID` and `SSC_SOLID_CHECK_COLLIDING` with the solid-sprite rules,
  hooks for clipdata-affecting actions and message banners, and
  `SpriteSpawnSecondary` returning its slot.
- `mzm_blocks.c` ports `BlockUpdateMakeSolidBlocks` and the three
  make-solid actions (their values exported as `makesolid`).
- Exporters: the statues' graphics (unflipped and X-flipped only) and the
  decompilation's `MultiSpriteData` tables (`multi` lines, parsed by
  `mzm_c_tables.multi_sprite_data`); the runtime library holds up to 16384
  entries.
- Dashboard: the ability items of the Chozo statues and the four tank kinds
  join the items list (statue items done when their statue has an AI).
- Checked in brinstar_005: the seated Long Beam statue holds its ball;
  shooting it reveals the orb, touching it opens the "Long Beam" banner, and
  after A the ball is gone, the hand glows and the next shot is a Long
  Beam (projectile type LONG_BEAM).
- Not ported: the hint and refill grabs (Samus's grabbed pose, the hint map,
  the sitting animation, the refill), the unknown item statues, the
  flashing palette rows.
- Tests: `tests/test_mzm_sprites.c` (statue spawn, clipdata, multi-sprite
  positions, the ball's reveal and collection, the statue's wait, the glow,
  the refill and hint statues), `tests/test_mzm_blocks.c` (make-solid
  blocks), `tests/test_dashboard.py` (the new items).
- Validation on 2026-10-12: warning-clean build, CTest 31/31, the Python
  suite (430 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0269 - Zero Mission tank pickups and the message banner

- `src/runtime/mzm_items.c` ports the tank part of
  `BgClipCheckTouchingTransitionOrTank` (odd frames, the four Clipdata
  probes, last first): a revealed tank starts `gCollectingTank`, raises the
  equipment within the game total (100 energy refilled, 5 missiles, 2 super
  missiles, 2 power bombs on normal difficulty; the first missile, super
  missile or power bomb tank with its own message) and opens the message
  banner; `BgClipFinishCollectingTank` empties the block (air, or water for
  underwater tanks), `BgClipSetItemAsCollected` records it and
  `BgClipRemoveCollectedTanks` clears it on every room load.
  `scripts/mzm_c_tables.py` exports `sTankBehaviors`' item types, water
  flags and messages and the TankList rows into `blocks.tsv`.
- `src/runtime/mzm_message_banner.c` runs `MessageBanner` (graphics frames,
  text processing with the animation held, the one- or two-line spawn, 100
  static frames, A/B/directions to close, the removal) with the tank jingle;
  `gPreventMovementTimer` skips `SamusUpdate` and stops the sprites
  (`SpriteUtilCheckStopSpritesPose`: no Samus contact, only uninitialized
  sprites run).
- `scripts/mzm_message_text.py` transliterates `TextProcessMessageBanner`,
  `TextProcessCurrentMessage` and `TextDrawMessageCharacter`; the sprite
  exporter renders the banner's six OAM tables with each English message and
  records their text calls.
- Checked in brinstar_019 (`--spawn 615 96`, Right, then A): Samus walks
  onto the energy tank, energy rises from 99 to 199, the banner reads
  "Energy Tank acquired. / Energy capacity increased." while Samus stays
  frozen, and after A the banner closes and the tank block is empty.
- Not ported: the item jingles and the music track backup, the minimap and
  HUD, the item acquisition screen, the ability items.
- Tests: `tests/test_mzm_items.c` (tank collection, first tanks, the game
  total, cleared blocks, the collected registry, the banner's timeline and
  events), `tests/test_runtime_steep_slopes.c` (the probes, the finished
  collection and the removal on reload).
- Validation on 2026-10-12: warning-clean build, CTest 31/31, the Python
  suite (430 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0268 - Zero Mission tumbling missiles

- `mzm_sprites.c`: `ProjectileMissileHitSprite` and
  `ProjectileSuperMissileHitSprite` now start `ProjectileStartTumblingMissile`
  when the sprite is immune or not weak to them (the invincible particle and
  the solid hit stay): the missile turns away from the sprite's x, stops its
  thrust sound, loses `PROJ_STATUS_CAN_AFFECT_ENVIRONMENT` (no more sprite
  hits) and draws in front of the backgrounds.
- `mzm_projectiles.c`: the tumbling stage (`ProjectileMoveTumbling`: along
  `sTumblingMissileSpeed`, its last speed kept, a pixel a frame sideways,
  through walls, removed once `ProjectileCheckDespawn` found it off
  screen); `ProjectileCheckDespawn` now records `PROJ_STATUS_ON_SCREEN`.
- Runtime: the `Tumbling` OAM tables of missiles and super missiles.
- Checked in brinstar_003 (L and R held, B): a missile fired diagonally at a
  brown Ripper bounces back and falls tumbling.
- Tests: `tests/test_mzm_samus.c` (the tumbling speeds, sideways drift,
  no wall collision, removal off screen) and `tests/test_mzm_sprites.c`
  (the Ripper's tumbling missile, its thrust stop and no second hit).
- Validation on 2026-10-12: warning-clean build, CTest 30/30, the Python
  suite (430 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0267 - Zero Mission Wave and Plasma Beams, beam and missile trails

- `mzm_projectiles.c`: `mzm_beam_type` follows `SamusCheckNewProjectile`
  (plasma, then wave, ice, long) with all 24 normal and charged shot
  sounds; the Wave and Plasma charge sounds. `ProjectileProcessWaveBeam` /
  `ChargedWaveBeam` move 28 subpixels a frame through walls with
  `ProjectileCheckWaveBeamHittingBlocks` (CAA_BEAM on three points across
  the wave); `ProjectileProcessPlasmaBeam` / `ChargedPlasmaBeam` move 32 and
  stop at walls (the Plasma impact) unless the Wave Beam is equipped.
  `mzm_projectile_hitbox` gives every routine's hitbox (the per-direction
  wave boxes with their truncated float factors).
- Trails: `ProjectileSetTrail` takes the effect; the ice trail of every beam
  with the Ice Beam, the charged Long, Ice, Wave, Plasma and full beam
  trails (every 8 frames), the missile (8) and super missile (4) trails.
- `ProjectileCheckDespawn` now tests the screen (plus the projectile's draw
  distance and padding) before the distance to Samus, with the runtime
  camera's corner and the GBA screen size.
- `mzm_sprites.c`: the Wave and Plasma cases of `ProjectileCheckHittingSprite`
  (3 plus one per other beam, charged 12 plus four; ice hits with the full
  beam particles when the Ice Beam is equipped); the Plasma Beam goes on
  through the sprites it hits, immune ones included.
- `mzm_effects.c`: the Wave, Plasma and full beam impacts (their sounds) and
  the trails' tables.
- Exporters: the projectile library adds the Wave and Plasma impacts, the
  missile and charged trails for each beam variant (the Plasma trails with
  the Plasma graphics only) and ice-coloured `/ice` versions of the Wave and
  Plasma projectiles; the sprite library adds the same tables for their
  durations. The Samus pipeline names the first unresolved particle's error.
- Runtime: Wave and Plasma Beam debug items (`--item wave-beam`,
  `--item plasma-beam`), their projectile keys (`_Wave`/`_NoWave` by the
  Wave Beam at the shot, `/ice`), beam variants, release and charging rows,
  pseudo screw damage; `PROJ_STATUS_ABOVE_BG1` projectiles draw after the
  front backgrounds; captures list the active projectiles.
- Checked in brinstar_003: `--item wave-beam --item long-beam` sends a wave
  through the right wall, drawn in front of it until it leaves the screen;
  `--item plasma-beam` fires the green Plasma Beam through a Ripper; both
  flash their release rows.
- Tests: `tests/test_mzm_samus.c` (beam types and sounds, hitboxes, the Wave
  Beam through a wall with its block action, the Plasma Beam's speed and
  wall impact, the charged wave trail), `tests/test_mzm_sprites.c` (Wave,
  Wave + Ice, Plasma pass-through, fully charged hits, the impact sounds)
  and `tests/test_mzm_projectile_compose.py` (the exported particle lists).
- Validation on 2026-10-12: warning-clean build, CTest 30/30, the Python
  suite (430 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0266 - Zero Mission frozen sprites as platforms

- `mzm_sprites.c` ports the frozen branch of
  `SpriteUtilSamusAndSpriteCollision`: unless Samus pulls herself up or the
  contact kills the sprite, a falling Samus whose feet are within 6 pixels
  of its top lands on it (y = top + 1, `SAMUS_ON_TOP`, standing on it),
  rising into its bottom puts her under it and zeroes the upward velocity,
  and walking into its sides stops her
  (`SpriteUtilCheckStopSamusAgainstSolidSpriteLeft` / `Right`).
  `SpriteUtilSamusStandingOnSprite` runs after each AI: the landing request
  and `STANDING_ENEMY`, then the two-frame release (Samus kept on the top
  when she did not rise since her last pose change) to `STANDING_MIDAIR`.
  Kills of a sprite she stands on drop her (`mzm_sprites_samus_dropped`).
- `mzm_samus.c` keeps `STANDING_ENEMY` (`on_enemy`): no floor check in the
  ground poses and no block landing while it holds; `gSamusDataCopy`'s y is
  kept at each pose change; `mzm_samus_land_on_sprite` is the landing
  request. A room load clears it.
- The runtime passes the sprites gSamusData's y (the port's y - 1), the
  vertical velocity, the facing, the pull-up and morphed states and the
  room, applies the pushes, and prints the freeze timers and the standing
  state in captures.
- Checked in ridley_003 (`--spawn 600 144 --item ice-beam --item
  charge-beam`): a charged Ice Beam freezes a Dessgeega, Samus jumps onto
  it and stands on it (y 122 over the floor at 144) for 400 frames; once
  it thaws she takes its contact damage and is knocked back. In
  brinstar_003, a frozen Ripper stops a spin jump at its side and bumps
  Samus's head from below.
- Not ported: ziplines (`SpriteUtilCheckOnZipline`), the non-frozen solid
  sprite collisions (no ported sprite uses them).
- Tests: `tests/test_mzm_sprites.c` (landing on top, not while rising, the
  standing request, the release, the head bump, the side stop, pulling up,
  the drop on a kill) and `tests/test_mzm_samus.c` (the landing request and
  ground poses over air while standing on a sprite).
- Validation on 2026-10-12: warning-clean build, CTest 30/30, the Python
  suite (429 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0265 - Zero Mission Ice Beam and freezing

- `mzm_projectiles.c` ports `ProjectileProcessIceBeam` and its charged twin:
  the ice types win over the Long Beam, move 26 subpixels a frame, keep the
  Long Beam lifetime rule and drop `ProjectileSetTrail` particles half a
  block from the shot when `gFrameCounter8Bit & 3` is 0; the ice shot and
  charge sounds are used.
- `mzm_sprites.c` ports `ProjectileIceBeamHittingSprite`,
  `ProjectileChargedIceBeamHittingSprite`, `ProjectileIceBeamDealDamage` and
  `ProjectileFreezeSprite` (3/4 and 12/16 damage, the freeze flag from the
  high byte of the weakness column, 240 ticks, the frozen row,
  `SOUND_FREEZING_SPRITE`, the freezing particles), `SpriteUtilUpdateFreezeTimer`
  in every ported AI (and the Space Pirates' suited branch), and
  `SpriteUtilUpdateStunTimer`'s stun palette flash. Frozen sprites take
  contact damage and do not hurt Samus; standing on them is not ported yet.
- `mzm_effects.c` adds the ice impact (with its sound), the freezing
  particles (drawn every other frame) and the beam trails (drifting one
  subpixel sideways and one pixel down a frame).
- The sprite exporter bakes `/frozen` and `/stun` variants of every sprite
  frame; the runtime picks them from the palette row, draws the ice
  projectiles and particles from the IceBeam variant, uses the ice release
  (row 2) and charging rows (4-5), counts the Ice Beam in the pseudo screw
  damage, and lists Ice Beam in the debug menu (`--item ice-beam`).
- Checked in brinstar_003 (`--item ice-beam`, L held, B taps): a brown
  Ripper (immune to beams, freezable) freezes blue under the ice impact and
  freezing particles, trails follow the shots and Samus flashes the ice
  release row; the Ripper stays frozen in place.
- Not ported: frozen sprites as platforms, the charged ice trail, the Wave
  and Plasma Beams, the pistol.
- Tests: `tests/test_mzm_sprites.c` (ice hits with and without the freeze
  flag, Long Beam damage, the freeze timer and flicker, the charged kill,
  freezable-but-immune and immune sprites, the stun flash, the freezing and
  trailing particles), `tests/test_mzm_samus.c` (the ice type, sound, speed
  and trail) and `tests/test_mzm_sprites.py` (the frozen and stun rows).
- Validation on 2026-10-12: warning-clean build, CTest 30/30, the Python
  suite (429 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0264 - Zero Mission Charge Beam and Long Beam

- `mzm_projectiles.c` ports `SamusCheckFireBeamMissile` with the charge
  (to 79 then 64, a charged shot from 64, a beam above 25, dropped by arming
  a weapon) and the charge branches of `SamusCheckNewProjectile` (the Morph
  Ball releases a full charge, dying drops it), the beam type of the equipped
  beams, the Long Beam (24 subpixels a frame, no short lifetime), the charged
  normal and Long Beams (two at a time), their sounds, the beam release
  palette timer and the `ProjectileSetBeamParticleEffect` flash, and
  `ParticleChargingBeam`'s stages and sounds.
- `mzm_sprites.c` ports the Long Beam and `ProjectileNonIceChargedHitSprite`
  hits with their hitboxes, and the pseudo screw contact
  (`DCT_PSEUDO_SCREW`: the charge spent, 4 * (2 + beams), the
  `PSEUDO_SCREW_DESTROYED` death with its particle and sound).
- The projectile library now exports the beam particles (charging, shooting
  flashes, trails, the Long Beam impact) for each beam variant
  `ProjectileLoadGraphics` can load (graphics set and `sBeamPal` row); the
  runtime draws the charging particle at the arm cannon and the shot flash,
  prefers those variants for beam particles, and uses the charging and beam
  release palette rows. Charge Beam and Long Beam are debug menu items.
- Checked in tourian_010 (`--item charge-beam --item long-beam`, B held 80
  frames): the charge glow and palette, then the charged Long Beam and its
  flash.
- Not ported: the Ice, Wave and Plasma Beams, freezing, the charged trails,
  the pistol.
- Tests: `tests/test_mzm_samus.c` (no charge without the item, the
  thresholds, the particle stages and sounds, the charged and partial
  releases, arming, the Long Beam speed and lifetime, the Morph Ball
  release), `tests/test_mzm_sprites.c` (charged and Long Beam damage, the
  pseudo screw) and `tests/test_runtime_steep_slopes.c` (the palettes).
- Validation on 2026-10-12: warning-clean build, CTest 30/30, the Python
  suite (428 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0263 - Zero Mission power bombs

- `src/runtime/mzm_power_bomb.c` ports `gCurrentPowerBomb`:
  `PowerBombExplosionStart`/`Begin` (the explosion sound), the power bomb
  cases of `HazeProcess` (`Haze_PowerBombExpanding`/`Retracting`: the axis by
  3 a frame to 159, five frames held, back to 4, the stage not reset), and
  `PowerBombExplosionProcess` (the hitbox of axis * 4 / 8 * .95 clamped to
  the room, the `CAA_POWER_BOMB` sweep of its edge columns or rows from the
  centre outward on alternate frames while expanding, the end).
- `mzm_projectiles.c` ports the power bomb branch of
  `SamusSetHighlightedWeapon` (R in the Morph Ball poses; the shinespark
  poses now arm nothing), the power bomb fire (the Bombs item required), the
  spawn (one at a time, none during an explosion, 5 frames of cooldown) and
  `ProjectileProcessPowerBomb` (the ammunition, the last disarming them, 54
  slow and 40 fast frames, the set sound). `mzm_sprites.c` ports
  `ProjectilePowerBombDealDamage` (50, once per power bomb through
  `SPRITE_ISFT_POWER_BOMB_STUNNED`, cleared when one is set), the power bomb
  drops and their pickup.
- The runtime runs the explosion after the hatches and blocks (`RoomUpdate`
  order), draws the power bomb sprites and an approximate look (a white
  backdrop, `PowerBombYellowTint` as an additive tint while expanding, the
  BLDY 12 darkening outside the native window rows of
  `sHaze_PowerBomb_WindowValuesPointers`, exported to `power_bomb.tsv` by
  `scripts/mzm_c_tables.py`), and gains power bomb ammunition in the debug
  menu and refills.
- Checked in brinstar_000: the explosion grows around the bomb and breaks the
  bomb blocks under Samus.
- Not ported: the native palette and window effects themselves, the screen
  shake.
- Tests: `tests/test_mzm_power_bomb.c` (new: the hitbox, both sweeps and the
  doubled centre block, air skipped, the expansion, hold, implosion and end
  timings, the window loader), `tests/test_mzm_samus.c` (selection,
  placement, one at a time, the spin timings, the explosion start, the last
  one disarming), `tests/test_mzm_sprites.c` (the damage once per power
  bomb) and `tests/test_mzm_c_tables.py` (the window table).
- Validation on 2026-10-12: warning-clean build, CTest 30/30, the Python
  suite (428 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0262 - Zero Mission shinespark and ballspark

- `mzm_samus.c` ports the shinespark start of the ground poses (A without a
  direction, half a block of room above, Samus rising by it) and of the
  mid-air poses, `SamusDelayBeforeShinesparkingGfx` (turning when the opposite
  direction is held), the `SamusSetPose` spark directions (up at 192 by
  default, sideways at 192, diagonal at 160/96 with L or the direction and
  Up), `SamusShinesparking` (stopped by a ceiling or a wall),
  `SamusShinesparkingGfx` (its loop frames), the collision (48 frames of
  invincibility, the collision sound, 17 frames) and the delay after, and the
  ballspark (High Jump in the Morph Ball: the half-block rise, the first-frame
  hop, one second of charge turning toward the input, the spark, the
  collision and the Morph Ball fall), with the sparks' constant velocity in
  `SamusUpdateVelocityPosition` and the sound stops of `SamusSetPose`.
- Sprites touched while sparking die as `DCT_SHINESPARK` with
  `PE_SHINESPARK_DESTROYED` (exported) and its sound; the Samus library maps
  the diagonal spark, the ballspark charge and its collision.
- Checked in tourian_010 (`--item speed-booster`, run, skid, Down, A): the
  spark rises to the ceiling, then the delay after.
- Not ported: the slope landing carry, screen shakes, the launchers, the
  aura sprites.
- Tests: `tests/test_mzm_samus.c` (the start and rise, the charge without
  gravity, the upward speed, the ceiling collision and its timings, the
  sideways spark to a wall, the diagonal velocities, the ballspark timings)
  and `tests/test_mzm_sprites.c` (the shinespark kill).
- Validation on 2026-10-12: warning-clean build, CTest 29/29, the Python
  suite (427 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0261 - Zero Mission speed booster

- `mzm_samus.c` ports the speed booster charge of `SamusRunning` (a timer to
  160 above 96 subpixels a frame; the cap rises to 140 at 120 and 160 at 140),
  `SamusCheckShinesparking` (the speed boost from 160 subpixels a frame, its
  start sound and per-frame stop, never restarted while skidding), the
  speed-boost caps of `SamusUpdatePhysics` (ground and mid-air 160), the
  carry through `SamusSetMidAir` and the speed-boosted landing,
  `SamusSkidding` (10 subpixels a frame, the skidding sound) and the
  shinespark storage (Down alone while skidding, or in the six frames after a
  speed-boosted roll: 180 frames with the charge sound every 16).
- The runtime draws `Running_Speedboosting` (new `run_speedboost` action of
  the Samus library) and the speed boost palette rows (`gFrameCounter8Bit %
  6`), breaks speed blocks with `DESTRUCTING_ACTION_SPEED` and
  `SPEED_ON_GROUND`, and kills weak sprites as `DCT_SPEEDBOOSTER` with
  `PE_SPEEDBOOSTER_DESTROYED` (exported) and its sound.
- The Speed Booster is a debug menu item; `--item NAME` enables an item at
  start for captures.
- Checked in tourian_010 with `--item speed-booster`: the red speed boost
  palette and running pose after about 160 frames, at 5 pixels a frame.
- Not ported: the shinespark and ballspark poses, the speed booster aura
  sprite, liquids.
- Tests: `tests/test_mzm_samus.c` (no boost without the item, the boost
  timing and cap, the jump carry, the skid, the storage and its charge sound,
  the 16-frame skid stop) and `tests/test_mzm_sprites.c` (the speed booster
  kill, its particle and sound).
- Validation on 2026-10-12: warning-clean build, CTest 29/29, the Python
  suite (427 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0260 - Zero Mission bombs and bomb jumps

- `src/runtime/mzm_projectiles.c` ports the bomb branch of the Morph Ball
  fire check (B with the Bombs item and no cooldown, without
  `hasNewProjectile`), the bomb spawn of `ProjectileUpdate` (four at most,
  7 frames of cooldown, at Samus's position) and `ProjectileProcessBomb`: a
  slow then a fast spin of 16 frames each (`sBombOam_Slow`/`_Fast`), then the
  hidden bomb's `PE_BOMB` explosion and 16 frames during which
  `CAA_BOMB_PISTOL` reaches the nine native points, one a frame. Bombs are
  never despawned by distance.
- `ProjectileCheckSamusBombBounce` (without BUGFIX: `ClipdataProcess`, now a
  sprite actor in the runtime's point query) and `SamusBombBounce` /
  the bomb branches of `SamusSetMidAir` (`mzm_samus.c`): straight up when
  Samus is within 5 subpixels of the bomb, otherwise away from the bomb's
  middle against `gPreviousXPosition`, at 48/164; in the air only when
  falling and not from a bomb above; the jump rises as a mid-air jump.
- `ProjectileBombHitSprite`: the explosion's sprite pass deals 8 to sprites
  weak to beams and bombs (immune and solid hits otherwise), with no particle;
  natively it runs before the projectile loop, so the runtime keeps the pass
  for the frame after the explosion starts.
- `ParticleBomb` joins the ported particles (exported by
  `scripts/mzm_sprites.py`) with its sound; `SOUND_BOMB_SET` plays on
  placement. The Bombs item is in every suited preset of the runtime.
- `--input-script MASK:FRAMES,...` scripts the GBA buttons of captures.
- Checked in brinstar_000 (captures at frames 50 and 75): the explosion, the
  bounce, then the three bomb blocks under Samus breaking.
- Not ported: Power Bombs, the morph ball launchers, liquids.
- Tests: `tests/test_mzm_samus.c` (timings, the nine points in order, the
  explosion particle, the straight bounce and its rise, the sideways and
  airborne rules, the limit of four) and `tests/test_mzm_sprites.c` (bomb
  damage only during the explosion's sprite pass).
- Validation on 2026-10-12: warning-clean build, CTest 29/29, the Python
  suite (427 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0259 - Zero Mission gameplay sound effects

- `src/runtime/mzm_sfx.c` carries the `SoundPlay`, `SoundPlayNotAlreadyPlaying`,
  `SoundStop`, `SoundFade` and `StopAllMusicAndSounds` calls of the gameplay
  modules to a sink the runtime points at the sound engine, and records them
  for tests. `scripts/mzm_sound_ids.py` generates `src/runtime/mzm_sound_ids.h`
  from the decompilation's `Sound` enum; `check_generated` flags it when stale.
- Samus (`mzm_samus.c`): the jump sounds by velocity and the Morph Ball jump
  (`SamusSetMidAir`), the wall jump, the landing effect on plain ground
  (`SamusCheckSetEnvironmentalEffect`), the footsteps on running frames 1 and 6,
  the spin, Space Jump and Screw Attack sounds on their first animation tick,
  crouching, morphing, unmorphing, the ledge grab and pull, the hurt sound,
  and `SamusDying`'s stop-all then dying sound. Liquids, the wet and dusty
  grounds and the suitless poses are not modelled.
- Weapons: the missile toggle and arming sounds, the beam shot, the missile
  and super missile shot and thrust; the impact particles play their impact
  and explosion sounds and stop the thrust.
- Hatches open, close or lock with their sound; blocks play the square block,
  screw attack, speed booster and bomb chain sounds (the chain sound fades
  when the last chain ends).
- Enemies: `SP_DAMAGED` is now kept (`damaged`) and each primary AI's damaged
  sound plays on screen; `SpriteUtilSpriteDeath` plays the explosion or screw
  attack kill sound; the drops play their pickup sounds; the Skree, Sidehopper,
  Dessgeega, Zeb, Geega, Zebbo, Reo, Mella, Holtz, Multiviola, Space Pirate
  (footsteps, jumps, landings, climbing, crawling, dying, alert, lasers),
  security laser and searchlight eye projectile sounds play at their native
  points and timings.
- The diagnostic restart after a death starts the room's track again (there
  is no game over flow).
- Tests: Samus and weapon sounds in `tests/test_mzm_samus.c`, hatch sounds in
  `tests/test_mzm_hatches.c`, block sounds in `tests/test_mzm_blocks.c`, the
  damaged, death and pickup sounds in `tests/test_mzm_sprites.c`, and the
  generated header in `tests/test_mzm_audio.py`.
- Validation on 2026-10-12: warning-clean build, CTest 29/29, the Python
  suite (427 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0258 - Zero Mission sound engine and room music

- `src/runtime/mzm_audio.c` ports the game's own sound engine routine by
  routine: `src/audio.c` (`UpdateMusic`, `UpdatePsgSounds`, `UpdateTrack` and
  the sequencer commands), `src/audio_wrappers.c`, the sound wrappers of
  `src/music_wrappers.c`, the assembly of `asm/audio_internal.s` (`InitTrack`,
  `StopMusicOrSound`, `ResetTrack`, `UpdateAudio`, the tempo, pitch, volume and
  channel start helpers, `UploadSampleToWaveRam`, the PSG register writes)
  and the mixer of `asm/soundcode.s` (`SoundCodeA` with its fast and checked
  groups of four samples, the reverb pass `SoundCodeB`, the clamp pass
  `SoundCodeC`, `GetNoteFrequency`, the DMA 2 interrupt). Engine structures
  keep ROM pointers as GBA addresses and the track variables in their RAM
  order, as the native code depends on both.
- `src/runtime/gba_psg.c` emulates the sound hardware the engine writes to:
  the two square channels (sweep, envelope, length), the wave channel and
  its two banks, the noise channel, the 512 Hz sequencer and the
  `SOUNDCNT_L/H/X` mixing of the PSG with the FIFO samples. The FIFO is fed
  as the timer 0 DMA does (224 samples a frame at 13379 Hz, the 84-block
  ring) and resampled to 48 kHz.
- `scripts/mzm_audio.py` (rebuild task `metroid_sound`) exports the ROM span
  of the sound objects (from the reference link map) to a private
  `sound_bank.bin`, and the engine tables of `src/data/audio.c` evaluated
  from the decompilation and checked against the ROM, plus the ROM bytes the
  engine reads past two tables, the `DetermineNewMusicTrack` pairs and the
  default track, to `sound_tables.tsv`. `scripts/mzm_c_tables.py` evaluates
  `SAMPLES_PER_SECOND` in single precision.
- Room exports gain a `U` line (the room entry's `musicTrack`). The runtime
  opens an SDL3 audio stream, runs `UpdateAudio` and one frame of sound
  hardware per game frame, starts the first room's track and, on each door
  or debug teleport, calls `CheckSetNewMusicTrack` (30-frame fade) then
  `CheckPlayTransitionMusicTrack` 32 frames later, since the runtime has no
  door transition. `--mute` disables sound; captures stay silent.
- Checked: the Brinstar and Chozo ruins tracks render 20 seconds under
  AddressSanitizer and UBSan without errors or clipping (up to seven
  DirectSound channels). The output has not been compared with hardware or
  an emulator.
- Not ported: gameplay sound effects (the `SoundPlay` calls of Samus,
  weapons, hatches, blocks and sprites), the alarm, Norfair and cutscene
  music cases, `InsertMusicAndQueueCurrent` and `ReplayQueuedMusic`.
- Tests: `tests/test_mzm_audio.c` (a synthetic bank: a looped DirectSound
  note at the native rate with its measured pitch and amplitude, the gate
  and `FINE`, a PSG sound effect started and stopped, a 30-frame fade into
  an aliased transition track, the default track, and a PSG square wave
  frequency) and `tests/test_mzm_audio.py` (link map parsing, the engine
  tables, the aliases, the room music value).
- Validation on 2026-10-12: warning-clean build, CTest 29/29, the Python
  suite (425 tests), every rebuild task up to date, `check_generated` fresh
  and the proprietary-file guard passed.

## Patch 0257 - Zero Mission breakable blocks

- `src/runtime/mzm_blocks.c` ports the block routines of `src/block.c`:
  `BlockApplyCcaa` (hatches first), the destruction and reveal rules by
  weakness, 2x2 blocks, never-reform blocks (remembered per room and removed
  on reload), reforming blocks with the native stages and BG1 frames (held
  while Samus stands in them, including the native read past the timer
  row), hidden tanks, bomb chains and their sub-chains,
  `BlockSamusApplyScrewSpeedboosterDamageToEnvironment`.
- The runtime keeps the room's raw Clipdata values, routes every beam,
  missile, super missile and pirate laser impact through `BlockApplyCcaa`,
  breaks blocks under the screw attack
  (`SamusCheckScrewSpeedboosterAffectingEnvironment`) and crumble blocks
  under Samus (`BgClipCheckWalkingOnCrumbleBlock`).
- `scripts/mzm_c_tables.py` evaluates the decompilation's constant
  initializers (enums, `CONVERT_SECONDS` in single precision,
  `BEHAVIOR_TO_*`, simple defines); the room export writes the block tables
  and the collision and behavior of every raw value to `blocks.tsv`.
- Checked in brinstar_022: crumble blocks give way under Samus and a shot
  block breaks through its frames.
- Not ported: bombs, power bombs, the speed booster, grabbing crumble
  blocks, block sounds, tank pickups.
- Tests: `tests/test_mzm_blocks.c` (no-reform shot block frames, reforming
  timing and the hold while Samus stands in it, 2x2 never-reform blocks and
  their reload removal, a hidden tank, the hatch path, a bomb chain, the
  screw attack) and `tests/test_mzm_c_tables.py`.
- Validation on 2026-10-11: warning-clean build, CTest 28/28, the Python
  suite, every rebuild task up to date, `check_generated` fresh and the
  proprietary-file guard passed.

## Patch 0244 - dashboard monsters and items bestiary

- New *Monsters & items* dashboard tab: every native monster and item of both
  games, done or not. Zero Mission rows reuse the object catalog's parsing of
  the pinned sprite enum and stats (95 monsters, 10 items); a monster counts
  as done when the `mzm_sprite_family` switch of `src/runtime/mzm_sprites.c`
  gives it an AI family (38 today), and the five enemy drops of `is_drop()`
  count as ported items. Aria lists all 113 enemy ids; only the six ported
  enemies of the `AOS_ENEMY` enum get a name, because resolving the others
  needs the ROM constructor table, which the dashboard never reads. Aria items
  are the nine documented pickup families, all without runtime behavior.
- The tab reuses the status badges, search and status filters. Its method is
  stated on the page: "done" means a ported AI exists, never complete fidelity
  (sounds and unported states remain).
- `scripts/dashboard.py` derives the bestiary from tracked sources only
  (no ROM, no private asset), carries it in `dashboard/data.json`/`data.js`,
  and `scripts/check_generated.py` now flags a stale bestiary.
- Tests: `tests/test_dashboard.py` covers the row counts, the done sets
  (Zoomer, Sidehopper, the six Aria enemies), the drop-versus-Water-Drop
  boundary, unnamed Aria enemies and the engine-switch parsing.
- Validation on 2026-10-12: the warning-clean build succeeded, CTest passed
  28/28, the Python suite passed 418/418, `check_generated` reported fresh
  documents and the proprietary-file guard passed.
