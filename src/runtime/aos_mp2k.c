/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_mp2k.h"

enum {
    MP2K_WAIT_FIRST = 0x80,
    MP2K_WAIT_LAST = 0xB0,
    MP2K_FINE = 0xB1,
    MP2K_GOTO = 0xB2,
    MP2K_PATT = 0xB3,
    MP2K_PEND = 0xB4,
    MP2K_REPT = 0xB5,
    MP2K_PRIO = 0xBA,
    MP2K_TEMPO = 0xBB,
    MP2K_KEYSH = 0xBC,
    MP2K_VOICE = 0xBD,
    MP2K_VOL = 0xBE,
    MP2K_PAN = 0xBF,
    MP2K_BEND = 0xC0,
    MP2K_BENDR = 0xC1,
    MP2K_LFOS = 0xC2,
    MP2K_LFODL = 0xC3,
    MP2K_MOD = 0xC4,
    MP2K_MODT = 0xC5,
    MP2K_TUNE = 0xC8,
    MP2K_PORT = 0xCC,
    MP2K_XCMD = 0xCD,
    MP2K_EOT = 0xCE,
    MP2K_NOTE_FIRST = 0xCF,
    MP2K_COMMAND_LIMIT = 1024
};

static const uint8_t clock_table[49] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17, 0x18, 0x1C, 0x1E, 0x20, 0x24, 0x28,
    0x2A, 0x2C, 0x30, 0x34, 0x36, 0x38, 0x3C, 0x40, 0x42, 0x44,
    0x48, 0x4C, 0x4E, 0x50, 0x54, 0x58, 0x5A, 0x5C, 0x60,
};

static bool fail(AosMp2kTrack *track) {
    track->active = false;
    track->failed = true;
    return false;
}

static bool read_byte(AosMp2kTrack *track, uint8_t *value) {
    if (track->cursor >= track->size) return fail(track);
    *value = track->data[track->cursor++];
    return true;
}

