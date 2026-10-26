/* SPDX-License-Identifier: GPL-3.0-only */
#define _DEFAULT_SOURCE
#include "object_catalog.h"

#include "fusion/assets/rom.h"
#include "generated.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

enum {
    GBA_BASE = 0x08000000,
    ARIA_ENEMY_TABLE = 0x080E9644,
    ARIA_ENEMY_ENTRY = 0x24,
    ARIA_ENEMY_COUNT = 0x71,
    ARIA_TEXT_POINTERS = 0x08506B38,
    ARIA_ICON_PALETTE = 0x082099FC,
};

static const char aria_rom_path[] = "roms/Castlevania - Aria of Sorrow (USA).gba";

static const char *const aria_kind_names[] = {
    "Nothing", "Enemy", "Special object", "Generic candle", "Pickup", "Hard-mode pickup",
    "All-souls reward"};

/* Behavioral constructors, audited against DSVEdit's Aria entity notes. */
static const char *const aria_special_objects[0x38] = {
    "Wooden door", "Pushable crates", "Boss door", "Boss Rush boss door",
    "Forced-entry boss door", "Arena door", "Darkness door", "Room visual effect",
    "Breakable wall", "Event-destroyed wall", "Boss-death-conditional enemy",
    "Event-conditional enemy", "Waterfall current", "Bell", "Destructible prop", "Flame",
    "Boss Rush reward", "Boss Rush start", "Background visual", "Ball and chain", "Boat",
    "Chaotic Realm moon portal", "Chaos portal", "Elevator", "Chest", "Patrolling skull",
    "Unused special object 1A", "Conveyor reversal button", "Save point",
    "Save/warp room walls", "Unused special object 1E", "Warp point", "Story event actor",
    "Falling spike trap", "Arena D-pad puzzle", "Bathroom water flow", "Bathroom mist",
    "Legion background members", "Arena background statue", "Arena statue hand",
    "Arena statue foot", "Vertical moving platform", "Horizontal moving platform",
    "Floating Garden fountain", "Floating Garden clouds", "Throne stairway statue",
    "Skull-eye flames", "Julius fight object 2F", "Julius fight object 30",
    "Julius fight object 31", "Crumbling platform", "Waterfall splash", "Swinging pendulum",
    "Metal gate and button", "Cog", "Demon-head gate"};

static const char *const aria_pickups[] = {
    "Empty pickup", "Money pickup", "Consumable pickup", "Weapon pickup",
    "Armor/accessory pickup", "Red soul candle", "Blue soul candle", "Yellow soul candle",
    "Ability soul candle"};

/* DSVEdit aos_constants.rb: first text id, count and first item id. */
static const struct {
    int subtype, first, count, start;
} item_ranges[] = {
    {2, 0x5B, 32, 0}, {3, 0x7B, 59, 0}, {4, 0xB6, 45, 0}, {5, 0xE3, 55, 1},
    {6, 0x11B, 24, 1}, {7, 0x133, 35, 1}, {8, 0x156, 6, 0}};
static const char *const family_names[] = {
    "", "Money", "Consumable", "Weapon", "Armor / accessory", "Red soul", "Blue soul",
    "Yellow soul", "Ability soul"};
static const char *const modes[][2] = {
    {"pickup", "Normal"}, {"hard-mode-pickup", "Hard Mode"}, {"all-souls-reward", "All Souls"}};

/* DSVEdit AOS_SPECIAL_CHARACTERS. */
static const struct {
    unsigned char code;
    const char *text;
} special_characters[] = {
    {0x90, "Œ"}, {0x91, "œ"}, {0xA7, "§"}, {0xAA, "ᵃ"}, {0xAB, "«"}, {0xBA, "°"},
    {0xBB, "»"}, {0xC0, "À"}, {0xC1, "Á"}, {0xC2, "Â"}, {0xC4, "Ä"}, {0xC7, "Ç"},
    {0xC8, "È"}, {0xC9, "É"}, {0xCA, "Ê"}, {0xCB, "Ë"}, {0xD6, "Ö"}, {0xD8, "Œ"},
    {0xDB, "Û"}, {0xDC, "Ü"}, {0xDF, "ß"}, {0xE0, "à"}, {0xE2, "â"}, {0xE4, "ä"},
    {0xE7, "ç"}, {0xE8, "è"}, {0xE9, "é"}, {0xEA, "ê"}, {0xEB, "ë"}, {0xEE, "î"},
    {0xEF, "ï"}, {0xF4, "ô"}, {0xF6, "ö"}, {0xF9, "ù"}, {0xFB, "û"}, {0xFC, "ü"}};

