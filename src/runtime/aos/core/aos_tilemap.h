/* SPDX-License-Identifier: GPL-3.0-only */
/* Per-room background tilemap lookup for the scroll tile-row rebuild.
 *
 * Native chain this ports (third_party/cvaos):
 *   - sub_0800F9EC binds each bgInfo[1..3] layer to a ROM
 *     EwramData_unkA078_0 metadata record through
 *     sub_0803F8A8/sub_0803F970; the record's unk_0/unk_1 are the
 *     map size in screens, unk_2 the flags (bit 1 = tile data is
 *     LZ77-compressed into gEwramData+0xA108/0xC0EC, bit 0 = affine
 *     layer), unk_4 the tile-word array and unk_C the 4x4-tile block
 *     map.
 *   - sub_08001800 resolves (tile_x, tile_y) to a cell index:
 *     block = unk_C[(tile_x >> 2) + (tile_y >> 2) * 8 * unk_0]; a zero
 *     block id yields no tile; the entry's low 14 bits pick a 4x4
 *     group ((block - 1) * 16 + (tile_x & 3) + (tile_y & 3) * 4) with
 *     the 0x4000/0x8000 bits mirroring the in-block coordinates, and
 *     the top two bits (kept in gUnk_03002CB0.unk_100C) XOR into the
 *     result's bits 8-9.
 *   - sub_08001994 returns that tilemap word (0 for an empty block)
 *     and is what sub_0803F3F8/3F4C8/3F580/3F650 call 0x15/0x1F times
 *     to fill the gEwramData+0x13414 staging buffer that
 *     BgCmdBuffer_WriteData streams to the BG tilemap in VRAM.
 *
 * The fusion assets reader fusion_aos_read_background_tiles already
 * performs exactly this decode (same block-map stride, flips,
 * (block-1)*16 cell order and palette XOR) into a dense per-tile
 * word array, so this module loads the three room backgrounds once
 * and answers sub_08001994 lookups directly from it. Differences
 * from the native reads, all unreachable for clamped scroll states:
 *   - coordinates outside the decoded map return 0 where the native
 *     code would index past the block map;
 *   - affine layers (metadata flags bit 0) stay
 *     FUSION_AOS_TILEMAP_UNSUPPORTED_AFFINE and look up as 0 — the
 *     native affine path sub_0803F7C8 uses a different 8x4 tiling
 *     that is not modelled here.
 */
#ifndef AOS_TILEMAP_H
#define AOS_TILEMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/aos.h"
#include "fusion/assets/rom.h"

/* The room's three scrollable backgrounds: index bg - 1 for BG1..BG3.
 * Each layer keeps its FusionAosTilemapStatus so callers can tell an
 * absent layer from an undecodable (affine) one. */
typedef struct {
    FusionAosBackgroundTilemap layers[FUSION_AOS_BACKGROUND_COUNT];
} AosRoomTilemap;

/* Reads the room descriptor and its three background tile arrays from
 * the ROM. Succeeds when every layer read succeeds; per-layer decode
 * status is kept (ABSENT/UNSUPPORTED_AFFINE layers look up as 0). */
bool aos_room_tilemap_load(AosRoomTilemap *tilemap, const FusionRom *rom,
                           const FusionAosAreaDirectory *directory,
                           size_t area, size_t room,
                           char *error, size_t error_size);
void aos_room_tilemap_free(AosRoomTilemap *tilemap);

/* The decode status of one layer (bg is 1..3); FUSION_AOS_TILEMAP_ABSENT
 * for an out-of-range bg. */
FusionAosTilemapStatus aos_tilemap_status(const AosRoomTilemap *tilemap,
                                          int bg);

/* sub_08001994: the BG tilemap word at map tile coordinates
 * (tile_x, tile_y), palette bits already XORed in. Returns 0 for an
 * absent/affine layer, an undecoded map or out-of-map coordinates —
 * the native code has no bounds check and would read adjacent data;
 * the scroll clamps keep real lookups inside the map. */
uint16_t aos_tilemap_lookup(const AosRoomTilemap *tilemap, int bg,
                            uint16_t tile_x, uint16_t tile_y);

/* The two rebuild spans of sub_0803F3F8/3F4C8 (fixed tile_x, tile_y
 * stepping upward; the native loop runs 0x15 lookups) and
 * sub_0803F580/3F650 (fixed tile_y, tile_x stepping upward; 0x1F
 * lookups). Writes up to `count` words to `out` (the caller passes
 * the native count, capped by `cap`; out may be NULL, which performs
 * no lookups and returns `count`) and returns the count written.
 * Coordinates past the map edge yield 0
 * words, matching an empty native block. */
size_t aos_tilemap_column(const AosRoomTilemap *tilemap, int bg,
                          uint16_t tile_x, uint16_t tile_y, size_t count,
                          uint16_t *out, size_t cap);
size_t aos_tilemap_row(const AosRoomTilemap *tilemap, int bg,
                       uint16_t tile_x, uint16_t tile_y, size_t count,
                       uint16_t *out, size_t cap);

#endif