static bool read_pointer(AosMp2kTrack *track, size_t *target) {
    if (track->cursor > track->size || track->size - track->cursor < 4)
        return fail(track);
    const uint8_t *bytes = track->data + track->cursor;
    uint32_t pointer = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
                       (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
    track->cursor += 4;
    if (pointer < track->address_base) return fail(track);
    uint64_t offset = (uint64_t)pointer - track->address_base;
    if (offset >= track->size) return fail(track);
    *target = (size_t)offset;
    return true;
}

void aos_mp2k_track_start(AosMp2kTrack *track, const uint8_t *data,
                          size_t size, uint32_t address_base) {
    if (!track) return;
    *track = (AosMp2kTrack){
        .data = data,
        .size = size,
        .address_base = address_base,
        .bend_range = 2,
        .lfo_speed = 0x16,
        .active = data != NULL && size != 0,
    };
}

void aos_mp2k_track_clear_notes(AosMp2kTrack *track) {
    if (track) track->note_count = 0;
}

static bool parameter(AosMp2kTrack *track, uint8_t command) {
    uint8_t value;
    if (!read_byte(track, &value)) return false;
    switch (command) {
    case MP2K_PRIO: track->priority = value; break;
    case MP2K_TEMPO: track->tempo_raw = (uint16_t)(value * 2u); break;
    case MP2K_KEYSH: track->key_shift = (int8_t)value; break;
    case MP2K_VOICE: track->voice = value; break;
    case MP2K_VOL: track->volume = value; break;
    case MP2K_PAN: track->pan = (int8_t)(value - 0x40); break;
    case MP2K_BEND: track->bend = (int8_t)(value - 0x40); break;
    case MP2K_BENDR: track->bend_range = value; break;
    case MP2K_LFOS: track->lfo_speed = value; break;
    case MP2K_LFODL: track->lfo_delay = value; break;
    case MP2K_MOD: track->mod_depth = value; break;
    case MP2K_MODT: track->mod_type = value; break;
    case MP2K_TUNE: track->tune = (int8_t)(value - 0x40); break;
    default: return fail(track);
    }
    return true;
}

static bool note(AosMp2kTrack *track, uint8_t command) {
    uint8_t gate = clock_table[command - MP2K_NOTE_FIRST];
    uint8_t key = track->key, velocity = track->velocity, extension;
    if (track->cursor < track->size && track->data[track->cursor] < 0x80)
        if (!read_byte(track, &key)) return false;
    if (track->cursor < track->size && track->data[track->cursor] < 0x80)
        if (!read_byte(track, &velocity)) return false;
    if (track->cursor < track->size && track->data[track->cursor] < 0x80) {
        if (!read_byte(track, &extension)) return false;
        gate = (uint8_t)(gate + extension);
    }
    track->key = key;
    track->velocity = velocity;
    track->gate = gate;
    if (track->note_count < AOS_MP2K_NOTE_LOG_CAPACITY)
        track->notes[track->note_count++] = (AosMp2kNote){key, velocity, gate};
    return true;
}

static bool control(AosMp2kTrack *track, uint8_t command) {
    size_t target;
    switch (command) {
    case MP2K_FINE:
        track->active = false;
        return true;
    case MP2K_GOTO:
        if (!read_pointer(track, &target)) return false;
        track->cursor = target;
        return true;
    case MP2K_PATT:
        if (track->pattern_level >= AOS_MP2K_PATTERN_DEPTH) return fail(track);
        if (!read_pointer(track, &target)) return false;
        track->pattern_stack[track->pattern_level++] = track->cursor;
        track->cursor = target;
        return true;
    case MP2K_PEND:
        if (track->pattern_level)
            track->cursor = track->pattern_stack[--track->pattern_level];
        return true;
    case MP2K_REPT: {
        size_t count_position = track->cursor;
        uint8_t count;
        if (!read_byte(track, &count)) return false;
        if (!count) {
            if (!read_pointer(track, &target)) return false;
            track->cursor = target;
            return true;
        }
        track->repeat_count++;
        if (track->repeat_count < count) {
            if (!read_pointer(track, &target)) return false;
            track->cursor = target;
        } else {
            track->repeat_count = 0;
            track->cursor = count_position + 5;
        }
        return true;
    }
    case MP2K_EOT: {
        uint8_t key = track->key;
        if (track->cursor < track->size && track->data[track->cursor] < 0x80)
            if (!read_byte(track, &key)) return false;
        track->end_tie_key = key;
        track->key = key;
        track->end_tie_count++;
        return true;
    }
    default:
        if ((command >= MP2K_PRIO && command <= MP2K_MODT) || command == MP2K_TUNE)
            return parameter(track, command);
        if (command == MP2K_PORT || command == MP2K_XCMD) return fail(track);
        /* The native jump table routes its reserved event slots to FINE. */
        track->active = false;
        return true;
    }
}

bool aos_mp2k_track_tick(AosMp2kTrack *track) {
    if (!track || !track->active || track->failed) return false;
    if (track->wait) {
        track->wait--;
        return true;
    }
    for (unsigned commands = 0; commands < MP2K_COMMAND_LIMIT && track->active;
         ++commands) {
        uint8_t command;
        if (!read_byte(track, &command)) return false;
        if (command < 0x80) {
            if (!track->running_status) return fail(track);
            track->cursor--;
            command = track->running_status;
        } else if (command >= MP2K_VOICE) {
            track->running_status = command;
        }
        if (command >= MP2K_WAIT_FIRST && command <= MP2K_WAIT_LAST) {
            track->wait = clock_table[command - MP2K_WAIT_FIRST];
            if (!track->wait) continue;
            track->wait--;
            return true;
        }
        if (command >= MP2K_NOTE_FIRST) {
            if (!note(track, command)) return false;
        } else if (!control(track, command)) {
            return false;
        }
    }
    if (track->active) return fail(track);
    return !track->failed;
}