/* The icon graphics pages, palette and item tables (DSVEdit, USA). */
static const uint32_t icon_banks[] = {0x081C5E00, 0x081C7E04, 0x081C9E08};
static const struct {
    int subtype;
    uint32_t pointer;
    int count, stride;
} item_tables[] = {{2, 0x08505B3C, 32, 16}, {3, 0x08505D3C, 59, 28}, {4, 0x085063B0, 45, 20}};

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static bool open_aria(const char *repository, FusionRom *rom, FebError *error) {
    char path[FEB_PATH_CAPACITY], detail[256] = "";
    if (!feb_join(path, repository, aria_rom_path) ||
        !fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, rom, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", detail[0] ? detail : "missing Aria ROM");
    return true;
}

static bool rom_slice(const FusionRom *rom, uint32_t pointer, size_t size, size_t *offset) {
    if (pointer < GBA_BASE || pointer - GBA_BASE > rom->size || size > rom->size - (pointer - GBA_BASE))
        return false;
    *offset = pointer - GBA_BASE;
    return true;
}

/* _label(): PSPRITE_X_Y as "X Y" in title case, digits apart, roman II/III. */
static void mzm_label(const char *symbol, char *out, size_t size) {
    const char *name = symbol + strlen("PSPRITE_");
    char title[128];
    size_t used = 0;
    bool previous_cased = false;
    for (const char *c = name; *c && used + 1 < sizeof title; ++c) {
        char value = *c == '_' ? ' ' : *c;
        if (isalpha((unsigned char)value)) {
            value = previous_cased ? (char)tolower((unsigned char)value)
                                   : (char)toupper((unsigned char)value);
            previous_cased = true;
        } else {
            previous_cased = false;
        }
        title[used++] = value;
    }
    title[used] = '\0';
    char spaced[256];
    used = 0;
    for (size_t index = 0; title[index] && used + 2 < sizeof spaced; ++index) {
        if (index && isalpha((unsigned char)title[index - 1]) && isdigit((unsigned char)title[index]))
            spaced[used++] = ' ';
        spaced[used++] = title[index];
    }
    spaced[used] = '\0';
    /* .replace(' Ii', ' II').replace(' Iii', ' III') */
    char replaced[256];
    used = 0;
    for (size_t index = 0; spaced[index] && used + 3 < sizeof replaced;) {
        if (!strncmp(spaced + index, " Ii", 3)) {
            memcpy(replaced + used, " II", 3);
            used += 3;
            index += 3;
        } else {
            replaced[used++] = spaced[index++];
        }
    }
    replaced[used] = '\0';
    used = 0;
    char *write = out;
    for (size_t index = 0; replaced[index] && used + 4 < size;) {
        if (!strncmp(replaced + index, " Iii", 4)) {
            memcpy(write + used, " III", 4);
            used += 4;
            index += 4;
        } else {
            write[used++] = replaced[index++];
        }
    }
    write[used] = '\0';
}

static bool zero_text(const char *value) {
    return !strcmp(value, "0") || !strcmp(value, "0u") || !strcmp(value, "0U") ||
           !strcmp(value, "0x0") || !strcmp(value, "0x00");
}

/* mzm_sprite_kind(): the conservative native sprite taxonomy. */
static const char *mzm_role(const char *symbol, const char *health, const char *damage) {
    const char *name = symbol + strlen("PSPRITE_");
    const char *role = "OTHER";
    static const char *const objects[] = {
        "BOMB_BLOCK", "MISSILE_BLOCK", "BEAM_BLOCK", "BREAKABLE_BLOCK", "CHOZO_STATUE",
        "MAP_STATION", "SAVE_PLATFORM", "SAVE_STATION", "ELEVATOR", "GUNSHIP", "BANNER",
        "CONSOLE", "TERMINAL"};
    static const char *const items[] = {
        "MORPH_BALL", "POWER_GRIP", "BOMBS", "LONG_BEAM", "ICE_BEAM", "WAVE_BEAM",
        "PLASMA_BEAM", "CHARGE_BEAM", "VARIA_SUIT", "GRAVITY_SUIT", "SPACE_JUMP", "HIGH_JUMP",
        "SCREW_ATTACK", "SPEED_BOOSTER", "MISSILE_EXPANSION", "SUPER_MISSILE_EXPANSION",
        "POWER_BOMB_EXPANSION"};
    static const char *const enemies[] = {
        "ZOOMER", "RIPPER", "SKREE", "METROID", "KRAID", "RIDLEY", "SPACE_PIRATE", "WAVER",
        "RINKA", "GEEGA", "ZEB", "DESSGEEGA", "SIDEHOPPER", "MOTHER_BRAIN"};
    size_t length = strlen(name);
    if (strstr(name, "UNUSED") || strstr(name, "UNKNOWN")) {
        role = "OTHER";
    } else if (strstr(name, "DOOR") || (length >= 5 && !strcmp(name + length - 5, "_GATE"))) {
        role = "DOOR";
    } else {
        bool matched = false;
        for (size_t index = 0; !matched && index < COUNT(objects); ++index)
            if (strstr(name, objects[index])) matched = true;
        if (matched) {
            role = "OBJECT";
        } else {
            bool item = strstr(name, "TANK") || strstr(name, "PICKUP") || strstr(name, "ITEM_DROP");
            for (size_t index = 0; !item && index < COUNT(items); ++index)
                if (!strcmp(name, items[index])) item = true;
            if (item) {
                role = "ITEM";
            } else if (health && damage && health[0] && !zero_text(health) && damage[0] &&
                       !zero_text(damage)) {
                role = "ENEMY";
            } else {
                for (size_t index = 0; index < COUNT(enemies); ++index)
                    if (!strncmp(name, enemies[index], strlen(enemies[index]))) role = "ENEMY";
            }
        }
    }
    return role;
}

const char *feb_mzm_sprite_kind(const char *symbol) {
    return mzm_role(symbol, NULL, NULL);
}

static const char *mzm_category(const char *symbol, const char *health, const char *damage) {
    const char *role = mzm_role(symbol, health, damage);
    if (!strcmp(role, "ENEMY")) return "Enemy / actor";
    if (!strcmp(role, "ITEM")) return "Item / pickup";
    if (!strcmp(role, "DOOR")) return "Door / gate sprite";
    if (!strcmp(role, "OBJECT")) return "World object";
    return "Unclassified native sprite";
}

typedef struct {
    char symbol[96];
    char health[256], damage[256], weakness[256];
    bool has_health, has_damage, has_weakness;
} Stats;

static char *read_source(const char *repository, const char *relative, FebError *error) {
    char path[FEB_PATH_CAPACITY];
    char *text = NULL;
    if (!feb_join(path, repository, relative) ||
        !feb_read_text(path, 64 * 1024 * 1024, &text, NULL)) {
        feb_fail(error, FEB_EXIT_OPERATION, "[Errno 2] No such file or directory: '%s'", path);
        return NULL;
    }
    return text;
}

/* One "[SPRITE_STATS_X] = value" line of a stat block. */
static void stat_line(Stats *stats, const char *line, size_t length) {
    const char *cursor = line, *end = line + length;
    while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
    if (cursor >= end || *cursor != '[') return;
    const char *key = ++cursor;
    while (cursor < end && (isupper((unsigned char)*cursor) || isdigit((unsigned char)*cursor) ||
                            *cursor == '_'))
        ++cursor;
    if (cursor >= end || *cursor != ']' || strncmp(key, "SPRITE_STATS_", 13)) return;
    size_t key_length = (size_t)(cursor - key);
    ++cursor;
    while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
    if (cursor >= end || *cursor != '=') return;
    ++cursor;
    while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
    const char *value_end = end;
    while (value_end > cursor && isspace((unsigned char)value_end[-1])) --value_end;
    if (value_end <= cursor) return;
    /* .rstrip(',').strip() */
    while (value_end > cursor && value_end[-1] == ',') --value_end;
    while (value_end > cursor && isspace((unsigned char)value_end[-1])) --value_end;
    char value[256];
    snprintf(value, sizeof value, "%.*s", (int)(value_end - cursor), cursor);
    if (key_length == strlen("SPRITE_STATS_HEALTH") && !strncmp(key, "SPRITE_STATS_HEALTH", key_length)) {
        snprintf(stats->health, sizeof stats->health, "%s", value);
        stats->has_health = true;
    } else if (key_length == strlen("SPRITE_STATS_DAMAGE") &&
               !strncmp(key, "SPRITE_STATS_DAMAGE", key_length)) {
        snprintf(stats->damage, sizeof stats->damage, "%s", value);
        stats->has_damage = true;
    } else if (key_length == strlen("SPRITE_STATS_WEAKNESSES") &&
               !strncmp(key, "SPRITE_STATS_WEAKNESSES", key_length)) {
        snprintf(stats->weakness, sizeof stats->weakness, "%s", value);
        stats->has_weakness = true;
    }
}

bool feb_mzm_object_records(const char *repository, FusionJson *records, FebError *error) {
    char *header = read_source(repository, "third_party/mzm/include/constants/sprite.h", error);
    if (!header) return false;
    const char *start = strstr(header, "MAKE_ENUM(u8, PrimarySprite)");
    const char *open = start ? strchr(start, '{') : NULL;
    const char *close = open ? strstr(open, "\n};") : NULL;
    if (!close) {
        free(header);
        return feb_fail(error, FEB_EXIT_OPERATION, "MZM PrimarySprite enum not found");
    }
    char (*symbols)[96] = calloc(512, sizeof *symbols);
    size_t count = 0;
    bool has_count = false;
    for (const char *line = open + 1; symbols && line < close;) {
        const char *end = memchr(line, '\n', (size_t)(close - line));
        if (!end) end = close;
        const char *cursor = line;
        while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
        if (!strncmp(cursor, "PSPRITE_", 8)) {
            const char *name = cursor;
            while (cursor < end && (isupper((unsigned char)*cursor) ||
                                    isdigit((unsigned char)*cursor) || *cursor == '_'))
                ++cursor;
            const char *after = cursor;
            while (after < end && isspace((unsigned char)*after)) ++after;
            if (after < end && *after == ',' && count < 512)
                snprintf(symbols[count++], sizeof symbols[0], "%.*s", (int)(cursor - name), name);
        }
        line = end + 1;
    }
    for (const char *c = open; c + 12 <= close; ++c)
        if (!strncmp(c, "PSPRITE_COUNT", 13)) has_count = true;
    free(header);
    if (!symbols) return feb_oom(error);
    if (count < 200 || !has_count) {
        free(symbols);
        return feb_fail(error, FEB_EXIT_OPERATION, "incomplete MZM PrimarySprite enum");
    }
    char *source = read_source(repository, "third_party/mzm/src/data/sprite_data.c", error);
    if (!source) {
        free(symbols);
        return false;
    }
    Stats *stats = calloc(count, sizeof *stats);
    /* STAT_BLOCK_RE: [PSPRITE_X] = { ... up to the first "\n<space>}". */
    for (const char *cursor = source; stats && (cursor = strstr(cursor, "[PSPRITE_")) != NULL;) {
        const char *name = cursor + 1, *name_end = name;
        while (isupper((unsigned char)*name_end) || isdigit((unsigned char)*name_end) ||
               *name_end == '_')
            ++name_end;
        const char *after = name_end;
        cursor = name_end;
        if (*after != ']') continue;
        ++after;
        while (isspace((unsigned char)*after)) ++after;
        if (*after != '=') continue;
        ++after;
        while (isspace((unsigned char)*after)) ++after;
        if (*after != '{') continue;
        const char *body = after + 1, *finish = NULL;
        for (const char *probe = body; *probe; ++probe) {
            if (*probe != '\n') continue;
            const char *next = probe + 1;
            while (*next == ' ' || *next == '\t' || *next == '\r' || *next == '\f' ||
                   *next == '\v' || *next == '\n') {
                if (*next == '\n') break;
                ++next;
            }
            if (*next == '}') {
                finish = probe;
                break;
            }
        }
        if (!finish) continue;
        char symbol[96];
        snprintf(symbol, sizeof symbol, "%.*s", (int)(name_end - name), name);
        for (size_t index = 0; index < count; ++index) {
            if (strcmp(symbols[index], symbol)) continue;
            Stats fresh = {0};
            for (const char *line = body; line < finish;) {
                const char *end = memchr(line, '\n', (size_t)(finish - line));
                if (!end) end = finish;
                stat_line(&fresh, line, (size_t)(end - line));
                line = end + 1;
            }
            stats[index] = fresh;
        }
        cursor = finish;
    }
    free(source);
    bool ok = stats != NULL;
    if (!ok) feb_oom(error);
    for (size_t index = 0; ok && index < count; ++index) {
        const Stats *values = &stats[index];
        char name[256], summary[1024];
        mzm_label(symbols[index], name, sizeof name);
        snprintf(summary, sizeof summary, "health=%s; damage=%s; weaknesses=%s",
                 values->has_health ? values->health : "not decoded",
                 values->has_damage ? values->damage : "not decoded",
                 values->has_weakness ? values->weakness : "not decoded");
        FusionJson record = fusion_json_object();
        ok = feb_put_string(&record, "world", "Zero Mission") &&
             feb_put_int(&record, "native_id", (int64_t)index) &&
             feb_put_string(&record, "native_type", symbols[index]) &&
             feb_put_string(&record, "name", name) &&
             feb_put_string(&record, "category",
                            mzm_category(symbols[index],
                                         values->has_health ? values->health : NULL,
                                         values->has_damage ? values->damage : NULL)) &&
             feb_put_string(&record, "summary", summary) &&
             feb_put(&record, "placements", fusion_json_null()) &&
             feb_put_bool(&record, "editable", false) && fusion_json_push(records, record);
        if (!ok) feb_oom(error);
    }
    free(stats);
    free(symbols);
    return ok;
}

/* _camel_label(): "SkeletonBlaze" as "Skeleton Blaze". */
static void camel_label(const char *symbol, char *out, size_t size) {
    size_t used = 0;
    for (size_t index = 0; symbol[index] && used + 2 < size; ++index) {
        char c = symbol[index];
        if (index && isupper((unsigned char)c) &&
            (islower((unsigned char)symbol[index - 1]) || isdigit((unsigned char)symbol[index - 1])))
            out[used++] = ' ';
        out[used++] = c;
    }
    out[used] = '\0';
    if (!strcmp(out, "Belmont")) snprintf(out, size, "Julius Belmont");
    else if (!strcmp(out, "Graham")) snprintf(out, size, "Graham Jones");
}

static int compare_strings(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

typedef struct {
    uint32_t address;
    char name[96];
} AsmSymbol;

/* parse_aria_symbols() and decode_aria_enemy_names(). */
bool feb_aria_enemy_names(const char *repository, char (*names)[96], FebError *error) {
    FusionRom rom = {0};
    if (!open_aria(repository, &rom, error)) return false;
    char folder[FEB_PATH_CAPACITY];
    if (!feb_join(folder, repository, "third_party/cvaos/asm/code") || !feb_is_directory(folder)) {
        fusion_rom_close(&rom);
        return feb_fail(error, FEB_EXIT_OPERATION, "missing pinned Aria code directory: %s", folder);
    }
    DIR *directory = opendir(folder);
    char **files = NULL;
    size_t file_count = 0;
    struct dirent *entry;
    while (directory && (entry = readdir(directory)) != NULL) {
        size_t length = strlen(entry->d_name);
        if (length < 2 || strcmp(entry->d_name + length - 2, ".s")) continue;
        char **grown = realloc(files, (file_count + 1) * sizeof *grown);
        if (!grown) break;
        files = grown;
        files[file_count++] = strdup(entry->d_name);
    }
    if (directory) closedir(directory);
    qsort(files, file_count, sizeof *files, compare_strings);
    AsmSymbol *symbols = NULL;
    size_t symbol_count = 0, capacity = 0;
    bool ok = file_count > 0 ||
              feb_fail(error, FEB_EXIT_OPERATION, "empty pinned Aria code directory: %s", folder);
    for (size_t file = 0; ok && file < file_count; ++file) {
        char path[FEB_PATH_CAPACITY + 256];
        snprintf(path, sizeof path, "%s/%s", folder, files[file]);
        char *text = NULL;
        if (!feb_read_text(path, 256 * 1024 * 1024, &text, NULL)) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "cannot read %s", path);
            break;
        }
        /* ^name:\s*@\s*0xADDRESS */
        for (char *line = text; ok && line && *line;) {
            char *end = strchr(line, '\n');
            if (end) *end = '\0';
            char *cursor = line;
            if (isalpha((unsigned char)*cursor) || *cursor == '_') {
                while (isalnum((unsigned char)*cursor) || *cursor == '_') ++cursor;
                size_t length = (size_t)(cursor - line);
                if (*cursor == ':') {
                    char *rest = cursor + 1;
                    while (isspace((unsigned char)*rest)) ++rest;
                    if (*rest == '@') {
                        ++rest;
                        while (isspace((unsigned char)*rest)) ++rest;
                        if (rest[0] == '0' && rest[1] == 'x' && isxdigit((unsigned char)rest[2])) {
                            uint32_t address = (uint32_t)strtoul(rest + 2, NULL, 16);
                            bool known = false;
                            for (size_t index = 0; index < symbol_count; ++index) {
                                if (symbols[index].address != address) continue;
                                known = true;
                                if (strlen(symbols[index].name) != length ||
                                    strncmp(symbols[index].name, line, length))
                                    ok = feb_fail(error, FEB_EXIT_OPERATION,
                                                  "duplicate Aria symbol address 0x%08x",
                                                  (unsigned)address);
                            }
                            if (!known) {
                                if (symbol_count == capacity) {
                                    capacity = capacity ? capacity * 2 : 4096;
                                    AsmSymbol *grown = realloc(symbols, capacity * sizeof *grown);
                                    if (!grown) {
                                        ok = feb_oom(error);
                                        break;
                                    }
                                    symbols = grown;
                                }
                                symbols[symbol_count].address = address;
                                snprintf(symbols[symbol_count].name, sizeof symbols[0].name,
                                         "%.*s", (int)length, line);
                                ++symbol_count;
                            }
                        }
                    }
                }
            }
            line = end ? end + 1 : NULL;
        }
        free(text);
    }
    size_t offset = 0;
    if (ok && !rom_slice(&rom, ARIA_ENEMY_TABLE, ARIA_ENEMY_COUNT * ARIA_ENEMY_ENTRY, &offset))
        ok = feb_fail(error, FEB_EXIT_OPERATION, "Aria enemy table is outside the ROM");
    for (int enemy = 0; ok && enemy < ARIA_ENEMY_COUNT; ++enemy) {
        uint32_t constructor = read_u32(rom.data + offset + (size_t)enemy * ARIA_ENEMY_ENTRY) & ~1u;
        const char *symbol = NULL;
        for (size_t index = 0; index < symbol_count; ++index)
            if (symbols[index].address == constructor) symbol = symbols[index].name;
        size_t length = symbol ? strlen(symbol) : 0;
        bool named = symbol && length > 11 && !strncmp(symbol, "Enemy", 5) &&
                     !strcmp(symbol + length - 6, "Create");
        for (size_t index = 5; named && index < length - 6; ++index)
            if (!isalnum((unsigned char)symbol[index])) named = false;
        if (!named) {
            ok = feb_fail(error, FEB_EXIT_OPERATION,
                          "unnamed Aria enemy constructor 0x%08x for ID 0x%02x",
                          (unsigned)constructor, enemy);
            break;
        }
        char middle[96];
        snprintf(middle, sizeof middle, "%.*s", (int)(length - 11), symbol + 5);
        camel_label(middle, names[enemy], sizeof names[0]);
    }
    for (size_t file = 0; file < file_count; ++file) free(files[file]);
    free(files);
    free(symbols);
    fusion_rom_close(&rom);
    return ok;
}

