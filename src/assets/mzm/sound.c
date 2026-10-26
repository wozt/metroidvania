/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission sound data of the runtime's sound engine (formerly
 * scripts/mzm_audio.py). The engine (src/audio.c, src/audio_wrappers.c,
 * asm/audio_internal.s, asm/soundcode.s, ported in src/runtime/mzm_audio.c)
 * reads the songs, track data, voice groups, DirectSound samples and
 * programmable wave samples through ROM addresses, so the ROM range of the
 * sound objects (the sound/ .rodata sections of the reference link map,
 * sSoundDataEntries included) is copied as sound_bank.bin: "MZMSND01", the
 * GBA base address and the size (two little-endian u32), then the bytes.
 * sound_tables.tsv holds the engine tables of src/data/audio.c, read at their
 * link-map symbols, the sUnk_808cc4d LFO window, and the music aliases of
 * DetermineNewMusicTrack. */
#include "fusion/assets/mzm.h"

#include "export.h"
#include "../../runtime/mzm_sound_ids.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ROM_BASE = 0x08000000, SOUND_ENTRY_SIZE = 8 };

/* The engine tables in the order the runtime reads them, with their element
 * width and their element count in src/data/audio.c. */
static const struct {
    const char *name;
    unsigned width;
    size_t count;
} tables[] = {
    {"sUnk_808cad0", 2, 128}, {"sClockTable", 1, 49}, {"sNoiseTable", 1, 60},
    {"sCgb3Vol", 1, 16}, {"sUnk_808cc4d", 1, 19}, {"sNativeSampleRate", 4, 13},
    {"sMusicPitchData", 4, 13}, {"sPcmSamplesPerVBlankTable", 2, 13},
    {"sArray_808cce2", 1, 512}, {"sArray_808cee2", 1, 10},
};

/* DetermineNewMusicTrack (src/music_wrappers.c), then the track of
 * CheckSetNewMusicTrack for a room without music. */
static const unsigned music_aliases[][2] = {
    {MZM_MUSIC_CHOZODIA_SURFACE, MZM_MUSIC_CHOZO_RUINS},
    {MZM_MUSIC_MAP_ROOM_2, MZM_MUSIC_MAP_ROOM},
    {MZM_MUSIC_SAVE_ELEVATOR_ROOM_2, MZM_MUSIC_SAVE_ELEVATOR_ROOM},
    {MZM_MUSIC_BEFORE_RUINS_TEST_ROOM, MZM_MUSIC_BEFORE_RUINS_TEST_UNUSED},
    {MZM_MUSIC_STEALTH_2, MZM_MUSIC_STEALTH},
};

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

static void set_error(char *error, size_t error_size, const char *format, ...) {
    if (!error || !error_size) return;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
}

static bool text_append(Text *text, const char *format, ...) {
    va_list arguments, copy;
    va_start(arguments, format);
    va_copy(copy, arguments);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(arguments);
        return false;
    }
    if (text->size + (size_t)needed + 1 > text->capacity) {
        size_t next = text->capacity ? text->capacity : 4096;
        while (next < text->size + (size_t)needed + 1) next *= 2;
        char *grown = realloc(text->data, next);
        if (!grown) {
            va_end(arguments);
            return false;
        }
        text->data = grown;
        text->capacity = next;
    }
    vsnprintf(text->data + text->size, text->capacity - text->size, format, arguments);
    va_end(arguments);
    text->size += (size_t)needed;
    return true;
}

static bool is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

