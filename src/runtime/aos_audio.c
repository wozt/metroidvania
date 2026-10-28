/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_audio.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum {
    AOS_STOP_SONG_FLAG = 0x8000,
    AOS_DEFAULT_SOUND_MODE = 0x0093F800,
    AOS_SOUND_MODE_FREQ_MASK = 0x000F0000,
    AOS_SOUND_MODE_DA_MASK = 0x00B00000
};

static AosAudioSink g_sink;
static void *g_context;
static AosAudioRequest g_log[AOS_AUDIO_LOG_CAPACITY];
static int g_count;
static AosAudioSongInfo g_songs[AOS_AUDIO_MAX_SONGS];
static size_t g_song_count;
static AosAudioState g_state = {.current_song = -1, .sound_mode = AOS_DEFAULT_SOUND_MODE};

void aos_audio_set_sink(AosAudioSink sink, void *context) {
    g_sink = sink;
    g_context = context;
}

static void request(AosAudioAction action, uint16_t value) {
    if (g_count < AOS_AUDIO_LOG_CAPACITY)
        g_log[g_count++] = (AosAudioRequest){action, value};
    if (g_sink) g_sink(g_context, action, value);
}

bool aos_audio_set_song_table(const AosAudioSongInfo *songs, size_t count) {
    if ((!songs && count != 0) || count > AOS_AUDIO_MAX_SONGS) return false;
    for (size_t i = 0; i < count; ++i)
        if (songs[i].player >= AOS_AUDIO_PLAYER_COUNT) return false;
    if (count != 0) memcpy(g_songs, songs, count * sizeof *songs);
    g_song_count = count;
    return true;
}

static bool load_error(char *error, size_t error_size, const char *message) {
    if (error && error_size) {
        snprintf(error, error_size, "%s", message);
        error[error_size - 1] = '\0';
    }
    return false;
}

bool aos_audio_load_song_table(const char *path, char *error, size_t error_size) {
    if (!path) return load_error(error, error_size, "invalid song-table path");
    FILE *file = fopen(path, "rb");
    if (!file) return load_error(error, error_size, "cannot open song table");
    char line[512];
    if (!fgets(line, sizeof line, file) ||
        strcmp(line, "schema\tmetroidvania-aos-song-table-v1\n")) {
        fclose(file);
        return load_error(error, error_size, "invalid Aria song-table schema");
    }
    AosAudioSongInfo songs[AOS_AUDIO_MAX_SONGS];
    size_t count = 0;
    unsigned line_number = 1;
    while (fgets(line, sizeof line, file)) {
        ++line_number;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        unsigned id, header, player, channel, tracks, blocks, priority, reverb;
        int used = 0;
        int fields = sscanf(line, "song\t%u\t0x%x\t%u\t%u\t%u\t%u\t%u\t0x%x%n",
                            &id, &header, &player, &channel, &tracks, &blocks,
                            &priority, &reverb, &used);
        if (fields != 8 || (line[used] != '\n' && line[used] != '\r' && line[used] != '\0') ||
            id != count || count >= AOS_AUDIO_MAX_SONGS ||
            player >= AOS_AUDIO_PLAYER_COUNT || channel >= AOS_AUDIO_PLAYER_COUNT ||
            tracks > 16 || blocks > UINT8_MAX || priority > UINT8_MAX ||
            reverb > UINT8_MAX || header < 0x08000000u) {
            char message[96];
            snprintf(message, sizeof message, "invalid Aria song-table row %u", line_number);
            fclose(file);
            return load_error(error, error_size, message);
        }
        songs[count++] = (AosAudioSongInfo){(uint8_t)player, (uint8_t)priority};
    }
    if (ferror(file)) {
        fclose(file);
        return load_error(error, error_size, "cannot read Aria song table");
    }
    fclose(file);
    if (!count) return load_error(error, error_size, "Aria song table is empty");
    if (!aos_audio_set_song_table(songs, count))
        return load_error(error, error_size, "cannot install Aria song table");
    if (error && error_size) error[0] = '\0';
    return true;
}

