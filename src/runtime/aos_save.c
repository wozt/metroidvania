/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_save.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum { AOS_SAVE_TEXT_MAX = 1280 };

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
    if (!save || save->slot >= AOS_SAVE_SLOT_COUNT || save->area > 99 ||
        save->room > 999 || !spawn_pair || save->hp <= 0 ||
        save->max_hp < save->hp || save->max_hp > 9999 || save->moves > 0x1Fu ||
        save->character > 1 || (save->challenge_flags & ~0x300000u) ||
        save->experience > 99999999u || save->gold > 99999999u ||
        (save->weapon >= 59 && save->weapon != 0xFF) ||
        (save->armor >= 45 && save->armor != 0xFF) ||
        (save->accessory >= 45 && save->accessory != 0xFF))
        return false;
    for (unsigned i = 0; i < AOS_SAVE_INVENTORY; ++i)
        if (save->inventory[i] > 9) return false;
    if (save->souls_collected > 999 || save->red_soul > 28 || save->blue_soul > 13 ||
        save->yellow_soul > 18 || save->mp < -1 || save->mp > 9999)
        return false;
    for (unsigned i = 0; i < 8; ++i)
        if (save->name[i] > 0x22) return false;
    return true;
}

static uint32_t checksum(const char *text, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= (uint8_t)text[i];
        hash *= 16777619u;
    }
    return hash;
}

static int payload_v1(char *out, size_t size, const AosSaveData *save) {
    return snprintf(out, size,
                    AOS_SAVE_SCHEMA_V1 "\n"
                    "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                    "hp=%d\nmax_hp=%d\nmoves=%" PRIu32 "\nplay_frames=%" PRIu64 "\n",
                    save->slot, save->area, save->room, save->spawn_x, save->spawn_y,
                    save->hp, save->max_hp, save->moves, save->play_frames);
}

static int payload_v2(char *out, size_t size, const AosSaveData *save) {
    return snprintf(out, size,
                    AOS_SAVE_SCHEMA_V2 "\n"
                    "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                    "hp=%d\nmax_hp=%d\nmoves=%" PRIu32 "\nplay_frames=%" PRIu64 "\n"
                    "name=%u,%u,%u,%u,%u,%u,%u,%u\n"
                    "character=%u\nchallenge_flags=%" PRIu32 "\n",
                    save->slot, save->area, save->room, save->spawn_x, save->spawn_y,
                    save->hp, save->max_hp, save->moves, save->play_frames,
                    save->name[0], save->name[1], save->name[2], save->name[3],
                    save->name[4], save->name[5], save->name[6], save->name[7],
                    save->character, save->challenge_flags);
}

static int payload_v3(char *out, size_t size, const AosSaveData *save) {
    int used = payload_v2(out, size, save);
    if (used < 0 || (size_t)used >= size) return used;
    /* The v2 text with its schema line renamed and two more fields. */
    memcpy(out, AOS_SAVE_SCHEMA_V3, sizeof AOS_SAVE_SCHEMA_V3 - 1);
    int added = snprintf(out + used, size - (size_t)used,
                         "experience=%" PRIu32 "\ngold=%" PRIu32 "\n", save->experience,
                         save->gold);
    return added < 0 ? added : used + added;
}

static int payload_v4(char *out, size_t size, const AosSaveData *save) {
    int used = payload_v3(out, size, save);
    if (used < 0 || (size_t)used >= size) return used;
    memcpy(out, AOS_SAVE_SCHEMA_V4, sizeof AOS_SAVE_SCHEMA_V4 - 1);
    int added = snprintf(out + used, size - (size_t)used,
                         "weapon=%u\narmor=%u\naccessory=%u\n", save->weapon, save->armor,
                         save->accessory);
    return added < 0 ? added : used + added;
}

static int payload_v5(char *out, size_t size, const AosSaveData *save) {
    int used = payload_v4(out, size, save);
    if (used < 0 || (size_t)used + AOS_SAVE_INVENTORY + 12 >= size) return -1;
    memcpy(out, AOS_SAVE_SCHEMA_V5, sizeof AOS_SAVE_SCHEMA_V5 - 1);
    used += snprintf(out + used, size - (size_t)used, "inventory=");
    for (unsigned i = 0; i < AOS_SAVE_INVENTORY; ++i) out[used++] = (char)('0' + save->inventory[i]);
    out[used++] = '\n';
    out[used] = '\0';
    return used;
}

