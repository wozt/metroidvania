/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-neutral MP2K track event decoder used by Aria's native audio port. */
#ifndef AOS_MP2K_H
#define AOS_MP2K_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    AOS_MP2K_PATTERN_DEPTH = 3,
    AOS_MP2K_NOTE_LOG_CAPACITY = 32
};

typedef struct {
    uint8_t key;
    uint8_t velocity;
    uint8_t gate;
} AosMp2kNote;

typedef struct {
    const uint8_t *data;
    size_t size;
    uint32_t address_base;
    size_t cursor;
    size_t pattern_stack[AOS_MP2K_PATTERN_DEPTH];
    uint8_t pattern_level;
    uint8_t repeat_count;
    uint8_t wait;
    uint8_t running_status;
    uint8_t key;
    uint8_t velocity;
    uint8_t gate;
    uint8_t priority;
    uint8_t voice;
    uint8_t volume;
    int8_t pan;
    int8_t bend;
    uint8_t bend_range;
    int8_t key_shift;
    int8_t tune;
    uint8_t lfo_speed;
    uint8_t lfo_delay;
    uint8_t mod_depth;
    uint8_t mod_type;
    uint16_t tempo_raw;
    uint8_t end_tie_key;
    bool active;
    bool failed;
    AosMp2kNote notes[AOS_MP2K_NOTE_LOG_CAPACITY];
    size_t note_count;
    size_t end_tie_count;
} AosMp2kTrack;

void aos_mp2k_track_start(AosMp2kTrack *track, const uint8_t *data,
                          size_t size, uint32_t address_base);
/* Advance one MP2K sequencer clock. Voice allocation and mixing are separate. */
bool aos_mp2k_track_tick(AosMp2kTrack *track);
void aos_mp2k_track_clear_notes(AosMp2kTrack *track);

#endif /* AOS_MP2K_H */
