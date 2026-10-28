/* SPDX-License-Identifier: GPL-3.0-only */
/* Private export of Aria's gSongTable and MP2K header metadata. */
#include "fusion/assets/aos.h"

#include "export.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GBA_BASE = 0x08000000,
    SONG_TABLE = 0x0827CD38,
    SONG_RECORD_SIZE = 8
};

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

static void set_error(char *error, size_t error_size, const char *format, ...) {
    if (!error || error_size == 0) return;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
    error[error_size - 1] = '\0';
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)(data[0] | (uint16_t)data[1] << 8);
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)read_u16(data) | (uint32_t)read_u16(data + 2) << 16;
}

static bool slice(const FusionRom *rom, uint32_t pointer, size_t size,
                  const uint8_t **data, char *error, size_t error_size) {
    if (!rom || !rom->data || pointer < GBA_BASE ||
        (uint64_t)pointer - GBA_BASE > rom->size ||
        size > rom->size - ((size_t)pointer - GBA_BASE)) {
        set_error(error, error_size, "Aria sound pointer 0x%08X is outside the ROM",
                  pointer);
        return false;
    }
    *data = rom->data + (pointer - GBA_BASE);
    return true;
}

bool fusion_aos_read_song_table(const FusionRom *rom,
                                FusionAosSongTable *table,
                                char *error, size_t error_size) {
    if (!rom || rom->game != FUSION_ASSET_GAME_ARIA || !table) {
        set_error(error, error_size, "invalid Aria song-table request");
        return false;
    }
    *table = (FusionAosSongTable){0};
    const uint8_t *records = NULL;
    if (!slice(rom, SONG_TABLE, FUSION_AOS_SONG_COUNT * SONG_RECORD_SIZE,
               &records, error, error_size)) return false;
    for (size_t index = 0; index < FUSION_AOS_SONG_COUNT; ++index) {
        const uint8_t *record = records + index * SONG_RECORD_SIZE;
        FusionAosSong *song = &table->entries[index];
        song->header_pointer = read_u32(record);
        song->player = read_u16(record + 4);
        song->channel = read_u16(record + 6);
        if (song->player >= 8 || song->channel >= 8) {
            set_error(error, error_size,
                      "Aria song %zu selects invalid player %u or channel %u",
                      index, song->player, song->channel);
            return false;
        }
        const uint8_t *header = NULL;
        if (!slice(rom, song->header_pointer, 4, &header, error, error_size)) return false;
        song->track_count = header[0];
        song->block_count = header[1];
        song->priority = header[2];
        song->reverb = header[3];
        if (song->track_count > 16) {
            set_error(error, error_size, "Aria song %zu has %u tracks",
                      index, song->track_count);
            return false;
        }
    }
    table->count = FUSION_AOS_SONG_COUNT;
    if (error && error_size) error[0] = '\0';
    return true;
}

static bool text_append(Text *text, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    va_list copy;
    va_copy(copy, arguments);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(arguments);
        return false;
    }
    size_t total = text->size + (size_t)needed + 1;
    if (total > text->capacity) {
        size_t capacity = text->capacity ? text->capacity : 4096;
        while (capacity < total) capacity *= 2;
        char *grown = realloc(text->data, capacity);
        if (!grown) {
            va_end(arguments);
            return false;
        }
        text->data = grown;
        text->capacity = capacity;
    }
    vsnprintf(text->data + text->size, text->capacity - text->size,
              format, arguments);
    va_end(arguments);
    text->size += (size_t)needed;
    return true;
}

bool fusion_aos_export_sound(const FusionRom *rom, const char *output,
                             size_t *song_count,
                             char *error, size_t error_size) {
    if (!output || !song_count) {
        set_error(error, error_size, "invalid Aria sound export request");
        return false;
    }
    FusionAosSongTable table;
    if (!fusion_aos_read_song_table(rom, &table, error, error_size)) return false;
    Text text = {0};
    bool success = text_append(
        &text,
        "schema\tmetroidvania-aos-song-table-v1\n"
        "# kind\tid\theader\tplayer\tchannel\ttracks\tblocks\tpriority\treverb\n");
    for (size_t index = 0; success && index < table.count; ++index) {
        const FusionAosSong *song = &table.entries[index];
        success = text_append(&text, "song\t%zu\t0x%08X\t%u\t%u\t%u\t%u\t%u\t0x%02X\n",
                              index, song->header_pointer, song->player,
                              song->channel, song->track_count, song->block_count,
                              song->priority, song->reverb);
    }
    char path[FUSION_EXPORT_PATH_CAPACITY];
    success = success && fusion_export_ensure_directory(output, error, error_size) &&
              fusion_export_join(path, output, "song_table.tsv") &&
              fusion_export_write_file(path, (const uint8_t *)text.data,
                                       text.size, error, error_size);
    if (!success && (!error || !error[0]))
        set_error(error, error_size, "cannot export Aria song table");
    if (success) *song_count = table.count;
    free(text.data);
    return success;
}
