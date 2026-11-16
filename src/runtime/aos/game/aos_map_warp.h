/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MAP_WARP_H
#define AOS_MAP_WARP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/aos.h"
#include "fusion/assets/rom.h"

#include "../core/aos_input.h"

/* GameModeInGameUpdate (sub_0800B700) cases 7 and 8: the warp-room
 * picker chain sub_080115D4 -> sub_080116A8 -> sub_08011AD0 ->
 * sub_08011B2C (third_party/cvaos src/code_080109F4.c,
 * src/code_0800B700.c).
 *
 * sUnk_08525FBC is the ROM warp-point table sub_0804F84C/
 * sub_0804F85C/sub_0804F7A4 index: eight-byte records
 * {s16 map_x, s16 map_y, u32 room_record_pointer} terminated by a
 * null pointer. The shipped table holds eight entries — the eight
 * castle-map warp rooms — and GameModeInGameUpdate's case-7 tail
 * and sub_080114FC's cursor walk both cycle unk_60.unk_3F4 over
 * indices 0..7. Each entry's (map_x, map_y) is the warp room's
 * castle-map cell; the cell's own u16 in sUnk_08116650 (the same
 * table quick_map.castle mirrors) decodes engine area bits 6-9 and
 * room bits 0-5, which resolves the destination room without
 * touching the native room-record pointer.
 *
 * unk_60.unk_378 is the discovered-point bitmask sub_0804F7A4 sets
 * inside every sub_0800F9EC room load when the player's map cell
 * matches a table entry; sub_080114FC only cycles selections whose
 * bit is set. unk_60.unk_3F4 is the picker index (-1 after a
 * B/Select cancel). Confirming with A runs sub_08011F44(point's
 * room record, 0, 0, 0x78, 0x80): the warp request case 8 (or
 * Object1F's think state 4, see below) commits.
 *
 * Native routing note: the only shipped picker opener is
 * Object1F's think (sub_08055748), which sets unk_42C bit 0x20000
 * on the same frame it calls sub_080115D4. GameModeInGameUpdate's
 * case-7 tail then takes the `unk_42C & 0x20000` branch — unk_64 = 1
 * and the bit cleared — so case 8 does NOT run for Object1F; the
 * warp commit happens inside the think's state 4 instead (the same
 * unk_88 = unk_3F8 / sub_0800ECA0 / sub_0801083C sequence case 8
 * stage 1 performs, plus the unk_400/unk_402 player-position copy).
 * Other sub_08011AD0 callers (Object15/Object16 door branches and
 * the link-sync path) stay unported, so the runtime performs the
 * commit through this module's case-8 stage model for the picker
 * path and documents the routing divergence. */

/* sUnk_08525FBC entry count is fixed at eight in the shipped ROM;
 * the native cursor walk wraps unk_3F4 over 0..7. */
enum { AOS_WARP_POINT_COUNT = 8 };

typedef struct {
    uint8_t map_x;      /* sUnk_08525FBC unk_0: castle cell x */
    uint8_t map_y;      /* sUnk_08525FBC unk_2: castle cell y */
    int area;           /* castle cell bits 6-9 (engine area) */
    int room;           /* castle cell bits 0-5 */
    bool valid;         /* cell decoded (not 0xFFFF) */
} AosWarpPoint;

typedef struct {
    AosWarpPoint points[AOS_WARP_POINT_COUNT];
    size_t count;               /* entries read (<= 8) */
    bool available;             /* ROM table + castle cells read */
    uint8_t discovered;         /* unk_60.unk_378 bits 0-7 */
    int8_t selected;            /* unk_60.unk_3F4; -1 = cancelled */
    /* sub_080116A8 stage machine (unk_65) with the modeled
     * gDisplayRegisters.bldY (0..16) that drives the runtime's
     * black veil. */
    uint8_t stage;
    uint8_t fade;               /* modeled bldY */
    uint8_t stage_frames;       /* frames left in timed stages */
    int song;                   /* pending PlaySong id, -1 = none */
    /* sub_08011F44 warp request consumed by the commit. */
    bool request_pending;
    int req_area, req_room;     /* resolved destination room */
    int req_x, req_y;           /* unk_400/unk_402 player position */
    /* sub_08011B2C stage machine. */
    uint8_t commit_stage;
} AosMapWarp;

