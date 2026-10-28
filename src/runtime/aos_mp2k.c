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
        .vol_public = 0x40,
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

static void player_mix(AosMp2kEngine *engine, uint8_t number);

void aos_mp2k_engine_player_frame(AosMp2kEngine *engine, uint8_t player) {
    if (!engine || player >= AOS_MP2K_PLAYER_COUNT) return;
    AosMp2kPlayer *state = &engine->players[player];
    if (!state->active || state->ended) return;
    player_frame(engine, state, player);
    if (!state->ended) player_mix(engine, player);
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

/* gFreqTable (m4a_tables.c): 2^31 * 2^(n / 12). gScaleTable is
 * ((14 - key / 12) << 4) | key % 12 for keys 0-179. */
static const uint32_t freq_table[12] = {
    2147483648u, 2275179671u, 2410468894u, 2553802834u, 2705659852u, 2866546760u,
    3037000500u, 3217589947u, 3408917802u, 3611622603u, 3826380858u, 4053909305u,
};

static uint32_t umul_high(uint32_t a, uint32_t b) {
    return (uint32_t)(((uint64_t)a * b) >> 32);
}

static uint32_t scale(unsigned key) {
    unsigned entry = ((14u - key / 12u) << 4) | (key % 12u);
    return freq_table[entry & 0xF] >> (entry >> 4);
}

uint32_t aos_mp2k_key_to_freq(uint32_t wav_freq, uint8_t key, uint8_t fine) {
    uint32_t shifted = (uint32_t)fine << 24;
    if (key > 178) {
        key = 178;
        shifted = 255u << 24;
    }
    uint32_t low = scale(key), high = scale(key + 1u);
    return umul_high(wav_freq, low + umul_high(high - low, shifted));
}

void aos_mp2k_track_vol_pit_set(AosMp2kTrack *track) {
    if (!track) return;
    int32_t x = (int32_t)((uint32_t)(track->volume * track->vol_public) >> 5);
    int32_t y = 2 * track->pan + track->pan_public;
    if (y < -128) y = -128;
    else if (y > 127) y = 127;
    track->vol_right = (uint8_t)((uint32_t)((y + 128) * x) >> 8);
    track->vol_left = (uint8_t)((uint32_t)((127 - y) * x) >> 8);
    int32_t bend = track->bend * track->bend_range;
    int32_t pitch = (track->tune + bend) * 4 + (track->key_shift << 8) +
                    (track->key_shift_public << 8) + track->pitch_public;
    track->key_shift_calculated = (int8_t)(pitch >> 8);
    track->pitch_calculated = (uint8_t)pitch;
}

/* ChnVolSetAsm and the frequency part of MP2K_event_nxx / MP2KPlayerMain. */
static void channel_mix(AosMp2kEngine *engine, AosMp2kChannel *channel,
                        const AosMp2kTrack *track) {
    int32_t right = ((0x80 + channel->rhythm_pan) * channel->velocity * track->vol_right) >> 14;
    int32_t left = ((0x7F - channel->rhythm_pan) * channel->velocity * track->vol_left) >> 14;
    channel->right_volume = (uint8_t)(right > 0xFF ? 0xFF : right);
    channel->left_volume = (uint8_t)(left > 0xFF ? 0xFF : left);
    int key = channel->key + track->key_shift_calculated;
    if (key < 0) key = 0;
    const uint8_t *wave = NULL;
    channel->frequency = 0;
    if (channel->type & 7) return;
    if (!bank_slice(engine, channel->tone.wav, 8, &wave)) {
        engine->missing_samples++;
        return;
    }
    channel->frequency = aos_mp2k_key_to_freq(read_u32(wave + 4), (uint8_t)key,
                                              track->pitch_calculated);
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
        if (channel) engine->cgb_notes++;
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
    AosMp2kTrack *track = &player->tracks[index];
    aos_mp2k_track_vol_pit_set(track);
    channel_mix(engine, channel, track);
}

/* MP2KPlayerMain after its clocks: every track's TrkVolPitSet and its
 * chained channels' volume and frequency (the VOLSET / PITSET flags only
 * skip recomputing the same values). */
static void player_mix(AosMp2kEngine *engine, uint8_t number) {
    AosMp2kPlayer *player = &engine->players[number];
    for (uint8_t index = 0; index < player->track_count; ++index)
        if (player->tracks[index].active) aos_mp2k_track_vol_pit_set(&player->tracks[index]);
    AosMp2kChannel *lists[2] = {engine->direct, engine->cgb};
    size_t counts[2] = {AOS_MP2K_DIRECT_CHANNELS, AOS_MP2K_CGB_CHANNELS};
    for (int list = 0; list < 2; ++list)
        for (size_t index = 0; index < counts[list]; ++index) {
            AosMp2kChannel *channel = &lists[list][index];
            if (!channel->linked || channel->player != number) continue;
            const AosMp2kTrack *track = &player->tracks[channel->track];
            if (!track->active || !(channel->status & AOS_MP2K_CHANNEL_ACTIVE)) continue;
            channel_mix(engine, channel, track);
        }
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

/* The WaveData of a channel: + 0x3 loop flags, + 0x8 loop start, + 0xC
 * size, + 0x10 the signed 8-bit samples. */
typedef struct {
    const uint8_t *data;
    size_t available;   /* sample bytes inside the bank */
    bool loop;
    uint32_t loop_start, size;
} Wave;

static bool wave_of(const AosMp2kEngine *engine, uint32_t pointer, Wave *wave) {
    const uint8_t *header = NULL;
    if (!bank_slice(engine, pointer, 16, &header)) return false;
    *wave = (Wave){.loop = (header[3] & 0xC0) != 0, .loop_start = read_u32(header + 8),
                   .size = read_u32(header + 12)};
    uint64_t offset = (uint64_t)pointer + 16 - engine->bank_base;
    wave->data = engine->bank + offset;
    wave->available = engine->bank_size - (size_t)offset;
    return wave->size <= wave->available && wave->loop_start <= wave->size;
}

/* A sample byte; the one past the end is read too, as natively. */
static int sample_at(const Wave *wave, uint32_t index) {
    return index < wave->available ? (int8_t)wave->data[index] : 0;
}

/* The envelope step at the head of SoundMainRAM's channel loop. Returns
 * false when the channel is (or becomes) silent. */
static bool envelope(AosMp2kChannel *channel, const Wave *wave) {
    uint8_t status = channel->status;
    unsigned env = channel->envelope;
    if (status & AOS_MP2K_CHANNEL_START) {
        if (status & AOS_MP2K_CHANNEL_STOP) {
            channel->status = 0;
            return false;
        }
        status = 3;
        channel->position = 0;
        channel->remaining = (int32_t)wave->size;
        channel->fine = 0;
        env = 0;
        if (wave->loop) status |= AOS_MP2K_CHANNEL_LOOP;
        goto attack;
    }
    if (status & AOS_MP2K_CHANNEL_ECHO) {
        if (--channel->echo_length == 0) {
            channel->status = 0;
            return false;
        }
    } else if (status & AOS_MP2K_CHANNEL_STOP) {
        env = env * channel->tone.release >> 8;
        if (env <= channel->echo_volume) {
        echo:
            env = channel->echo_volume;
            if (!env) {
                channel->status = 0;
                return false;
            }
            status |= AOS_MP2K_CHANNEL_ECHO;
        }
    } else if ((status & 3) == 2) {
        env = env * channel->tone.decay >> 8;
        if (env <= channel->tone.sustain) {
            env = channel->tone.sustain;
            if (!env) goto echo;
            status--;
        }
    } else if ((status & 3) == 3) {
    attack:
        env += channel->tone.attack;
        if (env >= 0xFF) {
            env = 0xFF;
            status--;
        }
    }
    channel->status = status;
    channel->envelope = (uint8_t)env;
    unsigned scaled = (AOS_MP2K_MASTER_VOLUME + 1u) * env >> 4;
    channel->mix_volume =
        (uint8_t)((channel->right_volume + channel->left_volume) * scaled >> 9);
    return true;
}

/* The end of a sample: a looping one restarts within its loop, another
 * ends its channel. */
static bool wrap(AosMp2kChannel *channel, const Wave *wave) {
    uint32_t length = wave->size - wave->loop_start;
    if (!(channel->status & AOS_MP2K_CHANNEL_LOOP) || !length) {
        channel->status = 0;
        return false;
    }
    while (channel->remaining <= 0) channel->remaining += (int32_t)length;
    channel->position = wave->size - (uint32_t)channel->remaining;
    return true;
}

static void mix_channel(AosMp2kChannel *channel, const Wave *wave, int8_t *out, size_t count) {
    int volume = channel->mix_volume;
    if (channel->type & 8) {
        /* Fixed frequency: one sample per output. */
        for (size_t index = 0; index < count; ++index) {
            int value = sample_at(wave, channel->position);
            out[index] = (int8_t)(uint8_t)(out[index] + ((value * volume) >> 8));
            channel->position++;
            if (--channel->remaining == 0 && !wrap(channel, wave)) return;
        }
        return;
    }
    uint32_t step = channel->frequency * AOS_MP2K_DIV_FREQ;
    int s0 = sample_at(wave, channel->position), d = sample_at(wave, channel->position + 1) - s0;
    for (size_t index = 0; index < count; ++index) {
        int value = s0 + (((int32_t)channel->fine * d) >> 23);
        out[index] = (int8_t)(uint8_t)(out[index] + ((value * volume) >> 8));
        channel->fine += step;
        uint32_t advance = channel->fine >> 23;
        if (!advance) continue;
        channel->fine &= 0x7FFFFF;
        channel->remaining -= (int32_t)advance;
        if (channel->remaining <= 0) {
            if (!wrap(channel, wave)) return;
        } else {
            channel->position += advance;
        }
        s0 = sample_at(wave, channel->position);
        d = sample_at(wave, channel->position + 1) - s0;
    }
}

void aos_mp2k_mix(AosMp2kEngine *engine, int8_t *out, size_t count) {
    if (!out) return;
    for (size_t index = 0; index < count; ++index) out[index] = 0;
    if (!engine) return;
    for (size_t index = 0; index < AOS_MP2K_DIRECT_CHANNELS; ++index) {
        AosMp2kChannel *channel = &engine->direct[index];
        if (!(channel->status & AOS_MP2K_CHANNEL_ACTIVE)) continue;
        Wave wave;
        if (!wave_of(engine, channel->tone.wav, &wave)) {
            channel->status = 0;
            continue;
        }
        if (!envelope(channel, &wave)) continue;
        mix_channel(channel, &wave, out, count);
    }
}