typedef struct {
    int64_t kind, id, placements;
    int64_t *health;
    size_t health_count;
    char **bosses;
    size_t boss_count;
} Group;

static int compare_ints(const void *left, const void *right) {
    int64_t a = *(const int64_t *)left, b = *(const int64_t *)right;
    return a < b ? -1 : a > b;
}

static int compare_groups(const void *left, const void *right) {
    const Group *a = left, *b = right;
    if (a->kind != b->kind) return a->kind < b->kind ? -1 : 1;
    return a->id < b->id ? -1 : a->id > b->id;
}

static void lower_ascii(char *text) {
    for (; *text; ++text) *text = (char)tolower((unsigned char)*text);
}

/* aria_entity_identity(): a truthful display name and category. */
void feb_aria_entity_identity(int64_t kind, int64_t id, char (*enemies)[96], char *name,
                              size_t size, const char **category) {
    if (kind == 1) {
        if (id >= 0 && id < ARIA_ENEMY_COUNT) snprintf(name, size, "%s", enemies[id]);
        else snprintf(name, size, "Unknown enemy 0x%02llX", (long long)id);
        *category = id >= 0x6A ? "Boss" : "Enemy";
        return;
    }
    if (kind == 2) {
        if (id >= 0 && id < (int64_t)COUNT(aria_special_objects))
            snprintf(name, size, "%s", aria_special_objects[id]);
        else snprintf(name, size, "Unknown special object 0x%02llX", (long long)id);
        *category = "World object / event";
        return;
    }
    if (kind == 3) {
        snprintf(name, size, "Generic candle");
        *category = "Candle / breakable";
        return;
    }
    char pickup[96];
    if (id >= 0 && id < (int64_t)COUNT(aria_pickups)) snprintf(pickup, sizeof pickup, "%s", aria_pickups[id]);
    else snprintf(pickup, sizeof pickup, "Unknown pickup 0x%02llX", (long long)id);
    if (kind == 4) {
        snprintf(name, size, "%s", pickup);
        *category = "Item / soul pickup";
    } else if (kind == 5 || kind == 6) {
        char lower[96];
        snprintf(lower, sizeof lower, "%s", pickup);
        lower_ascii(lower);
        snprintf(name, size, kind == 5 ? "Hard-mode %s" : "All-souls reward: %s", lower);
        *category = "Conditional pickup";
    } else {
        snprintf(name, size, "Unknown %s 0x%02llX",
                 kind >= 0 && kind < (int64_t)COUNT(aria_kind_names) ? aria_kind_names[kind]
                                                                     : "entity kind",
                 (long long)id);
        *category = "Unknown native type";
    }
}

