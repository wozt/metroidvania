/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_save.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum { AOS_SAVE_TEXT_MAX = 512 };

static void set_error(char *out, size_t size, const char *format, ...) {
    if (!out || size == 0) return;
    va_list args;
    va_start(args, format);
    vsnprintf(out, size, format, args);
    va_end(args);
}

static bool valid(const AosSaveData *save) {
    bool spawn_pair = save && ((save->spawn_x == -1 && save->spawn_y == -1) ||
                               (save->spawn_x >= 0 && save->spawn_y >= 0));
    return save && save->slot < AOS_SAVE_SLOT_COUNT && save->area <= 99 && save->room <= 999 &&
           spawn_pair && save->hp > 0 &&
           save->max_hp >= save->hp && save->max_hp <= 9999 && save->moves <= 0x1Fu;
}

static uint32_t checksum(const char *text, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= (uint8_t)text[i];
        hash *= 16777619u;
    }
    return hash;
}

static int payload(char *out, size_t size, const AosSaveData *save) {
    return snprintf(out, size,
                    AOS_SAVE_SCHEMA "\n"
                    "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                    "hp=%d\nmax_hp=%d\nmoves=%" PRIu32 "\nplay_frames=%" PRIu64 "\n",
                    save->slot, save->area, save->room, save->spawn_x, save->spawn_y,
                    save->hp, save->max_hp, save->moves, save->play_frames);
}

void aos_save_init_new(AosSaveData *save, unsigned slot) {
    if (!save) return;
    *save = (AosSaveData){
        .slot = (uint8_t)slot,
        .area = 0,
        .room = 0,
        .spawn_x = -1,
        .spawn_y = -1,
        .hp = 320,
        .max_hp = 320,
        .moves = 0x1F,
    };
}

bool aos_save_load(const char *path, AosSaveData *save, char *error, size_t error_size) {
    if (!path || !save) {
        set_error(error, error_size, "invalid save arguments");
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error(error, error_size, "cannot open %s: %s", path, strerror(errno));
        return false;
    }
    char text[AOS_SAVE_TEXT_MAX];
    size_t used = fread(text, 1, sizeof text - 1, file);
    bool read_ok = !ferror(file) && feof(file);
    fclose(file);
    text[used] = '\0';
    if (!read_ok) {
        set_error(error, error_size, "save file is too large or unreadable");
        return false;
    }

    unsigned slot, area, room;
    int spawn_x, spawn_y, hp, max_hp;
    uint32_t moves, expected;
    uint64_t play_frames;
    int consumed = 0;
    int matched = sscanf(text,
                         AOS_SAVE_SCHEMA "\n"
                         "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                         "hp=%d\nmax_hp=%d\nmoves=%" SCNu32 "\nplay_frames=%" SCNu64
                         "\nchecksum=%" SCNx32 "\n%n",
                         &slot, &area, &room, &spawn_x, &spawn_y, &hp, &max_hp, &moves,
                         &play_frames, &expected, &consumed);
    if (matched != 10 || consumed <= 0 || (size_t)consumed != used) {
        set_error(error, error_size, "save schema is invalid");
        return false;
    }
    if (slot >= AOS_SAVE_SLOT_COUNT || area > 99 || room > 999 || spawn_x < -1 ||
        spawn_x > INT16_MAX || spawn_y < -1 || spawn_y > INT16_MAX || hp < 1 ||
        hp > 9999 || max_hp < hp || max_hp > 9999 || moves > 0x1F) {
        set_error(error, error_size, "save contents are out of range");
        return false;
    }
    AosSaveData parsed = {(uint8_t)slot, (uint8_t)area, (uint16_t)room,
                          (int16_t)spawn_x, (int16_t)spawn_y, (int16_t)hp,
                          (int16_t)max_hp, moves, play_frames};
    char canonical[AOS_SAVE_TEXT_MAX];
    int canonical_size = payload(canonical, sizeof canonical, &parsed);
    if (!valid(&parsed) || canonical_size < 0 || (size_t)canonical_size >= sizeof canonical ||
        checksum(canonical, (size_t)canonical_size) != expected) {
        set_error(error, error_size, "save contents or checksum are invalid");
        return false;
    }
    *save = parsed;
    if (error && error_size) error[0] = '\0';
    return true;
}

bool aos_save_store(const char *path, const AosSaveData *save, char *error, size_t error_size) {
    if (!path || !valid(save)) {
        set_error(error, error_size, "save contents are invalid");
        return false;
    }
    char text[AOS_SAVE_TEXT_MAX];
    int used = payload(text, sizeof text, save);
    if (used < 0 || (size_t)used >= sizeof text) {
        set_error(error, error_size, "save payload is too large");
        return false;
    }
    int added = snprintf(text + used, sizeof text - (size_t)used, "checksum=%08" PRIx32 "\n",
                         checksum(text, (size_t)used));
    if (added < 0 || (size_t)(used + added) >= sizeof text) {
        set_error(error, error_size, "save payload is too large");
        return false;
    }
    char temporary[1024];
    if (snprintf(temporary, sizeof temporary, "%s.tmp", path) >= (int)sizeof temporary) {
        set_error(error, error_size, "save path is too long");
        return false;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "cannot create %s: %s", temporary, strerror(errno));
        return false;
    }
    size_t total = (size_t)(used + added);
    bool ok = fwrite(text, 1, total, file) == total;
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        remove(temporary);
        set_error(error, error_size, "cannot write %s", temporary);
        return false;
    }
    if (rename(temporary, path) != 0) {
        remove(temporary);
        set_error(error, error_size, "cannot replace %s: %s", path, strerror(errno));
        return false;
    }
    if (error && error_size) error[0] = '\0';
    return true;
}
