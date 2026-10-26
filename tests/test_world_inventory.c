/* SPDX-License-Identifier: GPL-3.0-only */
/* data/story/world_inventory.toml: the exact campaign boss rosters, their
 * verified Aria native identities and the complete savepoint sets. */
#include "core/json.h"
#include "core/toml.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif

static int failures = 0;

static void fail(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    fputs("FAIL: ", stderr);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    va_end(arguments);
    ++failures;
}

static const FusionJson *get(const FusionJson *object, const char *key, const char *owner) {
    const FusionJson *value = fusion_json_member(object, key);
    if (!value) fail("%s: missing %s", owner, key);
    return value;
}

static const char *text(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    return value && value->kind == FUSION_JSON_STRING ? value->string : "";
}

static int64_t number(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    return value && value->kind == FUSION_JSON_NUMBER ? value->integer : -1;
}

static bool in_list(const char *const *list, size_t count, const char *value) {
    for (size_t index = 0; index < count; ++index)
        if (!strcmp(list[index], value)) return true;
    return false;
}

static const char *const mzm_bosses[] = {
    "mzm.deorem", "mzm.mua", "mzm.kraid", "mzm.kiru_giru", "mzm.imago", "mzm.ridley",
    "mzm.mother_brain", "mzm.ruins_test", "mzm.mecha_ridley"};
static const struct {
    const char *id;
    int area, room, entity, health;
} aria_bosses[] = {
    {"aria.creaking_skull", 0, 10, 0x21, 240}, {"aria.manticore", 1, 6, 0x36, 440},
    {"aria.great_armor", 2, 15, 0x3C, 650},    {"aria.big_golem", 3, 19, 0x45, 1200},
    {"aria.headhunter", 4, 11, 0x6A, 700},     {"aria.julius", 5, 6, 0x6E, 6000},
    {"aria.death", 6, 7, 0x6B, 4444},          {"aria.legion", 7, 37, 0x6C, 5000},
    {"aria.balore", 8, 1, 0x6D, 4000},         {"aria.graham", 9, 4, 0x6F, 5000},
    {"aria.chaos", 11, 20, 0x70, 9999}};

static void test_bosses(const FusionJson *data) {
    const FusionJson *sources = get(data, "sources", "inventory");
    const FusionJson *bosses = get(data, "bosses", "inventory");
    if (!sources || !bosses) return;
    static const char *const required[] = {
        "id", "game", "name", "zone", "classification", "campaign_required",
        "original_requirement", "source_rooms", "rom_addresses", "spawn_conditions",
        "prerequisites", "capabilities", "stats", "attacks", "music", "resources", "sequences",
        "reward", "flags", "decode_state", "story_role", "sources"};
    if (bosses->count != 20) fail("bosses: %zu records, expected 20", bosses->count);
    size_t mzm = 0, aria = 0;
    for (size_t index = 0; index < bosses->count; ++index) {
        const FusionJson *boss = &bosses->items[index];
        const char *id = text(boss, "id");
        for (size_t key = 0; key < sizeof required / sizeof *required; ++key) get(boss, required[key], id);
        for (size_t other = index + 1; other < bosses->count; ++other)
            if (!strcmp(id, text(&bosses->items[other], "id"))) fail("bosses: duplicate %s", id);
        const FusionJson *required_flag = fusion_json_member(boss, "campaign_required");
        if (!required_flag || required_flag->kind != FUSION_JSON_BOOL || !required_flag->boolean)
            fail("%s: not campaign_required", id);
        const FusionJson *classification = fusion_json_member(boss, "classification");
        bool classified = classification &&
                          ((classification->kind == FUSION_JSON_ARRAY && classification->count) ||
                           (classification->kind == FUSION_JSON_STRING && classification->string[0]));
        if (!classified || !text(boss, "decode_state")[0])
            fail("%s: empty classification or decode_state", id);
        const FusionJson *aliases = fusion_json_member(boss, "sources");
        for (size_t alias = 0; aliases && alias < aliases->count; ++alias)
            if (!fusion_json_member(sources, aliases->items[alias].string))
                fail("%s: unknown source %s", id, aliases->items[alias].string);
        const char *game = text(boss, "game");
        if (!strcmp(game, "zero_mission")) {
            ++mzm;
            if (!in_list(mzm_bosses, sizeof mzm_bosses / sizeof *mzm_bosses, id))
                fail("unexpected Zero Mission boss %s", id);
            continue;
        }
        if (strcmp(game, "aria_of_sorrow")) {
            fail("%s: unknown game %s", id, game);
            continue;
        }
        ++aria;
        int found = -1;
        for (size_t other = 0; other < sizeof aria_bosses / sizeof *aria_bosses; ++other)
            if (!strcmp(aria_bosses[other].id, id)) found = (int)other;
        if (found < 0) {
            fail("unexpected Aria boss %s", id);
            continue;
        }
        if (strstr(text(boss, "decode_state"), "unverified")) fail("%s: unverified", id);
        const FusionJson *rooms = fusion_json_member(boss, "source_rooms");
        const FusionJson *first = rooms && rooms->count ? &rooms->items[0] : NULL;
        if (!first || number(first, "engine_area") != aria_bosses[found].area ||
            number(first, "room") != aria_bosses[found].room)
            fail("%s: source room", id);
        const FusionJson *addresses = fusion_json_member(boss, "rom_addresses");
        if (!addresses || addresses->count < 3) fail("%s: fewer than 3 ROM addresses", id);
        const FusionJson *identity = fusion_json_member(boss, "native_identity");
        if (!identity || strcmp(text(identity, "kind"), "enemy") ||
            number(identity, "entity_id") != aria_bosses[found].entity)
            fail("%s: native identity", id);
        const FusionJson *stats = fusion_json_member(boss, "stats");
        if (!stats || strcmp(text(stats, "kind"), "enemy_table_raw") ||
            number(stats, "health") != aria_bosses[found].health)
            fail("%s: stats", id);
        if (!strcmp(id, "aria.chaos")) {
            if (!rooms || rooms->count != 2 || number(&rooms->items[0], "room") != 20 ||
                strcmp(text(&rooms->items[0], "phase"), "first_phase") ||
                number(&rooms->items[1], "room") != 22 ||
                strcmp(text(&rooms->items[1], "phase"), "second_phase"))
                fail("aria.chaos: phases");
        }
    }
    if (mzm != sizeof mzm_bosses / sizeof *mzm_bosses) fail("Zero Mission roster: %zu", mzm);
    if (aria != sizeof aria_bosses / sizeof *aria_bosses) fail("Aria roster: %zu", aria);
    const FusionJson *excluded = get(data, "excluded_encounters", "inventory");
    bool man_eater = false;
    for (size_t index = 0; excluded && index < excluded->count; ++index)
        man_eater |= !strcmp(text(&excluded->items[index], "id"), "aria.man_eater");
    if (!man_eater) fail("aria.man_eater is not excluded");
    for (size_t index = 0; index < bosses->count; ++index)
        if (!strcmp(text(&bosses->items[index], "id"), "aria.man_eater"))
            fail("aria.man_eater is a campaign boss");
}

