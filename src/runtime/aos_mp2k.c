/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_mp2k.h"

#include <stdio.h>
#include <stdlib.h>

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
    (void)aos_mp2k_track_start_at(track, data, size, address_base, address_base);
}

bool aos_mp2k_track_start_at(AosMp2kTrack *track, const uint8_t *data,
                             size_t size, uint32_t address_base,
                             uint32_t start_address) {
    if (!track) return false;
    *track = (AosMp2kTrack){
        .data = data,
        .size = size,
        .address_base = address_base,
        .bend_range = 2,
        .lfo_speed = 0x16,
        .active = data != NULL && size != 0,
    };
    if (!data || start_address < address_base ||
        (uint64_t)start_address - address_base >= size) {
        track->active = false;
        track->failed = true;
        return false;
    }
    track->cursor = (size_t)(start_address - address_base);
    return true;
}

void aos_mp2k_track_clear_notes(AosMp2kTrack *track) {
    if (track) track->note_count = 0;
}

static bool parameter(AosMp2kTrack *track, uint8_t command) {
    uint8_t value;
    if (!read_byte(track, &value)) return false;
    switch (command) {
    case MP2K_PRIO: track->priority = value; break;
    case MP2K_TEMPO:
        track->tempo_raw = (uint16_t)(value * 2u);
        track->tempo_changed = true;
        break;
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
        track->notes[track->note_count++] = (AosMp2kNote){key, velocity, gate, false,
                                                               track->voice, track->priority};
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
        if (track->note_count < AOS_MP2K_NOTE_LOG_CAPACITY)
            track->notes[track->note_count++] = (AosMp2kNote){key, 0, 0, true, track->voice,
                                                               track->priority};
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

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || !error_size) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

void aos_mp2k_engine_init(AosMp2kEngine *engine) {
    if (engine) *engine = (AosMp2kEngine){.bank_base = AOS_MP2K_BANK_BASE};
}

void aos_mp2k_engine_destroy(AosMp2kEngine *engine) {
    if (!engine) return;
    free(engine->owned_bank);
    aos_mp2k_engine_init(engine);
}

bool aos_mp2k_engine_set_bank(AosMp2kEngine *engine, const uint8_t *bank,
                              size_t size, uint32_t address_base) {
    if (!engine || !bank || !size) return false;
    free(engine->owned_bank);
    *engine = (AosMp2kEngine){
        .bank = bank,
        .bank_size = size,
        .bank_base = address_base,
    };
    return true;
}

bool aos_mp2k_engine_load_bank(AosMp2kEngine *engine, const char *path,
                               char *error, size_t error_size) {
    if (!engine || !path) {
        set_error(error, error_size, "invalid MP2K bank request");
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error(error, error_size, "cannot open MP2K bank");
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error(error, error_size, "cannot seek MP2K bank");
        return false;
    }
    long length = ftell(file);
    if (length != AOS_MP2K_BANK_SIZE || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_error(error, error_size, "invalid MP2K bank size");
        return false;
    }
    uint8_t *bank = malloc(AOS_MP2K_BANK_SIZE);
    if (!bank) {
        fclose(file);
        set_error(error, error_size, "cannot allocate MP2K bank");
        return false;
    }
    bool complete = fread(bank, 1, AOS_MP2K_BANK_SIZE, file) == AOS_MP2K_BANK_SIZE &&
                    ferror(file) == 0;
    fclose(file);
    if (!complete) {
        free(bank);
        set_error(error, error_size, "cannot read MP2K bank");
        return false;
    }
    free(engine->owned_bank);
    *engine = (AosMp2kEngine){
        .owned_bank = bank,
        .bank = bank,
        .bank_size = AOS_MP2K_BANK_SIZE,
        .bank_base = AOS_MP2K_BANK_BASE,
    };
    if (error && error_size) error[0] = '\0';
    return true;
}

static bool bank_slice(const AosMp2kEngine *engine, uint32_t pointer,
                       size_t size, const uint8_t **data) {
    if (!engine || !engine->bank || pointer < engine->bank_base) return false;
    uint64_t offset = (uint64_t)pointer - engine->bank_base;
    if (offset > engine->bank_size || size > engine->bank_size - (size_t)offset)
        return false;
    *data = engine->bank + offset;
    return true;
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

bool aos_mp2k_player_start(AosMp2kEngine *engine, uint8_t player,
                           uint32_t header_pointer) {
    if (!engine || player >= AOS_MP2K_PLAYER_COUNT) return false;
    const uint8_t *song_header = NULL;
    if (!bank_slice(engine, header_pointer, 8, &song_header)) return false;
    uint8_t track_count = song_header[0];
    if (track_count > AOS_MP2K_TRACK_COUNT ||
        !bank_slice(engine, header_pointer, 8 + track_count * 4u, &song_header))
        return false;
    AosMp2kPlayer next = {
        .active = track_count != 0,
        .header_pointer = header_pointer,
        .voicegroup_pointer = read_u32(song_header + 4),
        .track_count = track_count,
        .block_count = song_header[1],
        .priority = song_header[2],
        .reverb = song_header[3],
        .tempo_raw = 150,
        .tempo_interval = 150,
        .tempo_scale = 0x100,
    };
    const uint8_t *voicegroup = NULL;
    if (track_count &&
        !bank_slice(engine, next.voicegroup_pointer, 12, &voicegroup)) return false;
    for (uint8_t index = 0; index < track_count; ++index) {
        uint32_t track_pointer = read_u32(song_header + 8 + index * 4u);
        if (!aos_mp2k_track_start_at(&next.tracks[index], engine->bank,
                                     engine->bank_size, engine->bank_base,
                                     track_pointer)) return false;
    }
    /* MPlayStart's TrackStop: the player's channels stop and leave their
     * chains. */
    AosMp2kChannel *lists[2] = {engine->direct, engine->cgb};
    size_t counts[2] = {AOS_MP2K_DIRECT_CHANNELS, AOS_MP2K_CGB_CHANNELS};
    for (int list = 0; list < 2; ++list)
        for (size_t index = 0; index < counts[list]; ++index) {
            AosMp2kChannel *channel = &lists[list][index];
            if (!channel->linked || channel->player != player) continue;
            if (channel->status & AOS_MP2K_CHANNEL_ACTIVE) channel->status |= AOS_MP2K_CHANNEL_STOP;
            channel->linked = false;
        }
    engine->players[player] = next;
    return true;
}

const AosMp2kPlayer *aos_mp2k_player(const AosMp2kEngine *engine, uint8_t player) {
    return engine && player < AOS_MP2K_PLAYER_COUNT ? &engine->players[player] : NULL;
}

static void channel_clock(AosMp2kEngine *engine, uint8_t player, uint8_t track);
static void channel_events(AosMp2kEngine *engine, uint8_t player, uint8_t track);

static void player_frame(AosMp2kEngine *engine, AosMp2kPlayer *player, uint8_t number) {
    if (!player || !player->active || player->ended) return;
    player->tempo_counter = (uint16_t)(player->tempo_counter + player->tempo_interval);
    while (player->tempo_counter >= 150) {
        uint16_t bits = 0;
        for (uint8_t index = 0; index < player->track_count; ++index) {
            AosMp2kTrack *track = &player->tracks[index];
            if (!track->active) continue;
            bits |= (uint16_t)(1u << index);
            if (engine) channel_clock(engine, number, index);
            aos_mp2k_track_clear_notes(track);
            if (!aos_mp2k_track_tick(track) && track->failed) player->failed = true;
            if (engine) channel_events(engine, number, index);
            /* MP2K_event_tempo writes the player's fields at once. */
            if (track->tempo_changed) {
                track->tempo_changed = false;
                player->tempo_raw = track->tempo_raw;
                player->tempo_interval =
                    (uint16_t)((uint32_t)player->tempo_raw * player->tempo_scale >> 8);
            }
        }
        player->clock++;
        if (!bits) {
            player->ended = true;
            player->track_bits = 0;
            return;
        }
        player->track_bits = bits;
        player->tempo_counter = (uint16_t)(player->tempo_counter - 150);
    }
}

void aos_mp2k_player_frame(AosMp2kPlayer *player) { player_frame(NULL, player, 0); }

void aos_mp2k_engine_player_frame(AosMp2kEngine *engine, uint8_t player) {
    if (engine && player < AOS_MP2K_PLAYER_COUNT)
        player_frame(engine, &engine->players[player], player);
}

void aos_mp2k_engine_frame(AosMp2kEngine *engine) {
    if (!engine) return;
    for (uint8_t player = 0; player < AOS_MP2K_PLAYER_COUNT; ++player)
        aos_mp2k_engine_player_frame(engine, player);
}

bool aos_mp2k_tone(const AosMp2kEngine *engine, uint32_t voicegroup, unsigned index,
                   AosMp2kTone *tone) {
    const uint8_t *data = NULL;
    if (!tone || !bank_slice(engine, voicegroup + index * 12u, 12, &data)) return false;
    *tone = (AosMp2kTone){data[0], data[1], data[2], data[3], read_u32(data + 4),
                          data[8], data[9], data[10], data[11]};
    return true;
}

/* The tone word at + 0x8 read as a pointer (a key split's key table). */
static uint32_t tone_word(const AosMp2kTone *tone) {
    return (uint32_t)tone->attack | (uint32_t)tone->decay << 8 |
           (uint32_t)tone->sustain << 16 | (uint32_t)tone->release << 24;
}

/* The gate countdown at the head of each track's clock. */
static void channel_clock(AosMp2kEngine *engine, uint8_t player, uint8_t track) {
    AosMp2kChannel *lists[2] = {engine->direct, engine->cgb};
    size_t counts[2] = {AOS_MP2K_DIRECT_CHANNELS, AOS_MP2K_CGB_CHANNELS};
    for (int list = 0; list < 2; ++list)
        for (size_t index = 0; index < counts[list]; ++index) {
            AosMp2kChannel *channel = &lists[list][index];
            if (!channel->linked || channel->player != player || channel->track != track)
                continue;
            if (!(channel->status & AOS_MP2K_CHANNEL_ACTIVE)) {
                channel->linked = false;    /* ClearChain */
                continue;
            }
            if (channel->gate && --channel->gate == 0) channel->status |= AOS_MP2K_CHANNEL_STOP;
        }
}

/* Whether a candidate's track comes after another (track-address order). */
static unsigned ordinal(uint8_t player, uint8_t track) { return player * 16u + track; }

static AosMp2kChannel *direct_channel(AosMp2kEngine *engine, uint8_t priority, unsigned self) {
    unsigned best_priority = priority, best_track = self;
    bool stopping = false;
    AosMp2kChannel *best = NULL;
    for (size_t index = 0; index < AOS_MP2K_DIRECT_CHANNELS; ++index) {
        AosMp2kChannel *channel = &engine->direct[index];
        if (!(channel->status & AOS_MP2K_CHANNEL_ACTIVE)) return channel;
        unsigned owner = ordinal(channel->player, channel->track);
        if (channel->status & AOS_MP2K_CHANNEL_STOP) {
            if (!stopping) {
                stopping = true;
                best_priority = channel->priority;
                best_track = owner;
                best = channel;
                continue;
            }
        } else if (stopping) {
            continue;
        }
        if (channel->priority < best_priority) {
            best_priority = channel->priority;
            best_track = owner;
            best = channel;
        } else if (channel->priority == best_priority) {
            if (owner > best_track) {
                best_track = owner;
                best = channel;
            } else if (owner == best_track) {
                best = channel;
            }
        }
    }
    return best;
}

static void play_note(AosMp2kEngine *engine, uint8_t number, uint8_t index,
                      const AosMp2kNote *note) {
    AosMp2kPlayer *player = &engine->players[number];
    AosMp2kTone tone;
    if (!aos_mp2k_tone(engine, player->voicegroup_pointer, note->voice, &tone)) {
        engine->dropped_notes++;
        return;
    }
    uint8_t key = note->key;
    int8_t pan = 0;
    if (tone.type & 0xC0) {
        unsigned entry = key;
        if (tone.type & 0x40) {
            const uint8_t *table = NULL;
            if (!bank_slice(engine, tone_word(&tone) + key, 1, &table)) {
                engine->dropped_notes++;
                return;
            }
            entry = table[0];
        }
        AosMp2kTone sub;
        if (!aos_mp2k_tone(engine, tone.wav, entry, &sub) || (sub.type & 0xC0)) {
            engine->dropped_notes++;
            return;
        }
        if (tone.type & 0x80) {
            if (sub.pan_sweep & 0x80) pan = (int8_t)((sub.pan_sweep - 0xC0) * 2);
            key = sub.key;
        }
        tone = sub;
    }
    unsigned sum = (unsigned)player->priority + note->priority;
    uint8_t priority = (uint8_t)(sum > 0xFF ? 0xFF : sum);
    unsigned self = ordinal(number, index);
    unsigned cgb = tone.type & 7;
    AosMp2kChannel *channel = NULL;
    if (cgb) {
        AosMp2kChannel *candidate = &engine->cgb[(cgb - 1) & 3];
        unsigned owner = ordinal(candidate->player, candidate->track);
        if (!(candidate->status & AOS_MP2K_CHANNEL_ACTIVE) ||
            (candidate->status & AOS_MP2K_CHANNEL_STOP) || candidate->priority < priority ||
            (candidate->priority == priority && owner >= self))
            channel = candidate;
    } else {
        channel = direct_channel(engine, priority, self);
    }
    if (!channel) {
        engine->dropped_notes++;
        return;
    }
    *channel = (AosMp2kChannel){
        .status = AOS_MP2K_CHANNEL_START, .type = tone.type, .key = key,
        .gate = note->gate, .midi_key = note->key, .velocity = note->velocity,
        .priority = priority, .rhythm_pan = pan, .tone = tone, .linked = true,
        .player = number, .track = index, .serial = ++engine->serial,
    };
}

static void end_tie(AosMp2kEngine *engine, uint8_t player, uint8_t track, uint8_t key) {
    AosMp2kChannel *newest = NULL;
    AosMp2kChannel *lists[2] = {engine->direct, engine->cgb};
    size_t counts[2] = {AOS_MP2K_DIRECT_CHANNELS, AOS_MP2K_CGB_CHANNELS};
    for (int list = 0; list < 2; ++list)
        for (size_t index = 0; index < counts[list]; ++index) {
            AosMp2kChannel *channel = &lists[list][index];
            if (!channel->linked || channel->player != player || channel->track != track ||
                !(channel->status & 0x83) || (channel->status & AOS_MP2K_CHANNEL_STOP) ||
                channel->midi_key != key)
                continue;
            if (!newest || channel->serial > newest->serial) newest = channel;
        }
    if (newest) newest->status |= AOS_MP2K_CHANNEL_STOP;
}

static void channel_events(AosMp2kEngine *engine, uint8_t player, uint8_t track) {
    const AosMp2kTrack *state = &engine->players[player].tracks[track];
    for (size_t index = 0; index < state->note_count; ++index) {
        const AosMp2kNote *note = &state->notes[index];
        if (note->end_tie) end_tie(engine, player, track, note->key);
        else play_note(engine, player, track, note);
    }
}