size_t aos_audio_song_count(void) { return g_song_count; }

void aos_audio_reset(void) {
    memset(&g_state, 0, sizeof g_state);
    g_state.current_song = -1;
    g_state.sound_mode = AOS_DEFAULT_SOUND_MODE;
    g_song_count = 0;
    g_count = 0;
}

static void volume_set(unsigned player, uint16_t step, uint8_t volume) {
    if (player >= AOS_AUDIO_PLAYER_COUNT) return;
    AosAudioPlayerState *state = &g_state.players[player];
    if (step != 0) {
        state->volume_step = step;
        state->volume_target = (uint16_t)(volume * 0x10);
        state->volume_coarse = state->volume_updated = state->volume_current;
        state->volume_up = state->volume_current < state->volume_target;
    } else {
        state->volume_current = (uint16_t)(volume * 0x10);
    }
}

static void stop_player(unsigned player) {
    if (player < AOS_AUDIO_PLAYER_COUNT) g_state.players[player].paused = true;
}

static void continue_player(unsigned player) {
    if (player < AOS_AUDIO_PLAYER_COUNT && g_state.players[player].paused)
        g_state.players[player].paused = false;
}

static void start_song(uint16_t song) {
    if (song >= g_song_count) return;
    const AosAudioSongInfo info = g_songs[song];
    AosAudioPlayerState *player = &g_state.players[info.player];
    if (song < AOS_AUDIO_MUSIC_COUNT) g_state.current_song = (int16_t)song;
    if (!player->paused && player->active && player->priority > info.priority) return;
    if (!player->paused && player->active) stop_player(info.player);
    player->active = true;
    player->paused = false;
    player->song = song;
    player->priority = info.priority;
    player->volume_step = 0;
    player->volume_current = 0xFF0;
}

static void stop_song(uint16_t song) {
    if (song >= g_song_count) return;
    const unsigned player_index = g_songs[song].player;
    AosAudioPlayerState *player = &g_state.players[player_index];
    if (player->active && player->song == song) stop_player(player_index);
    player->volume_step = 0;
    if (song < AOS_AUDIO_MUSIC_COUNT) g_state.current_song = -1;
}

static void stop_all(void) {
    for (unsigned i = 0; i < AOS_AUDIO_PLAYER_COUNT; ++i) stop_player(i);
}

static void continue_all(void) {
    for (unsigned i = 0; i < AOS_AUDIO_PLAYER_COUNT; ++i) continue_player(i);
}