/* Picker step results (mapped onto GameModeInGameUpdate's case-7
 * tail and the runtime's commit trigger). */
enum {
    AOS_WARP_PICKER_BUSY = 0,   /* sub_080116A8 still running */
    AOS_WARP_PICKER_CONFIRMED,  /* A pressed: request written, picker done */
    AOS_WARP_PICKER_CANCELLED   /* B/Select: unk_3F4 = -1, picker done */
};

/* Commit step results (sub_08011B2C). */
enum {
    AOS_WARP_COMMIT_BUSY = 0,
    AOS_WARP_COMMIT_LOAD,       /* stage-1 frame: perform the room swap */
    AOS_WARP_COMMIT_DONE        /* sub_08011B2C returned 1 -> unk_64 = 1 */
};

/* Reads sUnk_08525FBC (0x08525FBC, stride 8, null-pointer
 * terminated) and resolves each entry's castle cell through
 * sUnk_08116650 (0x08116650) like sub_0804F85C/sub_0804F84C do.
 * Requires an Aria ROM; without one the feature stays unavailable
 * (the native unk_88 map data would be absent too). */
bool aos_map_warp_init(AosMapWarp *warp, const FusionRom *rom,
                       char *error, size_t error_size);

/* sub_0804F7A4, run by sub_0800F9EC on every room load: when the
 * room just entered is a warp point its discovered bit sets. Matches
 * on the resolved (engine_area, room) — warp rooms occupy exactly
 * their table cell. */
void aos_map_warp_visit(AosMapWarp *warp, int area, int room);

/* sub_080116A8 stage 0's tail (the bldY == BLDY_MAX_VALUE branch):
 * unk_3F4 = sub_0804F85C(player map cell), clamped to 0 when the
 * cell is no warp point, and the `!(unk_378[0] & 1)` first-use arm
 * that discovers point 0 and recenters the cursor on it. `area`/
 * `room` are the room the picker opened in. Returns false when the
 * table is unavailable or empty. */
bool aos_map_warp_picker_begin(AosMapWarp *warp, int area, int room);

/* One sub_080116A8 call. `new_input`/`repeated_input` are the
 * inputData words (GBA key bits, AOS_KEY_*); `frame` is the host
 * frame counter standing in for gEwramData.frameCounter (the
 * stage-2 cursor pulse). Stage 2 is the only interactive stage:
 * B|Select cancels (unk_3F4 = -1), A writes the sub_08011F44 warp
 * request {point room, 0, 0, 0x78, 0x80}. Returns a
 * AOS_WARP_PICKER_* result; BUSY while the stage machine runs. */
int aos_map_warp_picker_step(AosMapWarp *warp, uint16_t new_input,
                             uint16_t repeated_input, uint32_t frame);

/* sub_08011AD0: latches the case-8 commit. Only valid after a
 * CONFIRMED picker step (request_pending). */
void aos_map_warp_commit_begin(AosMapWarp *warp);

/* One sub_08011B2C call. AOS_WARP_COMMIT_LOAD marks the frame whose
 * stage-1 branch performs the native room swap (unk_88 = unk_3F8,
 * player position = unk_400/unk_402, sub_0800ECA0(unk_3FC,
 * unk_3FE), sub_0801083C) — the host loads warp->req_area/req_room
 * at (warp->req_x, warp->req_y). AOS_WARP_COMMIT_DONE is the
 * routine's `return 1`: GameModeInGameUpdate's case-8 tail writes
 * unk_64 = 1. */
int aos_map_warp_commit_step(AosMapWarp *warp);

/* Pixel marks for the picker display, drawn over the castle-map
 * backdrop (the same sUnk_08116650/sUnk_080E3664 resources the
 * quick map renders): every discovered warp point gets a corner
 * bracket at its cell and the selected point a brighter pulsing
 * box, approximating sub_08002324's cursor sprite. `rgba` is a
 * 240x160 RGBA frame; cell (x, y) maps to output pixels
 * [14 + 4x, 18 + 4x) x [6 + 4y, 10 + 4y) — the BG0HOFS = 2 /
 * 256x256-BG sampling quick_map_render performs. */
void aos_map_warp_overlay(const AosMapWarp *warp, uint8_t *rgba,
                          size_t rgba_size, uint32_t frame);

#endif /* AOS_MAP_WARP_H */
