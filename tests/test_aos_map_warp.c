/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/game/aos_map_warp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_SIZE = 0x540000,
    WARP_TABLE_OFFSET = 0x525FBC,   /* sUnk_08525FBC - 0x08000000 */
    CASTLE_OFFSET = 0x116650,       /* sUnk_08116650 - 0x08000000 */
};

static void write_u16(uint8_t *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
    bytes[offset + 2] = (uint8_t)(value >> 16);
    bytes[offset + 3] = (uint8_t)(value >> 24);
}

static void write_warp_record(uint8_t *bytes, size_t index, int16_t x,
                              int16_t y, uint32_t room_record) {
    size_t offset = WARP_TABLE_OFFSET + index * 8;
    write_u16(bytes, offset, (uint16_t)x);
    write_u16(bytes, offset + 2, (uint16_t)y);
    write_u32(bytes, offset + 4, room_record);
}

static void write_castle_cell(uint8_t *bytes, int x, int y, uint16_t cell) {
    write_u16(bytes, CASTLE_OFFSET + ((size_t)y * 64 + (size_t)x) * 2, cell);
}

int main(void) {
    uint8_t *bytes = calloc(1, ROM_SIZE);
    assert(bytes);
    /* sUnk_08525FBC: three records then the null-pointer terminator.
     * Cells decode area = bits 6-9, room = bits 0-5. */
    write_warp_record(bytes, 0, 10, 10, 0x08123456);
    write_warp_record(bytes, 1, 20, 12, 0x08123456);
    write_warp_record(bytes, 2, 30, 14, 0x08123456);
    write_castle_cell(bytes, 10, 10, (uint16_t)((3 << 6) | 5));
    write_castle_cell(bytes, 20, 12, (uint16_t)((4 << 6) | 9));
    write_castle_cell(bytes, 30, 14, 0xFFFF);   /* undecodable cell */

    FusionRom rom = {bytes, ROM_SIZE, FUSION_ASSET_GAME_ARIA};
    AosMapWarp warp;
    char error[160];
    assert(aos_map_warp_init(&warp, &rom, error, sizeof error));
    assert(warp.available && warp.count == 3);
    assert(warp.points[0].map_x == 10 && warp.points[0].map_y == 10);
    assert(warp.points[0].area == 3 && warp.points[0].room == 5);
    assert(warp.points[0].valid);
    assert(warp.points[1].area == 4 && warp.points[1].room == 9);
    assert(!warp.points[2].valid && warp.points[2].area == -1);

    /* sub_0804F7A4: visiting the warp room sets its discovered bit. */
    aos_map_warp_visit(&warp, 3, 5);
    assert(warp.discovered == 1);
    aos_map_warp_visit(&warp, 4, 9);
    assert(warp.discovered == 3);
    aos_map_warp_visit(&warp, 0, 0);   /* not a warp room */
    assert(warp.discovered == 3);

    /* sub_080116A8 stage 0: picker_begin selects the current warp
     * room's index and the first-use arm discovers point 0. */
    assert(aos_map_warp_picker_begin(&warp, 4, 9));
    assert(warp.selected == 1);
    warp.discovered = 0;    /* re-arm the first-use bit check */
    assert(aos_map_warp_picker_begin(&warp, 4, 9));
    assert(warp.selected == 0 && (warp.discovered & 1));

    /* Stage walk: 0 -> 1 -> 2 (interactive). Stage 2 waits on input. */
    assert(aos_map_warp_picker_step(&warp, 0, 0, 0) == AOS_WARP_PICKER_BUSY);
    assert(warp.stage == 1);
    for (int i = 0; i < 8 && warp.stage == 1; ++i)
        assert(aos_map_warp_picker_step(&warp, 0, 0, (uint32_t)i) ==
               AOS_WARP_PICKER_BUSY);
    assert(warp.stage == 2);

    /* Cursor walk: only discovered points are selectable. With
     * discovered == 1 the DOWN|RIGHT walk wraps back to point 0. */
    warp.discovered = 1;
    warp.selected = 0;
    aos_map_warp_picker_step(&warp, 0, AOS_KEY_DOWN, 0);
    assert(warp.selected == 0);

    /* B cancels: unk_3F4 = -1 after the fade-out stages. */
    assert(aos_map_warp_picker_step(&warp, AOS_KEY_B, 0, 0) ==
           AOS_WARP_PICKER_BUSY);
    assert(warp.selected == -1);
    int result = AOS_WARP_PICKER_BUSY;
    for (int i = 0; i < 12 && result == AOS_WARP_PICKER_BUSY; ++i)
        result = aos_map_warp_picker_step(&warp, 0, 0, (uint32_t)i);
    assert(result == AOS_WARP_PICKER_CANCELLED);

    /* A fresh picker in room 3/5 confirms point 0 and writes the
     * sub_08011F44 request {area 3, room 5, 0, 0, 0x78, 0x80}. */
    aos_map_warp_visit(&warp, 3, 5);
    assert(aos_map_warp_picker_begin(&warp, 3, 5));
    assert(warp.selected == 0);
    assert(aos_map_warp_picker_step(&warp, 0, 0, 0) == AOS_WARP_PICKER_BUSY);
    while (warp.stage != 2)
        assert(aos_map_warp_picker_step(&warp, 0, 0, 0) ==
               AOS_WARP_PICKER_BUSY);
    assert(aos_map_warp_picker_step(&warp, AOS_KEY_A, 0, 0) ==
           AOS_WARP_PICKER_BUSY);
    assert(warp.request_pending);
    assert(warp.req_area == 3 && warp.req_room == 5);
    assert(warp.req_x == 0x78 && warp.req_y == 0x80);
    result = AOS_WARP_PICKER_BUSY;
    for (int i = 0; i < 12 && result == AOS_WARP_PICKER_BUSY; ++i)
        result = aos_map_warp_picker_step(&warp, 0, 0, (uint32_t)i);
    assert(result == AOS_WARP_PICKER_CONFIRMED);

    /* sub_08011AD0 + sub_08011B2C: BUSY, LOAD (the room-swap frame),
     * DONE (the routine's return 1 -> unk_64 = 1). */
    aos_map_warp_commit_begin(&warp);
    assert(!warp.request_pending);
    assert(aos_map_warp_commit_step(&warp) == AOS_WARP_COMMIT_BUSY);
    assert(aos_map_warp_commit_step(&warp) == AOS_WARP_COMMIT_LOAD);
    assert(aos_map_warp_commit_step(&warp) == AOS_WARP_COMMIT_DONE);

    /* The overlay marks discovered points without touching pixels
     * outside the 240x160 frame. */
    uint8_t *rgba = calloc(240 * 160 * 4, 1);
    assert(rgba);
    aos_map_warp_overlay(&warp, rgba, 240 * 160 * 4, 0);
    bool any = false;
    for (size_t i = 0; i < 240 * 160 * 4; i += 4)
        any |= rgba[i] || rgba[i + 1] || rgba[i + 2];
    assert(any);
    free(rgba);
    free(bytes);

    /* A missing table stays unavailable instead of reading garbage. */
    AosMapWarp empty;
    memset(&empty, 0xAA, sizeof empty);
    FusionRom bad = {NULL, 0, FUSION_ASSET_GAME_ARIA};
    assert(!aos_map_warp_init(&empty, &bad, error, sizeof error));
    assert(!empty.available);
    aos_map_warp_visit(&empty, 3, 5);          /* no-op */
    assert(aos_map_warp_picker_begin(&empty, 3, 5) == false);
    printf("aos_map_warp: all checks passed\n");
    return 0;
}