static bool build_aria(const char *repository, FusionJson *records, FebError *error) {
    char path[FEB_PATH_CAPACITY], detail[256];
    FusionJson world;
    if (!feb_join(path, repository, "assets/extracted/rooms/aria/world.json") ||
        !feb_load_json(path, 64 * 1024 * 1024, &world, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "missing native Aria world catalog %s; run python3 -m scripts.rebuild "
                        "--task aria_world", path);
    const FusionJson *format = fusion_json_member(&world, "format");
    if (!format || format->kind != FUSION_JSON_STRING || strcmp(format->string, "MV_AOS_WORLD_2")) {
        fusion_json_free(&world);
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria native world catalog");
    }
    static char enemies[ARIA_ENEMY_COUNT][96];
    if (!feb_aria_enemy_names(repository, enemies, error)) {
        fusion_json_free(&world);
        return false;
    }
    Group *groups = NULL;
    size_t group_count = 0;
    bool ok = true;
    const FusionJson *rooms = fusion_json_member(&world, "rooms");
    for (size_t room = 0; ok && room < rooms->count; ++room) {
        const FusionJson *entities = fusion_json_member(&rooms->items[room], "entities");
        for (size_t index = 0; ok && entities && index < entities->count; ++index) {
            const FusionJson *entity = &entities->items[index];
            int64_t kind = fusion_json_member(entity, "kind")->integer;
            int64_t id = fusion_json_member(entity, "entity_id")->integer;
            Group *group = NULL;
            for (size_t other = 0; other < group_count; ++other)
                if (groups[other].kind == kind && groups[other].id == id) group = &groups[other];
            if (!group) {
                Group *grown = realloc(groups, (group_count + 1) * sizeof *grown);
                if (!grown) {
                    ok = feb_oom(error);
                    break;
                }
                groups = grown;
                group = &groups[group_count++];
                *group = (Group){kind, id, 0, NULL, 0, NULL, 0};
            }
            ++group->placements;
            const FusionJson *boss = fusion_json_member(entity, "boss_id");
            if (boss && boss->kind == FUSION_JSON_STRING) {
                char **grown = realloc(group->bosses, (group->boss_count + 1) * sizeof *grown);
                if (grown) {
                    group->bosses = grown;
                    group->bosses[group->boss_count++] = strdup(boss->string);
                }
            }
            const FusionJson *health = fusion_json_member(entity, "native_health");
            if (health && health->kind == FUSION_JSON_NUMBER) {
                bool seen = false;
                for (size_t other = 0; other < group->health_count; ++other)
                    if (group->health[other] == health->integer) seen = true;
                if (!seen) {
                    int64_t *grown = realloc(group->health,
                                             (group->health_count + 1) * sizeof *grown);
                    if (grown) {
                        group->health = grown;
                        group->health[group->health_count++] = health->integer;
                    }
                }
            }
        }
    }
    qsort(groups, group_count, sizeof *groups, compare_groups);
    for (size_t index = 0; ok && index < group_count; ++index) {
        Group *group = &groups[index];
        char health[512] = "not decoded", bosses[512] = "-", name[256], kind_token[96], token[128];
        qsort(group->health, group->health_count, sizeof *group->health, compare_ints);
        if (group->health_count) {
            size_t used = 0;
            for (size_t value = 0; value < group->health_count; ++value)
                used += (size_t)snprintf(health + used, sizeof health - used, "%s%lld",
                                         value ? "," : "", (long long)group->health[value]);
        }
        qsort(group->bosses, group->boss_count, sizeof *group->bosses, compare_strings);
        if (group->boss_count) {
            size_t used = 0;
            for (size_t value = 0; value < group->boss_count; ++value) {
                if (value && !strcmp(group->bosses[value], group->bosses[value - 1])) continue;
                used += (size_t)snprintf(bosses + used, sizeof bosses - used, "%s%s",
                                         used ? "," : "", group->bosses[value]);
            }
        }
        const char *category = NULL;
        feb_aria_entity_identity(group->kind, group->id, enemies, name, sizeof name, &category);
        if (group->kind >= 0 && group->kind < (int64_t)COUNT(aria_kind_names))
            snprintf(kind_token, sizeof kind_token, "%s", aria_kind_names[group->kind]);
        else
            snprintf(kind_token, sizeof kind_token, "Unknown kind %lld", (long long)group->kind);
        lower_ascii(kind_token);
        for (char *c = kind_token; *c; ++c)
            if (*c == ' ') *c = '-';
        snprintf(token, sizeof token, "%s:%02llX", kind_token, (long long)group->id);
        char summary[4096];
        snprintf(summary, sizeof summary,
                 "kind=%lld (0x%02llX); id=%lld (0x%02llX); health=%s; native placements=%lld; "
                 "boss references=%s",
                 (long long)group->kind, (long long)group->kind, (long long)group->id,
                 (long long)group->id, health, (long long)group->placements, bosses);
        FusionJson record = fusion_json_object();
        ok = feb_put_string(&record, "world", "Aria of Sorrow") &&
             feb_put_int(&record, "native_id", group->id) &&
             feb_put_string(&record, "native_type", token) && feb_put_string(&record, "name", name) &&
             feb_put_string(&record, "category", category) &&
             feb_put_string(&record, "summary", summary) &&
             feb_put_int(&record, "placements", group->placements) &&
             feb_put_bool(&record, "editable", false) && fusion_json_push(records, record);
        if (!ok) feb_oom(error);
    }
    for (size_t index = 0; index < group_count; ++index) {
        for (size_t boss = 0; boss < groups[index].boss_count; ++boss) free(groups[index].bosses[boss]);
        free(groups[index].bosses);
        free(groups[index].health);
    }
    free(groups);
    fusion_json_free(&world);
    return ok;
}

static const char *text_of(const FusionJson *record, const char *key) {
    const FusionJson *value = fusion_json_member(record, key);
    return value && value->kind == FUSION_JSON_STRING ? value->string : "";
}

static int compare_records(const void *left, const void *right) {
    const FusionJson *a = left, *b = right;
    int order = strcmp(text_of(a, "world"), text_of(b, "world"));
    if (order) return order;
    bool unused_a = !strcmp(text_of(a, "category"), "Unused native type");
    bool unused_b = !strcmp(text_of(b, "category"), "Unused native type");
    if (unused_a != unused_b) return unused_a ? 1 : -1;
    order = strcmp(text_of(a, "category"), text_of(b, "category"));
    if (order) return order;
    order = strcmp(text_of(a, "name"), text_of(b, "name"));
    if (order) return order;
    int64_t x = fusion_json_member(a, "native_id")->integer;
    int64_t y = fusion_json_member(b, "native_id")->integer;
    return x < y ? -1 : x > y;
}

/* A stable merge sort of JSON array items (Python's sorted() is stable). */
static void stable_sort(FusionJson *items, size_t count,
                        int (*compare)(const void *, const void *)) {
    if (count < 2) return;
    FusionJson *scratch = malloc(count * sizeof *scratch);
    if (!scratch) return;
    for (size_t width = 1; width < count; width *= 2) {
        for (size_t start = 0; start < count; start += 2 * width) {
            size_t middle = start + width < count ? start + width : count;
            size_t end = start + 2 * width < count ? start + 2 * width : count;
            size_t left = start, right = middle, out = start;
            while (left < middle && right < end)
                scratch[out++] = compare(&items[right], &items[left]) < 0 ? items[right++]
                                                                         : items[left++];
            while (left < middle) scratch[out++] = items[left++];
            while (right < end) scratch[out++] = items[right++];
        }
        memcpy(items, scratch, count * sizeof *items);
    }
    free(scratch);
}

bool feb_object_records(const char *repository, const char *world, FusionJson *records,
                        FebError *error) {
    *records = fusion_json_array();
    bool ok = true;
    if (!world || !strcmp(world, "zero_mission")) ok = feb_mzm_object_records(repository, records, error);
    if (ok && (!world || !strcmp(world, "aria"))) ok = build_aria(repository, records, error);
    if (!ok) {
        fusion_json_free(records);
        return false;
    }
    stable_sort(records->items, records->count, compare_records);
    return true;
}

/* _text() of the project room model. */
static bool display_text(const char *value, size_t limit, char *out, size_t out_size) {
    /* value[:limit] in code points */
    size_t points = 0, bytes = 0;
    while (value[bytes] && points < limit) {
        unsigned char c = (unsigned char)value[bytes];
        size_t step = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        bytes += step;
        ++points;
    }
    snprintf(out, out_size, "%.*s", (int)bytes, value);
    if (!points || strpbrk(out, "|\t")) return false;
    uint32_t first = 0, last = 0;
    if (!feb_utf8_edges(out, &first, &last) || feb_python_space(first) || feb_python_space(last))
        return false;
    for (const unsigned char *c = (const unsigned char *)out; *c; ++c)
        if (*c < 32 || *c == 127) return false;
    return true;
}

static bool token_valid(const char *token) {
    size_t length = strlen(token);
    if (length < 1 || length > 64) return false;
    for (const char *c = token; *c; ++c)
        if (!(isalnum((unsigned char)*c) || strchr("_.:-", *c))) return false;
    return true;
}

/* str.casefold() for the Latin text of the catalogs. */
static void casefold(const char *text, char *out, size_t size) {
    size_t used = 0;
    for (const unsigned char *c = (const unsigned char *)text; *c && used + 4 < size;) {
        if (*c < 0x80) {
            out[used++] = (char)tolower(*c);
            ++c;
        } else if (c[0] == 0xC3 && c[1] == 0x9F) {        /* ß */
            out[used++] = 's';
            out[used++] = 's';
            c += 2;
        } else if (c[0] == 0xC3 && c[1] >= 0x80 && c[1] <= 0x9E && c[1] != 0x97) {
            out[used++] = (char)0xC3;                      /* À-Þ to à-þ */
            out[used++] = (char)(c[1] + 0x20);
            c += 2;
        } else if (c[0] == 0xC5 && c[1] == 0x92) {        /* Œ */
            out[used++] = (char)0xC5;
            out[used++] = (char)0x93;
            c += 2;
        } else {
            size_t step = (*c & 0xE0) == 0xC0 ? 2 : (*c & 0xF0) == 0xE0 ? 3 : (*c & 0xF8) == 0xF0 ? 4 : 1;
            for (size_t index = 0; index < step && c[index]; ++index) out[used++] = (char)c[index];
            c += step;
        }
    }
    out[used] = '\0';
}

static int compare_options(const void *left, const void *right) {
    char a[512], b[512];
    casefold(text_of(left, "name"), a, sizeof a);
    casefold(text_of(right, "name"), b, sizeof b);
    int order = strcmp(a, b);
    return order ? order : strcmp(text_of(left, "native_type"), text_of(right, "native_type"));
}

static bool has_option(const FusionJson *options, const char *token) {
    for (size_t index = 0; index < options->count; ++index)
        if (!strcmp(text_of(&options->items[index], "native_type"), token)) return true;
    return false;
}

const char *feb_catalog_role(const char *world, const char *token, const char *category) {
    if (!world || !token || !category || !token_valid(token)) return NULL;
    if (!strcmp(world, "mzm")) {
        if (!strcmp(category, "Enemy / actor")) return "ENEMY";
        if (!strcmp(category, "Item / pickup") || !strcmp(category, "Upgrade / ability"))
            return "ITEM";
        if (!strcmp(category, "World object")) return "OBJECT";
        return NULL;
    }
    if (strcmp(world, "aria")) return NULL;
    const char *colon = strchr(token, ':');
    if (!colon) return NULL;
    char group[64];
    snprintf(group, sizeof group, "%.*s", (int)(colon - token), token);
    char *tail = NULL;
    long id = strtol(colon + 1, &tail, 16);
    if (!colon[1] || *tail) return NULL;
    if (!strcmp(group, "enemy") || (!strcmp(group, "special-object") && (id == 10 || id == 11)))
        return "ENEMY";
    if (!strcmp(group, "pickup") || !strcmp(group, "hard-mode-pickup") ||
        !strcmp(group, "all-souls-reward"))
        return "ITEM";
    if (!strcmp(group, "special-object") &&
        (id == 0 || id == 2 || id == 3 || id == 4 || id == 5 || id == 6))
        return "DOOR";
    if (!strcmp(group, "special-object") || !strcmp(group, "generic-candle")) return "OBJECT";
    return NULL;
}

bool feb_aria_item_family_options(FusionJson *options, FebError *error) {
    if (!options || options->kind != FUSION_JSON_ARRAY)
        return feb_fail(error, FEB_EXIT_OPERATION, "Aria item options must be an array");
    static const char *const families[][2] = {
        {"Pickup", "pickup"}, {"Hard Mode", "hard-mode-pickup"},
        {"All Souls", "all-souls-reward"}};
    bool ok = true;
    for (size_t family = 0; ok && family < 3; ++family) {
        for (int subtype = 0; ok && subtype < 9; ++subtype) {
            char token[64], name[160], category[160];
            snprintf(token, sizeof token, "%s:%02X", families[family][1], subtype);
            if (has_option(options, token)) continue;
            snprintf(name, sizeof name, "%s / %s", families[family][0], aria_pickups[subtype]);
            snprintf(category, sizeof category, "Aria %s / subtype %02X", families[family][0],
                     subtype);
            FusionJson option = fusion_json_object();
            ok = (feb_put_string(&option, "native_type", token) &&
                  feb_put_string(&option, "name", name) &&
                  feb_put_string(&option, "category", category) &&
                  fusion_json_push(options, option)) || feb_oom(error);
        }
    }
    return ok;
}

bool feb_catalog_options(const char *repository, const char *world, const char *kind,
                         FusionJson *options, FebError *error) {
    *options = fusion_json_array();
    bool aria = !strcmp(world, "aria");
    if ((!aria && strcmp(world, "mzm")) || !kind ||
        (strcmp(kind, "ENEMY") && strcmp(kind, "ITEM") && strcmp(kind, "OBJECT")))
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported native catalog scope");
    FusionJson records = fusion_json_array();
    bool ok = aria ? build_aria(repository, &records, error) : feb_mzm_object_records(repository, &records, error);
    for (size_t index = 0; ok && index < records.count; ++index) {
        const FusionJson *record = &records.items[index];
        const char *token = text_of(record, "native_type");
        const char *category = text_of(record, "category");
        if (!token_valid(token)) continue;
        const char *role = feb_catalog_role(world, token, category);
        if (!role || strcmp(role, kind) || has_option(options, token)) continue;
        char name[512], category_text[1024];
        if (!display_text(text_of(record, "name"), 80, name, sizeof name) ||
            !display_text(category, 160, category_text, sizeof category_text))
            continue;
        FusionJson option = fusion_json_object();
        ok = (feb_put_string(&option, "native_type", token) && feb_put_string(&option, "name", name) &&
              feb_put_string(&option, "category", category_text) &&
              fusion_json_push(options, option)) || feb_oom(error);
    }
    fusion_json_free(&records);
    if (ok && aria && !strcmp(kind, "ITEM")) {
        /* The documented subtype families, even if never placed originally. */
        ok = feb_aria_item_family_options(options, error);
    }
    if (!ok) {
        fusion_json_free(options);
        return false;
    }
    stable_sort(options->items, options->count, compare_options);
    return true;
}

/* _read_name(): a native Aria text string with its control opcodes. */
static bool read_name(const FusionRom *rom, int text_id, char *name, size_t size) {
    if (text_id < 0 || text_id > 0xB4E) return false;
    size_t offset = 0;
    if (!rom_slice(rom, ARIA_TEXT_POINTERS + (uint32_t)text_id * 4, 4, &offset)) return false;
    uint32_t address = read_u32(rom->data + offset);
    size_t position = 0;
    if (!rom_slice(rom, address, 3, &position)) return false;
    if (rom->data[position] != 1 || rom->data[position + 1] != 0) return false;
    char text[512];
    size_t used = 0, cursor = position + 2;
    size_t end = cursor + 160 < rom->size ? cursor + 160 : rom->size;
    while (cursor < end) {
        unsigned char code = rom->data[cursor++];
        if (code == 0x0A) {
            text[used] = '\0';
            /* .strip() of spaces and a name with at least one letter or digit */
            char *start = text;
            while (*start == ' ') ++start;
            size_t length = strlen(start);
            while (length && start[length - 1] == ' ') start[--length] = '\0';
            bool alnum = false;
            for (const unsigned char *c = (const unsigned char *)start; *c; ++c)
                if (isalnum(*c) || *c >= 0x80) alnum = true;
            if (!length || feb_utf8_length(start) > 80 || !alnum) return false;
            snprintf(name, size, "%s", start);
            return true;
        }
        if (code == 1 || code == 2 || code == 3 || code == 7 || code == 8) {
            if (cursor >= end) break;
            ++cursor;
        } else if (code == 6) {
            if (used && text[used - 1] != ' ' && used + 1 < sizeof text) text[used++] = ' ';
        } else if (code == 4 || code == 5 || code == 9 || (code >= 0x0B && code <= 0x1A)) {
            continue;
        } else if (code >= 0x20 && code <= 0x7E) {
            if (used + 1 < sizeof text) text[used++] = (char)code;
        } else {
            const char *special = NULL;
            for (size_t index = 0; index < COUNT(special_characters); ++index)
                if (special_characters[index].code == code) special = special_characters[index].text;
            if (!special) return false;
            size_t length = strlen(special);
            if (used + length < sizeof text) {
                memcpy(text + used, special, length);
                used += length;
            }
        }
    }
    return false;
}

bool feb_aria_item_name_from_rom(const FusionRom *rom, int64_t subtype, int64_t item_id,
                                 char *name, size_t size) {
    for (size_t index = 0; index < COUNT(item_ranges); ++index) {
        if (item_ranges[index].subtype != subtype) continue;
        if (item_id < item_ranges[index].start ||
            item_id >= item_ranges[index].start + item_ranges[index].count)
            return false;
        return read_name(rom, item_ranges[index].first + (int)(item_id - item_ranges[index].start),
                         name, size);
    }
    return false;
}

bool feb_aria_item_name(const char *repository, int64_t subtype, int64_t item_id, char *name,
                        size_t size) {
    FusionRom rom = {0};
    FebError ignored;
    if (!open_aria(repository, &rom, &ignored)) return false;
    bool ok = feb_aria_item_name_from_rom(&rom, subtype, item_id, name, size);
    fusion_rom_close(&rom);
    return ok;
}

static bool lz10(const FusionRom *rom, uint32_t address, uint8_t *out, size_t *size) {
    size_t pointer = 0;
    if (!rom_slice(rom, address, 4, &pointer) || rom->data[pointer] != 0x10) return false;
    size_t total = rom->data[pointer + 1] | (rom->data[pointer + 2] << 8) |
                   ((size_t)rom->data[pointer + 3] << 16);
    if (total != 0x2000 && total != 0x2004) return false;
    pointer += 4;
    size_t used = 0;
    while (used < total) {
        if (pointer >= rom->size) return false;
        uint8_t flags = rom->data[pointer++];
        for (int bit = 7; bit >= 0 && used < total; --bit) {
            if (flags & (1 << bit)) {
                if (pointer + 2 > rom->size) return false;
                unsigned pair = (rom->data[pointer] << 8) | rom->data[pointer + 1];
                pointer += 2;
                size_t count = (pair >> 12) + 3, distance = (pair & 0xFFF) + 1;
                if (distance > used || used + count > total) return false;
                for (size_t index = 0; index < count; ++index, ++used) out[used] = out[used - distance];
            } else {
                if (pointer >= rom->size) return false;
                out[used++] = rom->data[pointer++];
            }
        }
    }
    *size = total;
    return true;
}

/* _page(): one GfxWrapper icon page of 0x2000 bytes. */
bool feb_aria_icon_page(const FusionRom *rom, uint32_t pointer, uint8_t page[0x2000]) {
    size_t offset = 0;
    if (!rom_slice(rom, pointer, 4, &offset)) return false;
    uint8_t kind = rom->data[offset], bpp = rom->data[offset + 1], chunks = rom->data[offset + 3];
    if (bpp != 4 || kind > 1) return false;
    if (kind == 0) {
        size_t data = 0;
        if (chunks != 16 || !rom_slice(rom, pointer + 4, 0x2000, &data)) return false;
        memcpy(page, rom->data + data, 0x2000);
        return true;
    }
    size_t at = 0;
    if (!rom_slice(rom, pointer + 4, 4, &at)) return false;
    static uint8_t buffer[0x2004];
    size_t size = 0;
    if (!lz10(rom, read_u32(rom->data + at), buffer, &size)) return false;
    const uint8_t *data = buffer;
    if (size == 0x2004) {
        if (memcmp(buffer, "\x00\x04\x00\x10", 4) && memcmp(buffer, "\x00\x00\x00\x00", 4))
            return false;
        data += 4;
    }
    memcpy(page, data, 0x2000);
    return true;
}

static bool write_if_changed(const char *path, const uint8_t *data, size_t size, int *count) {
    char *current = NULL;
    size_t current_size = 0;
    if (feb_read_text(path, size + 1, &current, &current_size) && current_size == size &&
        !memcmp(current, data, size)) {
        free(current);
        return true;
    }
    free(current);
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool ok = fwrite(data, 1, size, file) == size;
    ok = fclose(file) == 0 && ok;
    if (ok) ++*count;
    return ok;
}

static void make_directory(const char *path) {
    char current[FEB_PATH_CAPACITY];
    snprintf(current, sizeof current, "%s", path);
    for (char *slash = current + 1; (slash = strchr(slash, '/')) != NULL; ++slash) {
        *slash = '\0';
        mkdir(current, 0777);
        *slash = '/';
    }
    mkdir(current, 0777);
}

/* generate_aria_item_thumbnails(): the 16x16 ROM item icons. */
bool feb_aria_item_thumbnails_from_rom(const char *repository, const FusionRom *rom,
                                       int *written) {
    static uint8_t pages[3][0x2000];
    for (size_t page = 0; page < 3; ++page)
        if (!feb_aria_icon_page(rom, icon_banks[page], pages[page])) return false;
    size_t palette_offset = 0;
    if (!rom_slice(rom, ARIA_ICON_PALETTE, 4, &palette_offset)) return false;
    const uint8_t *wrapper = rom->data + palette_offset;
    char output[FEB_PATH_CAPACITY];
    if (!feb_join(output, repository, "assets/extracted/sprite_previews/aria")) return false;
    int count = 0;
    for (size_t table = 0; table < COUNT(item_tables); ++table) {
        for (int item = 0; item < item_tables[table].count; ++item) {
            size_t entry = 0;
            if (!rom_slice(rom, item_tables[table].pointer + (uint32_t)(item_tables[table].stride * item),
                           4, &entry))
                return false;
            unsigned packed = rom->data[entry + 2] | (rom->data[entry + 3] << 8);
            if (!(packed & 0xFF)) continue;
            int icon = (int)(packed & 0xFF) - 1, palette_index = (int)(packed >> 8) - 4;
            if (palette_index < 0 || palette_index > 4 || icon >= 3 * 64) continue;
            if (wrapper[0] || wrapper[3] || wrapper[2] <= palette_index) return false;
            size_t palette = 0;
            if (!rom_slice(rom, ARIA_ICON_PALETTE + 4 + (uint32_t)palette_index * 32, 32, &palette))
                return false;
            const uint8_t *data = pages[icon / 64] + (icon % 64) * 128;
            uint8_t pixels[16 * 16 * 4];
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int tile = (y / 8) * 2 + x / 8;
                    int position = tile * 32 + (y % 8) * 4 + (x % 8) / 2;
                    int code = data[position] >> (x % 2 ? 4 : 0) & 15;
                    unsigned rgb = rom->data[palette + code * 2] | (rom->data[palette + code * 2 + 1] << 8);
                    uint8_t *pixel = pixels + (y * 16 + x) * 4;
                    pixel[0] = (uint8_t)((rgb & 31) * 255 / 31);
                    pixel[1] = (uint8_t)((rgb >> 5 & 31) * 255 / 31);
                    pixel[2] = (uint8_t)((rgb >> 10 & 31) * 255 / 31);
                    pixel[3] = code ? 255 : 0;
                }
            }
            uint8_t *png = NULL;
            size_t size = 0;
            if (!feb_png_rgba(16, 16, pixels, &png, &size)) return false;
            char folder[FEB_PATH_CAPACITY + 16], path[FEB_PATH_CAPACITY + 64];
            snprintf(folder, sizeof folder, "%s/items", output);
            make_directory(folder);
            snprintf(path, sizeof path, "%s/%02X_%03d.png", folder, item_tables[table].subtype, item);
            bool ok = write_if_changed(path, png, size, &count);
            if (ok && item == 0) {
                static const char *const families[] = {"pickup", "hard-mode-pickup", "all-souls-reward"};
                for (size_t family = 0; ok && family < 3; ++family) {
                    snprintf(path, sizeof path, "%s/%s:%02X.png", output, families[family],
                             item_tables[table].subtype);
                    ok = write_if_changed(path, png, size, &count);
                }
            }
            free(png);
            if (!ok) return false;
        }
    }
    if (written) *written = count;
    return true;
}

