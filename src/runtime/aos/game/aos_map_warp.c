/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_map_warp.h"

#include <stdio.h>
#include <string.h>

enum {
    AOS_ROM_BASE = 0x08000000,
    /* sUnk_08525FBC: the warp-point table sub_0804F84C/sub_0804F85C
     * index — {s16 map_x, s16 map_y, u32 room_record} stride 8. */
    AOS_WARP_TABLE = 0x08525FBC,
    AOS_WARP_TABLE_STRIDE = 8,
    /* sUnk_08116650: the 64x35 castle-map u16 cells (bit 15 save,
     * bit 14 warp, bits 6-9 area, bits 0-5 room) — the same table
     * quick_map.castle mirrors. */
    AOS_CASTLE_TABLE = 0x08116650,
    AOS_CASTLE_WIDTH = 64,
    AOS_CASTLE_HEIGHT = 35,
    /* Native PlaySong ids fired inside sub_080116A8/sub_080114FC. */
    AOS_WARP_SONG_CONFIRM = 0xF4,   /* SE_244 */
    AOS_WARP_SONG_CANCEL = 0xF0,    /* SE_240 */
    AOS_WARP_SONG_MOVE = 0xF1       /* SE_241 */
};

static void set_error(char *error, size_t size, const char *message) {
    if (!error || !size) return;
    snprintf(error, size, "%s", message);
    error[size - 1] = '\0';
}

static const uint8_t *rom_slice(const FusionRom *rom, uint32_t pointer,
                                size_t size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA ||
        pointer < AOS_ROM_BASE)
        return NULL;
    size_t offset = pointer - AOS_ROM_BASE;
    if (offset > rom->size || size > rom->size - offset) return NULL;
    return rom->data + offset;
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | bytes[1] << 8);
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static int16_t read_s16(const uint8_t *bytes) {
    return (int16_t)read_u16(bytes);
}

bool aos_map_warp_init(AosMapWarp *warp, const FusionRom *rom,
                       char *error, size_t error_size) {
    if (!warp || !rom) {
        set_error(error, error_size, "invalid Aria warp-map request");
        return false;
    }
    *warp = (AosMapWarp){ .selected = -1, .song = -1 };
    /* The native loops bound the table scan by the null var_4 and the
     * picker cycles 0..7; read at most AOS_WARP_POINT_COUNT records. */
    const uint8_t *table = rom_slice(rom, AOS_WARP_TABLE,
                                     AOS_WARP_POINT_COUNT *
                                         AOS_WARP_TABLE_STRIDE);
    if (!table) {
        set_error(error, error_size, "truncated Aria warp-point table");
        return false;
    }
    const uint8_t *castle = rom_slice(rom, AOS_CASTLE_TABLE,
                                      AOS_CASTLE_WIDTH * AOS_CASTLE_HEIGHT * 2);
    if (!castle) {
        set_error(error, error_size, "truncated Aria castle map table");
        return false;
    }
    for (size_t i = 0; i < AOS_WARP_POINT_COUNT; ++i) {
        const uint8_t *entry = table + i * AOS_WARP_TABLE_STRIDE;
        /* sub_0804F84C/sub_0804F85C terminate on the null room-record
         * pointer at record offset 4. */
        if (read_u32(entry + 4) == 0) break;
        int16_t x = read_s16(entry);
        int16_t y = read_s16(entry + 2);
        AosWarpPoint *point = &warp->points[warp->count];
        point->map_x = (uint8_t)x;
        point->map_y = (uint8_t)y;
        point->valid = false;
        point->area = -1;
        point->room = -1;
        if (x >= 0 && x < AOS_CASTLE_WIDTH && y >= 0 &&
            y < AOS_CASTLE_HEIGHT) {
            uint16_t cell = read_u16(castle +
                                     ((size_t)y * AOS_CASTLE_WIDTH + x) * 2);
            if (cell != 0xFFFF) {
                point->area = (cell >> 6) & 0xF;
                point->room = cell & 0x3F;
                point->valid = true;
            }
        }
        warp->count++;
    }
    warp->available = warp->count != 0;
    if (error && error_size) error[0] = '\0';
    return warp->available;
}

void aos_map_warp_visit(AosMapWarp *warp, int area, int room) {
    if (!warp || !warp->available) return;
    /* sub_0804F7A4 scans sUnk_08525FBC for the player's map cell and
     * sets unk_378 bit index; matching the resolved room is the same
     * test for the single-cell warp rooms. */
    for (size_t i = 0; i < warp->count && i < AOS_WARP_POINT_COUNT; ++i)
        if (warp->points[i].valid && warp->points[i].area == area &&
            warp->points[i].room == room) {
            warp->discovered |= (uint8_t)(1u << i);
            return;
        }
}