static void audio_command(uint16_t command) {
    if (command >= AOS_AUDIO_FREQ_BASE && command < AOS_AUDIO_FREQ_BASE + 12) {
        uint32_t frequency = (uint32_t)(command - AOS_AUDIO_FREQ_BASE + 1) << 16;
        g_state.sound_mode = (g_state.sound_mode & ~AOS_SOUND_MODE_FREQ_MASK) | frequency;
        return;
    }
    if (command >= AOS_AUDIO_PLAYER_MUTE_BASE &&
        command < AOS_AUDIO_PLAYER_MUTE_BASE + AOS_AUDIO_PLAYER_COUNT) {
        volume_set(command - AOS_AUDIO_PLAYER_MUTE_BASE, 0x19, 0);
        return;
    }
    if (command >= AOS_AUDIO_PLAYER_STOP_BASE &&
        command < AOS_AUDIO_PLAYER_STOP_BASE + AOS_AUDIO_PLAYER_COUNT) {
        unsigned player = command - AOS_AUDIO_PLAYER_STOP_BASE;
        stop_player(player);
        /* The native command clears the volume ramp through gSongTable[i],
         * not directly through music player i. */
        if (player < g_song_count)
            g_state.players[g_songs[player].player].volume_step = 0;
        return;
    }
    if (command >= AOS_AUDIO_PLAYER_RESUME_BASE &&
        command < AOS_AUDIO_PLAYER_RESUME_BASE + AOS_AUDIO_PLAYER_COUNT) {
        continue_player(command - AOS_AUDIO_PLAYER_RESUME_BASE);
        return;
    }
    switch (command) {
    case AOS_AUDIO_STOP:
        stop_all();
        g_state.paused = false;
        g_state.current_song = -1;
        for (unsigned i = 0; i < AOS_AUDIO_PLAYER_COUNT; ++i) {
            g_state.players[i].volume_step = 0;
            g_state.players[i].volume_current = 0xFF0;
        }
        break;
    case AOS_AUDIO_PAUSE:
        if (!g_state.paused) {
            g_state.paused = true;
            stop_all();
        }
        break;
    case AOS_AUDIO_RESUME:
        if (g_state.paused) {
            g_state.paused = false;
            continue_all();
        }
        break;
    case AOS_AUDIO_6BIT:
    case AOS_AUDIO_7BIT:
    case AOS_AUDIO_8BIT:
    case AOS_AUDIO_9BIT: {
        uint32_t resolution = (uint32_t)(0xB - (command - AOS_AUDIO_6BIT)) << 20;
        g_state.sound_mode = (g_state.sound_mode & ~AOS_SOUND_MODE_DA_MASK) | resolution;
        break;
    }
    case AOS_AUDIO_SONG_MUTE:
    case AOS_AUDIO_SONG_LOUD:
    case AOS_AUDIO_SONG_QUIET:
        if (g_state.current_song >= 0 && (size_t)g_state.current_song < g_song_count) {
            unsigned player = g_songs[g_state.current_song].player;
            if (command == AOS_AUDIO_SONG_MUTE)
                volume_set(player, 0x19, 0);
            else if (command == AOS_AUDIO_SONG_LOUD)
                volume_set(player, 0x3C, 0xFF);
            else
                volume_set(player, 0x3C, 0x90);
        }
        break;
    default:
        break;
    }
}

void aos_audio_play_song(uint16_t song) {
    /* PlaySong starts table entries below 0x400. Higher values either stop
     * one masked song or pass through to AudioCommand unchanged. */
    if (song < 0x400) {
        request(AOS_AUDIO_START_SONG, song);
        start_song(song);
    } else if ((song & AOS_STOP_SONG_FLAG) != 0) {
        uint16_t stopped = song & 0x3FF;
        request(AOS_AUDIO_STOP_SONG, stopped);
        stop_song(stopped);
    } else {
        request(AOS_AUDIO_COMMAND, song);
        audio_command(song);
    }
}

void aos_audio_volume_update(void) {
    for (unsigned i = 0; i < AOS_AUDIO_PLAYER_COUNT; ++i) {
        AosAudioPlayerState *player = &g_state.players[i];
        if (player->volume_step == 0) continue;
        if (player->volume_up) {
            uint16_t next = (uint16_t)(player->volume_updated + player->volume_step);
            if (player->volume_target <= next || player->volume_updated >= 0xFF0) {
                player->volume_step = 0;
                player->volume_current = player->volume_target;
            } else {
                player->volume_updated = next;
                player->volume_coarse = (uint16_t)(next >> 5);
                player->volume_current = next;
            }
        } else {
            uint16_t next = (uint16_t)(player->volume_updated - player->volume_step);
            if (player->volume_target >= next || player->volume_updated <= 0x100) {
                player->volume_step = 0;
                player->volume_current = player->volume_target;
                if (player->volume_target <= 0x10) stop_player(i);
            } else {
                player->volume_updated = next;
                player->volume_coarse = (uint16_t)(next >> 5);
                player->volume_current = next;
            }
        }
    }
}

const AosAudioState *aos_audio_state(void) { return &g_state; }

void aos_audio_clear(void) { g_count = 0; }
int aos_audio_count(void) { return g_count; }

AosAudioRequest aos_audio_get(int index) {
    return index >= 0 && index < g_count
               ? g_log[index]
               : (AosAudioRequest){AOS_AUDIO_START_SONG, 0};
}

int aos_audio_requested(AosAudioAction action, uint16_t value) {
    for (int i = 0; i < g_count; ++i)
        if (g_log[i].action == action && g_log[i].value == value) return 1;
    return 0;
}