static const struct {
    int area, room, x, y;
    const char *pointer;
} aria_saves[] = {
    {5, 11, 19, 1, "0x085179d0"},  {6, 31, 38, 5, "0x085196e0"},  {9, 22, 29, 6, "0x085209b8"},
    {4, 20, 15, 10, "0x08516768"}, {6, 15, 39, 11, "0x08518d28"}, {3, 21, 9, 13, "0x08514c70"},
    {0, 31, 40, 16, "0x0851021c"}, {3, 22, 10, 19, "0x08514ce8"}, {5, 12, 29, 20, "0x08517a48"},
    {1, 14, 48, 20, "0x085122fc"}, {0, 14, 17, 21, "0x0850f7ec"}, {0, 36, 9, 22, "0x08510504"},
    {2, 12, 39, 26, "0x085132d4"}, {7, 39, 18, 27, "0x0851c0d4"}, {7, 48, 27, 28, "0x0851c6b4"},
    {8, 21, 2, 29, "0x0851f128"},  {7, 41, 24, 33, "0x0851c1cc"}};

static const struct {
    const char *area;
    int rooms[8];
    int count;
} mzm_saves[] = {{"Brinstar", {33, 34, 36, 39}, 4},     {"Kraid", {20, 31, 32, 36, 39}, 5},
                 {"Norfair", {36, 39, 41, 44, 45}, 5},  {"Ridley", {1, 20, 24, 25}, 4},
                 {"Tourian", {6, 11, 17}, 3},
                 {"Chozodia", {4, 15, 21, 27, 40, 61, 74, 75}, 8}};