static int warp_point_index(const AosMapWarp *warp, int area, int room) {
    for (size_t i = 0; i < warp->count && i < AOS_WARP_POINT_COUNT; ++i)
        if (warp->points[i].valid && warp->points[i].area == area &&
            warp->points[i].room == room)
            return (int)i;
    return -1;
}

static bool warp_discovered(const AosMapWarp *warp, int index) {
    return index >= 0 && index < AOS_WARP_POINT_COUNT &&
           (warp->discovered & (uint8_t)(1u << index)) != 0;
}

bool aos_map_warp_picker_begin(AosMapWarp *warp, int area, int room) {
    if (!warp || !warp->available || warp->count == 0) return false;
    /* sub_080116A8 unk_65 == 0 tail: unk_3F4 = sub_0804F85C(cell)
     * clamped to 0, then the `!(unk_378[0] & 1)` arm discovers point
     * 0 and recenters on it. */
    int index = warp_point_index(warp, area, room);
    warp->selected = (int8_t)(index < 0 ? 0 : index);
    if ((warp->discovered & 1) == 0) {
        warp->discovered |= 1;
        warp->selected = 0;
    }
    warp->stage = 0;
    warp->fade = 16;                /* sub_080115D4 set bldY = MAX */
    warp->stage_frames = 0;
    warp->request_pending = false;
    warp->song = -1;
    return true;
}

/* sub_080114FC: the UP|LEFT / DOWN|RIGHT repeated-input walk over
 * discovered points, wrapping 0..7 like the native bounds. */
static void warp_cursor_step(AosMapWarp *warp, uint16_t repeated) {
    if (warp->discovered == 0) return;
    int sel = warp->selected < 0 ? 0 : warp->selected;
    /* Two independent native ifs: UP|LEFT decrements, DOWN|RIGHT
     * increments; both held runs both walks in order. Each walk is
     * bounded at one full lap so a corrupt mask cannot spin (the
     * native mask always holds bit 0 after stage 0). */
    for (int pass = 0; pass < 2; ++pass) {
        uint16_t mask = pass == 0 ? (AOS_KEY_UP | AOS_KEY_LEFT)
                                  : (AOS_KEY_DOWN | AOS_KEY_RIGHT);
        int direction = pass == 0 ? -1 : 1;
        if (!(repeated & mask)) continue;
        warp->song = AOS_WARP_SONG_MOVE;
        for (int lap = 0; lap < AOS_WARP_POINT_COUNT; ++lap) {
            sel += direction;
            if (sel < 0) sel = AOS_WARP_POINT_COUNT - 1;
            if (sel >= AOS_WARP_POINT_COUNT) sel = 0;
            if (warp_discovered(warp, sel)) break;
        }
    }
    warp->selected = (int8_t)sel;
}

int aos_map_warp_picker_step(AosMapWarp *warp, uint16_t new_input,
                             uint16_t repeated_input, uint32_t frame) {
    (void)frame;    /* the cursor pulse reads it host-side */
    if (!warp || !warp->available) return AOS_WARP_PICKER_BUSY;
    switch (warp->stage) {
    case 0:
        /* bldY already BLDY_MAX_VALUE (sub_080115D4): the map setup
         * runs and unk_65 advances the same call. */
        warp->stage = 1;
        return AOS_WARP_PICKER_BUSY;
    case 1:
        /* bldY -= 4 per frame; the frame that reaches 0 installs the
         * window registers and advances. */
        warp->fade = warp->fade > 4 ? (uint8_t)(warp->fade - 4) : 0;
        if (warp->fade == 0) warp->stage = 2;
        return AOS_WARP_PICKER_BUSY;
    case 2: {
        /* sub_080114FC cursor walk, then the A / B|Select edges. */
        warp_cursor_step(warp, repeated_input);
        if (new_input & (AOS_KEY_B | AOS_KEY_SELECT)) {
            /* PlaySong(SE_240), fade regs, unk_3F4 = -1. */
            warp->song = AOS_WARP_SONG_CANCEL;
            warp->selected = -1;
            warp->fade = 0;
            warp->stage = 3;
            return AOS_WARP_PICKER_BUSY;
        }
        if (new_input & AOS_KEY_A && warp->selected >= 0 &&
            warp_discovered(warp, warp->selected)) {
            /* PlaySong(SE_244) + sub_08011F44(var_4, 0, 0, 0x78,
             * 0x80): the warp request case 8 / think state 4
             * commits. var_4 resolves to the point's room through
             * the castle cell like the native room record. */
            const AosWarpPoint *point =
                &warp->points[warp->selected];
            warp->request_pending = true;
            warp->req_area = point->area;
            warp->req_room = point->room;
            warp->req_x = 0x78;
            warp->req_y = 0x80;
            warp->song = AOS_WARP_SONG_CONFIRM;
            warp->fade = 0;
            warp->stage = 3;
        }
        return AOS_WARP_PICKER_BUSY;
    }
    case 3:
        /* bldY += 4 per frame until BLDY_MAX_VALUE. */
        warp->fade = (uint8_t)(warp->fade + 4);
        if (warp->fade >= 16) {
            warp->fade = 16;
            warp->stage = 4;
        }
        return AOS_WARP_PICKER_BUSY;
    case 4:
        /* Display/register restore frame. */
        warp->stage = 5;
        return AOS_WARP_PICKER_BUSY;
    default: /* stage 5: sp4 = 1 — sub_080116A8 returns 1 */
        warp->stage = 0;
        return warp->request_pending ? AOS_WARP_PICKER_CONFIRMED
                                     : AOS_WARP_PICKER_CANCELLED;
    }
}