static int payload_v6(char *out, size_t size, const AosSaveData *save) {
    int used = payload_v5(out, size, save);
    if (used < 0 || (size_t)used + 2 * AOS_SAVE_SOUL_BYTES + 32 >= size) return -1;
    memcpy(out, AOS_SAVE_SCHEMA_V6, sizeof AOS_SAVE_SCHEMA_V6 - 1);
    used += snprintf(out + used, size - (size_t)used, "souls=");
    for (unsigned i = 0; i < AOS_SAVE_SOUL_BYTES; ++i)
        used += snprintf(out + used, size - (size_t)used, "%02x", save->souls[i]);
    used += snprintf(out + used, size - (size_t)used, "\nsouls_collected=%u\n",
                     save->souls_collected);
    return used;
}

static int payload(char *out, size_t size, const AosSaveData *save) {
    int used = payload_v6(out, size, save);
    if (used < 0 || (size_t)used + 64 >= size) return -1;
    memcpy(out, AOS_SAVE_SCHEMA, sizeof AOS_SAVE_SCHEMA - 1);
    used += snprintf(out + used, size - (size_t)used, "souls_equipped=%u,%u,%u\nmp=%d\n",
                     save->red_soul, save->blue_soul, save->yellow_soul, save->mp);
    return used;
}

