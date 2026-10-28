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

static void engine_song_start_test(void) {
    uint8_t bank[128] = {0};
    bank[16] = 2;
    bank[17] = 1;
    bank[18] = 50;
    bank[19] = 0x80;
    write_pointer(bank, 20, 96);
    write_pointer(bank, 24, 64);
    write_pointer(bank, 28, 80);
    bank[64] = 0xB1;
    bank[80] = 0xB1;

    AosMp2kEngine engine;
    aos_mp2k_engine_init(&engine);
    assert(aos_mp2k_engine_set_bank(&engine, bank, sizeof bank, BASE));
    assert(aos_mp2k_player_start(&engine, 2, BASE + 16));
    const AosMp2kPlayer *player = aos_mp2k_player(&engine, 2);
    assert(player && player->active && player->track_count == 2);
    assert(player->header_pointer == BASE + 16 && player->voicegroup_pointer == BASE + 96);
    assert(player->block_count == 1 && player->priority == 50 && player->reverb == 0x80);
    assert(player->tempo_raw == 150 && player->tempo_interval == 150);
    assert(player->tempo_scale == 0x100);
    assert(player->tracks[0].cursor == 64 && player->tracks[1].cursor == 80);
    assert(player->tracks[0].bend_range == 2 && player->tracks[0].lfo_speed == 0x16);

    write_pointer(bank, 24, sizeof bank);
    assert(!aos_mp2k_player_start(&engine, 2, BASE + 16));
    assert(aos_mp2k_player(&engine, 2)->tracks[0].cursor == 64);
    assert(!aos_mp2k_player_start(&engine, AOS_MP2K_PLAYER_COUNT, BASE + 16));
    aos_mp2k_engine_destroy(&engine);
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

/* MP2KPlayerMain's tempo clock: 150 per clock, TEMPO doubling its value. */
static void player_tempo_test(void) {
    const uint8_t stream[] = {0xBB, 60, 0x83, 0xB1};
    AosMp2kPlayer player = {.active = true, .track_count = 1, .tempo_raw = 150,
                            .tempo_interval = 150, .tempo_scale = 0x100};
    aos_mp2k_track_start(&player.tracks[0], stream, sizeof stream, BASE);
    /* Frame 1: one clock reads TEMPO 60 (raw 120, interval 120) and waits 3. */
    aos_mp2k_player_frame(&player);
    assert(player.clock == 1 && player.tempo_raw == 120 && player.tempo_interval == 120);
    assert(player.tempo_counter == 0 && player.tracks[0].wait == 2 && player.track_bits == 1);
    /* Frame 2: 120 is under 150, no clock. */
    aos_mp2k_player_frame(&player);
    assert(player.clock == 1 && player.tempo_counter == 120);
    /* Frames 3-5: 240, 210, 180 each give one clock; the fifth reads FINE. */
    aos_mp2k_player_frame(&player);
    assert(player.clock == 2 && player.tempo_counter == 90 && player.tracks[0].wait == 1);
    aos_mp2k_player_frame(&player);
    assert(player.clock == 3 && player.tempo_counter == 60 && player.tracks[0].wait == 0);
    aos_mp2k_player_frame(&player);
    assert(player.clock == 4 && player.tempo_counter == 30 && !player.tracks[0].active);
    assert(!player.ended && player.track_bits == 1);
    /* Frame 6: the next clock finds no track and ends the player. */
    aos_mp2k_player_frame(&player);
    assert(player.ended && player.clock == 5 && player.track_bits == 0 && !player.failed);
    aos_mp2k_player_frame(&player);
    assert(player.clock == 5);
    /* The scale applies to the next TEMPO (m4aMPlayTempoControl keeps it). */
    AosMp2kPlayer scaled = {.active = true, .track_count = 1, .tempo_raw = 150,
                            .tempo_interval = 150, .tempo_scale = 0x80};
    aos_mp2k_track_start(&scaled.tracks[0], stream, sizeof stream, BASE);
    aos_mp2k_player_frame(&scaled);
    assert(scaled.tempo_interval == 60);
}

int main(void) {
    player_tempo_test();
    engine_song_start_test();
    parameter_and_note_test();
    pattern_and_repeat_test();
    running_status_and_failure_test();
    puts("aos MP2K track tests passed");
    return 0;
}