void aos_map_warp_commit_begin(AosMapWarp *warp) {
    if (!warp) return;
    /* sub_08011AD0: unk_64 = 8, unk_65 = 0 and the fade registers.
     * The request fields persist until stage 1 consumes them. */
    warp->commit_stage = 0;
    warp->request_pending = false;
}

int aos_map_warp_commit_step(AosMapWarp *warp) {
    if (!warp) return AOS_WARP_COMMIT_DONE;
    switch (warp->commit_stage) {
    case 0:
        /* sub_08000B64 + unk_A074_2/4 = 3 frame. */
        warp->commit_stage = 1;
        return AOS_WARP_COMMIT_BUSY;
    case 1: {
        /* The native stage-1 branch loads the destination room
         * synchronously (unk_88 = unk_3F8, player position =
         * unk_400/unk_402, sub_0800ECA0(unk_3FC, unk_3FE),
         * sub_0801083C) and falls through into stage 2's restore in
         * the same call. The host performs the room swap on the
         * LOAD frame; the next call reports DONE like the native
         * `return 1` tail (unk_64 = 1). */
        warp->commit_stage = 2;
        return AOS_WARP_COMMIT_LOAD;
    }
    default:
        warp->commit_stage = 0;
        return AOS_WARP_COMMIT_DONE;
    }
}

void aos_map_warp_overlay(const AosMapWarp *warp, uint8_t *rgba,
                          size_t rgba_size, uint32_t frame) {
    if (!warp || !rgba || rgba_size < 240 * 160 * 4) return;
    /* sub_08002324's pulsing selection box: radius 16 - (frame>>2
     * & 15) like the quick-map cursor, drawn as a bright corner
     * bracket per discovered point and a filled pulse on the
     * selected one. Cell (x, y) maps to output pixels
     * [14 + 4x, 18 + 4x) x [6 + 4y, 10 + 4y) (BG0HOFS = 2 over
     * the 256x256 BG, matching quick_map_render's sampling). */
    int phase = (int)((frame >> 2) & 15);
    int radius = phase < 8 ? phase : 15 - phase;   /* 0..7 pulse */
    for (size_t i = 0; i < warp->count && i < AOS_WARP_POINT_COUNT; ++i) {
        const AosWarpPoint *point = &warp->points[i];
        if (!point->valid || !(warp->discovered & (1u << i))) continue;
        int cx = 14 + point->map_x * 4 + 2;
        int cy = 6 + point->map_y * 4 + 2;
        bool selected = (int)i == warp->selected;
        int half = selected ? 3 + (radius & 3) : 2;
        for (int dy = -half; dy <= half; ++dy) {
            for (int dx = -half; dx <= half; ++dx) {
                int px = cx + dx, py = cy + dy;
                if (px < 0 || px >= 240 || py < 0 || py >= 160) continue;
                bool edge = dx == -half || dx == half || dy == -half ||
                            dy == half;
                if (!edge && !selected) continue;
                size_t out = ((size_t)py * 240 + (size_t)px) * 4;
                uint8_t boost = selected ? (uint8_t)(160 + radius * 8)
                                         : (uint8_t)96;
                for (int ch = 0; ch < 3; ++ch) {
                    unsigned v = rgba[out + ch] +
                                 (255u - rgba[out + ch]) * boost / 255u;
                    rgba[out + ch] = (uint8_t)(v > 255 ? 255 : v);
                }
            }
        }
    }
}