void aos_save_init_new(AosSaveData *save, unsigned slot) {
    if (!save) return;
    *save = (AosSaveData){
        .slot = (uint8_t)slot,
        .area = 0,
        .room = 0,
        /* sub_08008ED0(0), called when the introduction reaches IN_GAME. */
        .spawn_x = 0x78,
        .spawn_y = 0x8D,
        .hp = 320,
        .max_hp = 320,
        .moves = 0x1F,
        .name = {0x14, 0x10, 0x0E, 0x02, 0, 0, 0, 0},
        .weapon = 0,
        .armor = 0,
        .accessory = 0xFF,
        .mp = -1,
    };
    save->inventory[AOS_SAVE_WEAPONS] = 1;
    save->inventory[AOS_SAVE_ARMOR] = 1;
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

    unsigned slot, area, room, name[8], character;
    int spawn_x, spawn_y, hp, max_hp;
    uint32_t moves, challenge_flags, expected, experience = 0, gold = 0;
    unsigned weapon = 0, armor = 0, accessory = 0xFF;
    uint64_t play_frames;
    int consumed = 0;
    bool legacy = strncmp(text, AOS_SAVE_SCHEMA_V1 "\n",
                          sizeof(AOS_SAVE_SCHEMA_V1)) == 0;
    bool v2 = strncmp(text, AOS_SAVE_SCHEMA_V2 "\n", sizeof(AOS_SAVE_SCHEMA_V2)) == 0;
    bool v3 = strncmp(text, AOS_SAVE_SCHEMA_V3 "\n", sizeof(AOS_SAVE_SCHEMA_V3)) == 0;
    bool v4 = strncmp(text, AOS_SAVE_SCHEMA_V4 "\n", sizeof(AOS_SAVE_SCHEMA_V4)) == 0;
    bool v5 = strncmp(text, AOS_SAVE_SCHEMA_V5 "\n", sizeof(AOS_SAVE_SCHEMA_V5)) == 0;
    bool v6 = strncmp(text, AOS_SAVE_SCHEMA_V6 "\n", sizeof(AOS_SAVE_SCHEMA_V6)) == 0;
    unsigned red = 0, blue = 0, yellow = 0;
    int mp = -1;
    char souls[2 * AOS_SAVE_SOUL_BYTES + 1] = {0};
    unsigned souls_collected = 0;
    char inventory[AOS_SAVE_INVENTORY + 1] = {0};
    int matched;
    if (legacy) {
        matched = sscanf(text,
                         AOS_SAVE_SCHEMA_V1 "\n"
                         "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                         "hp=%d\nmax_hp=%d\nmoves=%" SCNu32 "\nplay_frames=%" SCNu64
                         "\nchecksum=%" SCNx32 "\n%n",
                         &slot, &area, &room, &spawn_x, &spawn_y, &hp, &max_hp, &moves,
                         &play_frames, &expected, &consumed);
    } else if (v2) {
        matched = sscanf(text,
                         AOS_SAVE_SCHEMA_V2 "\n"
                         "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                         "hp=%d\nmax_hp=%d\nmoves=%" SCNu32 "\nplay_frames=%" SCNu64
                         "\nname=%u,%u,%u,%u,%u,%u,%u,%u\n"
                         "character=%u\nchallenge_flags=%" SCNu32
                         "\nchecksum=%" SCNx32 "\n%n",
                         &slot, &area, &room, &spawn_x, &spawn_y, &hp, &max_hp, &moves,
                         &play_frames, &name[0], &name[1], &name[2], &name[3],
                         &name[4], &name[5], &name[6], &name[7], &character,
                         &challenge_flags, &expected, &consumed);
    } else if (v3) {
        matched = sscanf(text,
                         AOS_SAVE_SCHEMA_V3 "\n"
                         "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                         "hp=%d\nmax_hp=%d\nmoves=%" SCNu32 "\nplay_frames=%" SCNu64
                         "\nname=%u,%u,%u,%u,%u,%u,%u,%u\n"
                         "character=%u\nchallenge_flags=%" SCNu32
                         "\nexperience=%" SCNu32 "\ngold=%" SCNu32
                         "\nchecksum=%" SCNx32 "\n%n",
                         &slot, &area, &room, &spawn_x, &spawn_y, &hp, &max_hp, &moves,
                         &play_frames, &name[0], &name[1], &name[2], &name[3],
                         &name[4], &name[5], &name[6], &name[7], &character,
                         &challenge_flags, &experience, &gold, &expected, &consumed);
    } else if (v4) {
        matched = sscanf(text,
                         AOS_SAVE_SCHEMA_V4 "\n"
                         "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                         "hp=%d\nmax_hp=%d\nmoves=%" SCNu32 "\nplay_frames=%" SCNu64
                         "\nname=%u,%u,%u,%u,%u,%u,%u,%u\n"
                         "character=%u\nchallenge_flags=%" SCNu32
                         "\nexperience=%" SCNu32 "\ngold=%" SCNu32
                         "\nweapon=%u\narmor=%u\naccessory=%u"
                         "\nchecksum=%" SCNx32 "\n%n",
                         &slot, &area, &room, &spawn_x, &spawn_y, &hp, &max_hp, &moves,
                         &play_frames, &name[0], &name[1], &name[2], &name[3],
                         &name[4], &name[5], &name[6], &name[7], &character,
                         &challenge_flags, &experience, &gold, &weapon, &armor, &accessory,
                         &expected, &consumed);
    } else if (v5 || v6 || strncmp(text, AOS_SAVE_SCHEMA "\n", sizeof(AOS_SAVE_SCHEMA)) == 0) {
        /* v5 and v6 share the fields up to the inventory. */
        matched = sscanf(text,
                         "%*[^\n]\n"
                         "slot=%u\narea=%u\nroom=%u\nspawn_x=%d\nspawn_y=%d\n"
                         "hp=%d\nmax_hp=%d\nmoves=%" SCNu32 "\nplay_frames=%" SCNu64
                         "\nname=%u,%u,%u,%u,%u,%u,%u,%u\n"
                         "character=%u\nchallenge_flags=%" SCNu32
                         "\nexperience=%" SCNu32 "\ngold=%" SCNu32
                         "\nweapon=%u\narmor=%u\naccessory=%u"
                         "\ninventory=%136[0-9]"
                         "%*[\n]%n",
                         &slot, &area, &room, &spawn_x, &spawn_y, &hp, &max_hp, &moves,
                         &play_frames, &name[0], &name[1], &name[2], &name[3],
                         &name[4], &name[5], &name[6], &name[7], &character,
                         &challenge_flags, &experience, &gold, &weapon, &armor, &accessory,
                         inventory, &consumed);
        /* The v5 checksum line, or the v6 soul lines and then it. */
        int tail = 0, read = consumed > 0 && v6
            ? sscanf(text + consumed, "souls=%246[0-9a-f]\nsouls_collected=%u"
                                      "\nchecksum=%" SCNx32 "\n%n",
                     souls, &souls_collected, &expected, &tail)
            : consumed > 0 && !v5
            ? sscanf(text + consumed, "souls=%246[0-9a-f]\nsouls_collected=%u"
                                      "\nsouls_equipped=%u,%u,%u\nmp=%d"
                                      "\nchecksum=%" SCNx32 "\n%n",
                     souls, &souls_collected, &red, &blue, &yellow, &mp, &expected, &tail)
            : consumed > 0 ? sscanf(text + consumed, "checksum=%" SCNx32 "\n%n", &expected, &tail)
                           : 0;
        if (read == (v5 ? 1 : v6 ? 3 : 7) && tail > 0) {
            matched += read;
            consumed += tail;
        } else {
            consumed = 0;
        }
    } else {
        matched = 0;
    }
    int expected_matches = legacy ? 10 : v2 ? 20 : v3 ? 22 : v4 ? 25 : v5 ? 26 : v6 ? 28 : 32;
    if (expected_matches >= 26 && matched == expected_matches &&
        (strlen(inventory) != AOS_SAVE_INVENTORY ||
         (!v5 && strlen(souls) != 2 * AOS_SAVE_SOUL_BYTES))) {
        set_error(error, error_size, "save schema is invalid");
        return false;
    }
    if (matched != expected_matches || consumed <= 0 || (size_t)consumed != used) {
        set_error(error, error_size, "save schema is invalid");
        return false;
    }
    if (slot >= AOS_SAVE_SLOT_COUNT || area > 99 || room > 999 || spawn_x < -1 ||
        spawn_x > INT16_MAX || spawn_y < -1 || spawn_y > INT16_MAX || hp < 1 ||
        hp > 9999 || max_hp < hp || max_hp > 9999 || moves > 0x1F) {
        set_error(error, error_size, "save contents are out of range");
        return false;
    }
    AosSaveData parsed;
    aos_save_init_new(&parsed, slot);
    parsed.area = (uint8_t)area;
    parsed.room = (uint16_t)room;
    parsed.spawn_x = (int16_t)spawn_x;
    parsed.spawn_y = (int16_t)spawn_y;
    parsed.hp = (int16_t)hp;
    parsed.max_hp = (int16_t)max_hp;
    parsed.moves = moves;
    parsed.play_frames = play_frames;
    if (!legacy) {
        if (character > 1) {
            set_error(error, error_size, "save contents are out of range");
            return false;
        }
        for (unsigned i = 0; i < 8; ++i) {
            if (name[i] > UINT8_MAX) {
                set_error(error, error_size, "save contents are out of range");
                return false;
            }
            parsed.name[i] = (uint8_t)name[i];
        }
        parsed.character = (uint8_t)character;
        parsed.challenge_flags = challenge_flags;
        parsed.experience = experience;
        parsed.gold = gold;
        if (weapon > 0xFF || armor > 0xFF || accessory > 0xFF) {
            set_error(error, error_size, "save contents are out of range");
            return false;
        }
        parsed.weapon = (uint8_t)weapon;
        parsed.armor = (uint8_t)armor;
        parsed.accessory = (uint8_t)accessory;
        if (!v2 && !v3 && !v4)
            for (unsigned i = 0; i < AOS_SAVE_INVENTORY; ++i)
                parsed.inventory[i] = (uint8_t)(inventory[i] - '0');
        if (!v2 && !v3 && !v4 && !v5) {
            for (unsigned i = 0; i < AOS_SAVE_SOUL_BYTES; ++i) {
                unsigned byte;
                sscanf(souls + 2 * i, "%2x", &byte);
                parsed.souls[i] = (uint8_t)byte;
            }
            if (souls_collected > 999) {
                set_error(error, error_size, "save contents are out of range");
                return false;
            }
            parsed.souls_collected = (uint16_t)souls_collected;
        }
        if (!v2 && !v3 && !v4 && !v5 && !v6) {
            if (red > 0xFF || blue > 0xFF || yellow > 0xFF || mp < -1 || mp > 9999) {
                set_error(error, error_size, "save contents are out of range");
                return false;
            }
            parsed.red_soul = (uint8_t)red;
            parsed.blue_soul = (uint8_t)blue;
            parsed.yellow_soul = (uint8_t)yellow;
            parsed.mp = (int16_t)mp;
        }
    }
    char canonical[AOS_SAVE_TEXT_MAX];
    int canonical_size = legacy ? payload_v1(canonical, sizeof canonical, &parsed)
                         : v2   ? payload_v2(canonical, sizeof canonical, &parsed)
                         : v3   ? payload_v3(canonical, sizeof canonical, &parsed)
                         : v4   ? payload_v4(canonical, sizeof canonical, &parsed)
                         : v5   ? payload_v5(canonical, sizeof canonical, &parsed)
                         : v6   ? payload_v6(canonical, sizeof canonical, &parsed)
                                : payload(canonical, sizeof canonical, &parsed);
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
