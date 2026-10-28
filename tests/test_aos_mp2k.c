/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_mp2k.h"

enum { BASE = 0x08300000 };

static void write_pointer(uint8_t *data, size_t offset, size_t target) {
    uint32_t pointer = BASE + (uint32_t)target;
    data[offset] = (uint8_t)pointer;
    data[offset + 1] = (uint8_t)(pointer >> 8);
    data[offset + 2] = (uint8_t)(pointer >> 16);
    data[offset + 3] = (uint8_t)(pointer >> 24);
}

static void parameter_and_note_test(void) {
    const uint8_t stream[] = {
        0x80, 0xBA, 7, 0xBB, 75, 0xBC, 0xFE, 0xBD, 9, 0xBE, 100,
        0xBF, 0x20, 0xC0, 0x44, 0xC1, 12, 0xC2, 20, 0xC3, 5,
        0xC4, 6, 0xC5, 2, 0xC8, 0x42,
        0xD3, 60, 110, 2, 0x83, 62, 90, 0xCE, 62, 0xB1,
    };
    AosMp2kTrack track;
    aos_mp2k_track_start(&track, stream, sizeof stream, BASE);
    assert(aos_mp2k_track_tick(&track));
    assert(track.priority == 7 && track.tempo_raw == 150);
    assert(track.key_shift == -2 && track.voice == 9 && track.volume == 100);
    assert(track.pan == -32 && track.bend == 4 && track.bend_range == 12);
    assert(track.lfo_speed == 20 && track.lfo_delay == 5);
    assert(track.mod_depth == 6 && track.mod_type == 2 && track.tune == 2);
    assert(track.note_count == 1);
    assert(track.notes[0].key == 60 && track.notes[0].velocity == 110);
    assert(track.notes[0].gate == 6);
    assert(track.wait == 2);
    assert(aos_mp2k_track_tick(&track) && track.wait == 1);
    assert(aos_mp2k_track_tick(&track) && track.wait == 0);
    assert(aos_mp2k_track_tick(&track));
    assert(track.note_count == 2 && track.notes[1].key == 62);
    assert(track.notes[1].velocity == 90 && track.notes[1].gate == 4);
    assert(track.end_tie_count == 1 && track.end_tie_key == 62);
    assert(!track.active && !track.failed);
}

static void pattern_and_repeat_test(void) {
    uint8_t stream[40] = {0xB3};
    write_pointer(stream, 1, 28);
    stream[5] = 0xB5;
    stream[6] = 3;
    write_pointer(stream, 7, 16);
    stream[11] = 0xB1;
    stream[16] = 0xCF;
    stream[17] = 40;
    stream[18] = 80;
    stream[19] = 0x81;
    stream[20] = 0xB2;
    write_pointer(stream, 21, 5);
    stream[28] = 0xD0;
    stream[29] = 50;
    stream[30] = 70;
    stream[31] = 0xB4;

    AosMp2kTrack track;
    aos_mp2k_track_start(&track, stream, sizeof stream, BASE);
    assert(aos_mp2k_track_tick(&track));
    assert(track.note_count == 2);
    assert(track.notes[0].key == 50 && track.notes[1].key == 40);
    assert(track.pattern_level == 0 && track.wait == 0);
    assert(aos_mp2k_track_tick(&track));
    assert(track.note_count == 3 && track.notes[2].key == 40);
    assert(track.active && !track.failed);
    assert(aos_mp2k_track_tick(&track));
    assert(!track.active && !track.failed);
}

static void running_status_and_failure_test(void) {
    const uint8_t stream[] = {0xD0, 64, 100, 0x81, 65, 90, 0x82, 66, 80, 0xB1};
    AosMp2kTrack track;
    aos_mp2k_track_start(&track, stream, sizeof stream, BASE);
    assert(aos_mp2k_track_tick(&track));
    assert(aos_mp2k_track_tick(&track));
    assert(track.note_count == 2);
    assert(track.notes[0].key == 64 && track.notes[1].key == 65);
    assert(track.notes[1].velocity == 90);
    assert(aos_mp2k_track_tick(&track) && track.wait == 0);
    assert(aos_mp2k_track_tick(&track));
    assert(track.note_count == 3 && track.notes[2].key == 66);
    assert(!track.active && !track.failed);

    const uint8_t bad[] = {0xB2, 0, 0, 0, 7};
    aos_mp2k_track_start(&track, bad, sizeof bad, BASE);
    assert(!aos_mp2k_track_tick(&track));
    assert(track.failed && !track.active);

    const uint8_t unsupported[] = {0xCD, 0};
    aos_mp2k_track_start(&track, unsupported, sizeof unsupported, BASE);
    assert(!aos_mp2k_track_tick(&track));
    assert(track.failed && !track.active);
}

int main(void) {
    parameter_and_note_test();
    pattern_and_repeat_test();
    running_status_and_failure_test();
    puts("aos MP2K track tests passed");
    return 0;
}