bool feb_aria_named_item_options_from_rom(const FusionRom *rom, FusionJson *options,
                                          FebError *error) {
    *options = fusion_json_array();
    bool ok = true;
    for (size_t mode = 0; ok && mode < COUNT(modes); ++mode) {
        char token[64], category[96];
        snprintf(token, sizeof token, "%s:01", modes[mode][0]);
        snprintf(category, sizeof category, "%s / Money", modes[mode][1]);
        FusionJson money = fusion_json_object();
        ok = feb_put_string(&money, "native_type", token) &&
             feb_put_string(&money, "name", "Money pickup") &&
             feb_put_string(&money, "category", category) && feb_put_int(&money, "item_id", 0) &&
             fusion_json_push(options, money);
        for (size_t range = 0; ok && range < COUNT(item_ranges); ++range) {
            int subtype = item_ranges[range].subtype;
            for (int item = item_ranges[range].start;
                 ok && item < item_ranges[range].start + item_ranges[range].count; ++item) {
                char name[256];
                if (!feb_aria_item_name_from_rom(rom, subtype, item, name, sizeof name))
                    snprintf(name, sizeof name, "[Name not decoded: %02X/%02X]", subtype, item);
                snprintf(token, sizeof token, "%s:%02X", modes[mode][0], subtype);
                snprintf(category, sizeof category, "%s / %s", modes[mode][1], family_names[subtype]);
                FusionJson option = fusion_json_object();
                ok = feb_put_string(&option, "native_type", token) &&
                     feb_put_string(&option, "name", name) &&
                     feb_put_string(&option, "category", category) &&
                     feb_put_int(&option, "item_id", item) && fusion_json_push(options, option);
            }
        }
    }
    if (!ok) {
        fusion_json_free(options);
        return feb_oom(error);
    }
    return true;
}

bool feb_editor_catalog_options(const char *repository, const char *world, const char *kind,
                                FusionJson *options, FebError *error) {
    if (!strcmp(world, "aria") && kind && !strcmp(kind, "ITEM")) {
        FusionRom rom = {0};
        FebError ignored;
        if (open_aria(repository, &rom, &ignored)) {
            bool ok = feb_aria_named_item_options_from_rom(&rom, options, error);
            if (ok) feb_aria_item_thumbnails_from_rom(repository, &rom, NULL);
            fusion_rom_close(&rom);
            if (ok) return true;
        }
    }
    return feb_catalog_options(repository, world, kind, options, error);
}