/* " .rodata  0x08xxxxxx  0xSIZE  sound/....o" lines. */
static bool rodata_line(const char *line, size_t length, uint32_t *start, uint32_t *size) {
    static const char prefix[] = " .rodata";
    if (length < sizeof prefix || strncmp(line, prefix, sizeof prefix - 1)) return false;
    const char *cursor = line + sizeof prefix - 1, *end = line + length;
    if (cursor >= end || (*cursor != ' ' && *cursor != '\t')) return false;
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    if (end - cursor < 12 || strncmp(cursor, "0x08", 4)) return false;
    for (int digit = 2; digit < 10; ++digit)
        if (!is_hex(cursor[digit])) return false;
    *start = (uint32_t)strtoul(cursor + 2, NULL, 16);
    cursor += 10;
    if (cursor >= end || (*cursor != ' ' && *cursor != '\t')) return false;
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    if (end - cursor < 3 || strncmp(cursor, "0x", 2)) return false;
    const char *digits = cursor + 2;
    cursor = digits;
    while (cursor < end && is_hex(*cursor)) ++cursor;
    if (cursor == digits || cursor >= end || (*cursor != ' ' && *cursor != '\t')) return false;
    *size = (uint32_t)strtoul(digits, NULL, 16);
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    const char *object = cursor;
    while (cursor < end && *cursor != ' ' && *cursor != '\t') ++cursor;
    return cursor == end && object + 8 < end && !strncmp(object, "sound/", 6) &&
           !strncmp(end - 2, ".o", 2);
}

/* "   0x08xxxxxx   NAME" lines. */
static bool symbol_address(const char *map, size_t size, const char *name, uint32_t *address) {
    size_t name_length = strlen(name);
    for (const char *line = map; line < map + size;) {
        const char *end = memchr(line, '\n', (size_t)(map + size - line));
        if (!end) end = map + size;
        const char *cursor = line;
        if (cursor < end && (*cursor == ' ' || *cursor == '\t')) {
            while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
            if (end - cursor > 10 && !strncmp(cursor, "0x", 2)) {
                bool hex = true;
                for (int digit = 2; digit < 10; ++digit) hex = hex && is_hex(cursor[digit]);
                const char *gap = cursor + 10, *label = gap;
                while (label < end && (*label == ' ' || *label == '\t')) ++label;
                if (hex && label > gap && (size_t)(end - label) == name_length &&
                    !strncmp(label, name, name_length)) {
                    *address = (uint32_t)strtoul(cursor + 2, NULL, 16);
                    return true;
                }
            }
        }
        line = end + 1;
    }
    return false;
}

static const uint8_t *rom_at(const FusionRom *rom, uint32_t address, size_t size) {
    if (address < ROM_BASE || address - ROM_BASE > rom->size ||
        size > rom->size - (address - ROM_BASE))
        return NULL;
    return rom->data + (address - ROM_BASE);
}

static bool write_text(const char *output, const char *name, const uint8_t *data, size_t size,
                       char *error, size_t error_size) {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_join(path, output, name)) {
        set_error(error, error_size, "sound output path is too long");
        return false;
    }
    return fusion_export_write_file(path, data, size, error, error_size);
}