static void test_savepoints(const FusionJson *data) {
    const FusionJson *points = get(data, "savepoints", "inventory");
    const FusionJson *sources = get(data, "sources", "inventory");
    if (!points || !sources) return;
    static const char *const required[] = {
        "id", "game", "area", "room", "map_x", "map_y", "type", "access", "gates",
        "boss_context", "capabilities", "connections", "graphics", "music", "verification",
        "sources", "cross_world_link"};
    size_t mzm = 0, aria = 0;
    bool aria_found[sizeof aria_saves / sizeof *aria_saves] = {false};
    for (size_t index = 0; index < points->count; ++index) {
        const FusionJson *point = &points->items[index];
        const char *id = text(point, "id");
        for (size_t key = 0; key < sizeof required / sizeof *required; ++key) get(point, required[key], id);
        for (size_t other = index + 1; other < points->count; ++other) {
            const FusionJson *next = &points->items[other];
            if (!strcmp(id, text(next, "id"))) fail("savepoints: duplicate %s", id);
            if (!strcmp(text(point, "game"), text(next, "game")) &&
                fusion_json_equal(fusion_json_member(point, "area"), fusion_json_member(next, "area")) &&
                number(point, "room") == number(next, "room"))
                fail("savepoints: duplicate location of %s", id);
        }
        const FusionJson *aliases = fusion_json_member(point, "sources");
        for (size_t alias = 0; aliases && alias < aliases->count; ++alias)
            if (!fusion_json_member(sources, aliases->items[alias].string))
                fail("%s: unknown source %s", id, aliases->items[alias].string);
        const FusionJson *capabilities = fusion_json_member(point, "capabilities");
        bool save = false;
        for (size_t entry = 0; capabilities && entry < capabilities->count; ++entry)
            save |= capabilities->items[entry].kind == FUSION_JSON_STRING &&
                    !strcmp(capabilities->items[entry].string, "save");
        if (!save) fail("%s: no save capability", id);
        if (strcmp(text(point, "cross_world_link"), "")) fail("%s: cross-world link", id);
        char expected[128];
        if (!strcmp(text(point, "game"), "zero_mission")) {
            ++mzm;
            char lower[32];
            snprintf(lower, sizeof lower, "%s", text(point, "area"));
            for (char *c = lower; *c; ++c)
                if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
            snprintf(expected, sizeof expected, "mzm.save.%s.%lld", lower,
                     (long long)number(point, "room"));
            if (strcmp(id, expected)) fail("%s: id should be %s", id, expected);
            if (strcmp(text(point, "type"), "save_platform") ||
                strcmp(text(point, "verification"), "verified_source"))
                fail("%s: type or verification", id);
            bool listed = false;
            for (size_t area = 0; area < sizeof mzm_saves / sizeof *mzm_saves; ++area) {
                if (strcmp(mzm_saves[area].area, text(point, "area"))) continue;
                for (int room = 0; room < mzm_saves[area].count; ++room)
                    listed |= mzm_saves[area].rooms[room] == number(point, "room");
            }
            if (!listed) fail("%s: unexpected Zero Mission savepoint", id);
        } else {
            ++aria;
            snprintf(expected, sizeof expected, "aria.save.%lld.%lld",
                     (long long)number(point, "engine_area"), (long long)number(point, "room"));
            if (strcmp(id, expected)) fail("%s: id should be %s", id, expected);
            if (strcmp(text(point, "type"), "save_room_flag")) fail("%s: type", id);
            if (strcmp(text(point, "verification"), "verified_rom_map_table"))
                fail("%s: verification", id);
            bool listed = false;
            for (size_t save = 0; save < sizeof aria_saves / sizeof *aria_saves; ++save)
                if (number(point, "engine_area") == aria_saves[save].area &&
                    number(point, "room") == aria_saves[save].room &&
                    number(point, "map_x") == aria_saves[save].x &&
                    number(point, "map_y") == aria_saves[save].y &&
                    !strcmp(text(point, "room_pointer"), aria_saves[save].pointer)) {
                    listed = true;
                    aria_found[save] = true;
                }
            if (!listed) fail("%s: unexpected Aria savepoint", id);
        }
    }
    if (mzm != 29) fail("Zero Mission savepoints: %zu, expected 29", mzm);
    if (aria != sizeof aria_saves / sizeof *aria_saves) fail("Aria savepoints: %zu", aria);
    for (size_t save = 0; save < sizeof aria_saves / sizeof *aria_saves; ++save)
        if (!aria_found[save]) fail("missing Aria savepoint %s", aria_saves[save].pointer);
    const FusionJson *coverage = get(data, "savepoint_coverage", "inventory");
    const FusionJson *mzm_coverage = coverage ? fusion_json_member(coverage, "zero_mission") : NULL;
    const FusionJson *aria_coverage = coverage ? fusion_json_member(coverage, "aria_of_sorrow") : NULL;
    if (!mzm_coverage || strcmp(text(mzm_coverage, "status"), "verified_source") ||
        number(mzm_coverage, "expected_count") != (int64_t)mzm)
        fail("Zero Mission savepoint coverage");
    if (!aria_coverage || strcmp(text(aria_coverage, "status"), "verified_rom_map_table") ||
        number(aria_coverage, "expected_count") != (int64_t)aria)
        fail("Aria savepoint coverage");
}

int main(void) {
    char path[4096];
    snprintf(path, sizeof path, "%s/data/story/world_inventory.toml", FUSION_SOURCE_DIR);
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot read %s\n", path);
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *source = malloc((size_t)size + 1);
    size_t read = source ? fread(source, 1, (size_t)size, file) : 0;
    fclose(file);
    if (!source) return 1;
    source[read] = '\0';
    FusionJson data;
    char error[256];
    if (!fusion_toml_parse(source, read, &data, error, sizeof error)) {
        fprintf(stderr, "world inventory: %s\n", error);
        free(source);
        return 1;
    }
    free(source);
    if (strcmp(text(&data, "schema"), "metroid-vania-world-inventory") ||
        number(&data, "version") != 1)
        fail("schema or version");
    test_bosses(&data);
    test_savepoints(&data);
    fusion_json_free(&data);
    if (failures) {
        fprintf(stderr, "%d world inventory failure(s)\n", failures);
        return 1;
    }
    puts("world inventory: ok");
    return 0;
}