bool fusion_mzm_export_sound(const FusionRom *rom, const char *link_map, size_t map_size,
                             const char *output, FusionMzmSoundExportSummary *summary,
                             char *error, size_t error_size) {
    if (summary) *summary = (FusionMzmSoundExportSummary){0};
    if (!rom || rom->game != FUSION_ASSET_GAME_METROID || !link_map || !output || !summary) {
        set_error(error, error_size, "invalid sound export request");
        return false;
    }
    uint32_t start = UINT32_MAX, end = 0, entries = 0, entries_size = 0;
    bool found_entries = false, any = false;
    if (!symbol_address(link_map, map_size, "sSoundDataEntries", &entries)) {
        set_error(error, error_size, "sSoundDataEntries not in the link map");
        return false;
    }
    for (const char *line = link_map; line < link_map + map_size;) {
        const char *line_end = memchr(line, '\n', (size_t)(link_map + map_size - line));
        if (!line_end) line_end = link_map + map_size;
        uint32_t section = 0, size = 0;
        if (rodata_line(line, (size_t)(line_end - line), &section, &size)) {
            any = true;
            if (section < start) start = section;
            if (section + size > end) end = section + size;
            if (section == entries && !found_entries) {
                entries_size = size;
                found_entries = true;
            }
        }
        line = line_end + 1;
    }
    if (!any) {
        set_error(error, error_size, "no sound/ .rodata sections in the link map");
        return false;
    }
    if (!found_entries || entries < start || entries >= end) {
        set_error(error, error_size, "sSoundDataEntries outside the sound range");
        return false;
    }
    const uint8_t *blob = rom_at(rom, start, end - start);
    if (!blob) {
        set_error(error, error_size, "sound range outside the ROM");
        return false;
    }
    size_t bank_size = 16 + (end - start);
    uint8_t *bank = malloc(bank_size);
    Text text = {0};
    bool ok = bank != NULL;
    if (ok) {
        memcpy(bank, "MZMSND01", 8);
        uint32_t header[2] = {start, end - start};
        for (int field = 0; field < 2; ++field)
            for (int byte = 0; byte < 4; ++byte)
                bank[8 + field * 4 + byte] = (uint8_t)(header[field] >> (8 * byte));
        memcpy(bank + 16, blob, end - start);
    }
    ok = ok && text_append(&text, "schema\tmetroidvania-mzm-sound-v1\nentries\t%u\t%u\n",
                           entries, entries_size / SOUND_ENTRY_SIZE);
    uint32_t window_symbol = 0;
    for (size_t table = 0; ok && table < sizeof tables / sizeof tables[0]; ++table) {
        uint32_t address = 0;
        /* GetNoteDelay reads one entry past sUnk_808cad0 for key 127. */
        size_t count = tables[table].count + (table == 0 ? 1 : 0);
        const uint8_t *data = symbol_address(link_map, map_size, tables[table].name, &address)
            ? rom_at(rom, address, count * tables[table].width) : NULL;
        if (!data) {
            set_error(error, error_size, "%s not in the link map", tables[table].name);
            ok = false;
            break;
        }
        if (!strcmp(tables[table].name, "sUnk_808cc4d")) window_symbol = address;
        ok = text_append(&text, "table\t%s\t", tables[table].name);
        for (size_t index = 0; ok && index < count; ++index) {
            uint32_t value = 0;
            for (unsigned byte = 0; byte < tables[table].width; ++byte)
                value |= (uint32_t)data[index * tables[table].width + byte] << (8 * byte);
            ok = text_append(&text, "%s%u", index ? "," : "", value);
        }
        ok = ok && text_append(&text, "\n");
    }
    /* unk_1ccc indexes sUnk_808cc4d with the LFO phase (-64 to 64). */
    const uint8_t *window = ok ? rom_at(rom, window_symbol - 64, 129) : NULL;
    if (ok && !window) {
        set_error(error, error_size, "sUnk_808cc4d window outside the ROM");
        ok = false;
    }
    ok = ok && text_append(&text, "window\tsUnk_808cc4d\t");
    for (size_t index = 0; ok && index < 129; ++index)
        ok = text_append(&text, "%s%u", index ? "," : "", window[index]);
    ok = ok && text_append(&text, "\n");
    for (size_t alias = 0; ok && alias < sizeof music_aliases / sizeof music_aliases[0]; ++alias)
        ok = text_append(&text, "alias\t%u\t%u\n", music_aliases[alias][0],
                         music_aliases[alias][1]);
    ok = ok && text_append(&text, "default\t%u\n", (unsigned)MZM_SOUND_A9);
    if (!ok && error && !error[0]) set_error(error, error_size, "cannot allocate the sound export");
    ok = ok && fusion_export_ensure_directory(output, error, error_size) &&
         write_text(output, "sound_bank.bin", bank, bank_size, error, error_size) &&
         write_text(output, "sound_tables.tsv", (const uint8_t *)text.data, text.size, error,
                    error_size);
    if (ok) *summary = (FusionMzmSoundExportSummary){start, end - start,
                                                     entries_size / SOUND_ENTRY_SIZE};
    free(bank);
    free(text.data);
    return ok;
}
