/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests of the development dashboard generator (formerly
 * tests/test_dashboard.py): registry validation, indicators, verification,
 * CTest discovery, coherent output, the bestiary, private-data refusal, the
 * static offline page, the stale-data check and when tests are run. */
#define FUSION_DASHBOARD_NO_MAIN
#include "../src/tools/fusion_dashboard.c"

#include <unistd.h>

static int failures = 0;

#define EXPECT(condition, ...)                                          \
    do {                                                                \
        if (!(condition)) {                                             \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);        \
            fprintf(stderr, __VA_ARGS__);                               \
            fputc('\n', stderr);                                        \
            ++failures;                                                 \
        }                                                               \
    } while (0)

static char *temporary_directory(void) {
    char *directory = strdup("/tmp/fusion-dashboard-test-XXXXXX");
    if (!directory || !mkdtemp(directory)) exit(2);
    return directory;
}

static void remove_directory(char *directory) {
    char command[512];
    snprintf(command, sizeof command, "rm -rf '%s'", directory);
    if (system(command) != 0) fprintf(stderr, "cannot remove %s\n", directory);
    free(directory);
}

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    if (!file) exit(2);
    fputs(text, file);
    fclose(file);
}

static const char header[] = "area\tid\tcategory\tstatus\tfidelity\ttitle\tlocal_sources\ttests\t"
                             "dependencies\tlimitations\tlast_validated\n";

static void row(char *out, size_t size, const char *id, const char *status, const char *area,
                const char *fidelity, const char *validated) {
    snprintf(out, size, "%s\t%s\teditor\t%s\t%s\tSample\tsrc/tools/fusion_dashboard.c\t\t\t\t%s\n", area,
             id, status, fidelity, validated);
}

/* load_registry() on a one-file registry: ``expected`` in the failure, or
 * NULL for success. */
static void registry_case(const char *rows, const char *expected) {
    char *directory = temporary_directory(), path[512];
    snprintf(path, sizeof path, "%s/features.tsv", directory);
    char text[4096];
    snprintf(text, sizeof text, "%s%s", header, rows);
    write_file(path, text);
    FusionJson features = fusion_json_array();
    bool ok = load_registry(path, &features);
    if (expected) EXPECT(!ok && strstr(failure, expected), "registry: %s (%s)", expected, failure);
    else EXPECT(ok, "registry: %s", failure);
    fusion_json_free(&features);
    remove_directory(directory);
}

static void test_registry(void) {
    char path[8192];
    snprintf(path, sizeof path, "%s/data/dashboard/features.tsv", root);
    FusionJson features = fusion_json_array();
    EXPECT(load_registry(path, &features), "tracked registry: %s", failure);
    EXPECT(features.count > 20, "tracked registry: %zu features", features.count);
    for (size_t index = 0; index < features.count; ++index)
        for (size_t other = index + 1; other < features.count; ++other)
            EXPECT(strcmp(text_of(&features.items[index], "id"), text_of(&features.items[other], "id")),
                   "duplicate tracked id %s", text_of(&features.items[index], "id"));
    fusion_json_free(&features);
    char line[512], two[1024];
    row(line, sizeof line, "tools.sample", "done", "tools", "n/a", "");
    registry_case(line, "unknown status");
    row(line, sizeof line, "tools.sample", "functional", "tools", "perfect", "");
    registry_case(line, "unknown fidelity");
    row(line, sizeof line, "aos.x", "functional", "aos", "n/a", "");
    registry_case(line, "unknown area");
    row(line, sizeof line, "assets.x", "functional", "tools", "n/a", "");
    registry_case(line, "does not match");
    row(line, sizeof line, "tools.sample", "functional", "tools", "n/a", "10/10/2026");
    registry_case(line, "YYYY-MM-DD");
    row(line, sizeof line, "tools.sample", "functional", "tools", "n/a", "");
    snprintf(two, sizeof two, "%s%s", line, line);
    registry_case(two, "duplicate");
    registry_case(line, NULL);
    char *directory = temporary_directory();
    snprintf(path, sizeof path, "%s/features.tsv", directory);
    write_file(path, "area\tid\n");
    features = fusion_json_array();
    EXPECT(!load_registry(path, &features) && strstr(failure, "header"), "bad header accepted");
    fusion_json_free(&features);
    remove_directory(directory);
}

static FusionJson feature(const char *status, const char *verdict) {
    FusionJson value = fusion_json_object(), verification = fusion_json_object(),
               sources = fusion_json_array();
    push(&sources, fusion_json_string("x"));
    put_string(&value, "status", status);
    put(&value, "local_sources", sources);
    put_string(&verification, "verdict", verdict);
    put(&value, "verification", verification);
    return value;
}

static int64_t integer_at(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    return value && value->kind == FUSION_JSON_NUMBER ? value->integer : -1;
}

static void test_indicators(void) {
    FusionJson features = fusion_json_array(), result = fusion_json_object();
    push(&features, feature("functional", "passing"));
    push(&features, feature("faithful", "untested"));
    push(&features, feature("partial", "passing"));
    push(&features, feature("unknown", "untested"));
    push(&features, feature("not_started", "untested"));
    indicator(&features, NULL, &result);
    EXPECT(integer_at(&result, "total") == 5, "indicator total");
    int64_t sum = 0;
    const FusionJson *by_status = fusion_json_member(&result, "by_status");
    for (size_t index = 0; index < by_status->count; ++index) sum += by_status->items[index].integer;
    EXPECT(sum == 5, "indicator by_status sum");
    EXPECT(integer_at(&result, "functional_or_better") == 2, "functional_or_better");
    EXPECT(fusion_json_member(&result, "functional_or_better_percent")->number == 40.0, "percent");
    EXPECT(fusion_json_member(&result, "with_passing_tests_percent")->number == 40.0, "passing percent");
    EXPECT(integer_at(&result, "unknown_or_blocked") == 1, "unknown_or_blocked");
    fusion_json_free(&result);
    fusion_json_free(&features);
    features = fusion_json_array();
    result = fusion_json_object();
    indicator(&features, NULL, &result);
    EXPECT(fusion_json_member(&result, "functional_or_better_percent")->kind == FUSION_JSON_NULL,
           "empty percent is null");
    fusion_json_free(&result);
    push(&features, feature("unknown", "untested"));
    push(&features, feature("unknown", "untested"));
    result = fusion_json_object();
    indicator(&features, NULL, &result);
    EXPECT(fusion_json_member(&result, "functional_or_better_percent")->number == 0.0 &&
           fusion_json_member(&result, "functional_or_better_percent")->kind == FUSION_JSON_NUMBER,
           "unknown percent is 0.0");
    EXPECT(integer_at(fusion_json_member(&result, "by_status"), "unknown") == 2, "unknown count");
    fusion_json_free(&result);
    fusion_json_free(&features);
}

static FusionJson parse(const char *text) {
    FusionJson value;
    char error[256];
    if (!fusion_json_parse(text, strlen(text), &value, error, sizeof error)) exit(2);
    return value;
}

static void test_verification(void) {
    FusionJson ctest = parse("{\"sample\": {\"name\": \"sample\", \"files\": [\"tests/test_debug_menu.c\"]}}");
    FusionJson validated = parse("{\"status\": \"validated\", \"local_sources\": "
                                 "[\"src/tools/fusion_dashboard.c\"], \"tests\": "
                                 "[\"tests/test_debug_menu.c\"]}");
    FusionJson result = verify(&validated, &ctest, NULL);
    EXPECT(!strcmp(text_of(&result, "verdict"), "tests_not_run"), "not run verdict");
    EXPECT(fusion_json_member(&result, "warnings")->count > 0, "not run warning");
    fusion_json_free(&result);
    FusionJson passed = parse("{\"sample\": \"passed\"}"), failed = parse("{\"sample\": \"failed\"}");
    result = verify(&validated, &ctest, &passed);
    EXPECT(!strcmp(text_of(&result, "verdict"), "passing") &&
           fusion_json_member(&result, "warnings")->count == 0, "passing verdict");
    fusion_json_free(&result);
    result = verify(&validated, &ctest, &failed);
    EXPECT(!strcmp(text_of(&result, "verdict"), "failing"), "failing verdict");
    fusion_json_free(&result);
    FusionJson missing = parse("{\"status\": \"functional\", \"local_sources\": "
                               "[\"does/not/exist.c\"], \"tests\": []}");
    result = verify(&missing, &ctest, NULL);
    EXPECT(!strcmp(text_of(&result, "verdict"), "missing_sources"), "missing sources");
    fusion_json_free(&result);
    FusionJson orphan = parse("{\"status\": \"partial\", \"local_sources\": [], \"tests\": "
                              "[\"tests/orphan.c\"]}");
    result = verify(&orphan, &ctest, NULL);
    EXPECT(!strcmp(text_of(&result, "verdict"), "unregistered_tests"), "unregistered tests");
    fusion_json_free(&result);
    fusion_json_free(&orphan);
    fusion_json_free(&missing);
    fusion_json_free(&passed);
    fusion_json_free(&failed);
    fusion_json_free(&validated);
    fusion_json_free(&ctest);
    FusionJson tests = cmake_tests();
    const FusionJson *aos = fusion_json_member(&tests, "aos_enemy");
    const FusionJson *gba = fusion_json_member(&tests, "gba_input");
    EXPECT(aos && contains(fusion_json_member(aos, "files"), "tests/test_aos_enemy.c"), "aos_enemy files");
    EXPECT(gba && contains(fusion_json_member(gba, "files"), "tests/test_gba_input.c"), "gba_input files");
    fusion_json_free(&tests);
}

static const FusionJson *row_named(const FusionJson *rows, const char *name) {
    for (size_t index = 0; index < rows->count; ++index)
        if (!strcmp(text_of(&rows->items[index], "name"), name)) return &rows->items[index];
    return NULL;
}

static void test_output(const FusionJson *data) {
    EXPECT(!strcmp(text_of(data, "schema"), schema), "schema");
    const FusionJson *features = fusion_json_member(data, "features");
    const FusionJson *list = fusion_json_member(data, "indicators");
    const FusionJson *overall = &list->items[0];
    EXPECT(!strcmp(text_of(overall, "id"), "global"), "global first");
    EXPECT(integer_at(overall, "total") == (int64_t)features->count, "global total");
    int64_t sum = 0;
    for (size_t index = 1; index < list->count; ++index) sum += integer_at(&list->items[index], "total");
    EXPECT(sum == (int64_t)features->count, "area totals");
    for (size_t index = 0; index < list->count; ++index) {
        const FusionJson *by_status = fusion_json_member(&list->items[index], "by_status");
        int64_t statuses_sum = 0;
        for (size_t status = 0; status < by_status->count; ++status)
            statuses_sum += by_status->items[status].integer;
        EXPECT(statuses_sum == integer_at(&list->items[index], "total"), "by_status total");
        static const char *const keys[] = {"functional_or_better_percent", "with_passing_tests_percent"};
        for (size_t key = 0; key < 2; ++key) {
            const FusionJson *value = fusion_json_member(&list->items[index], keys[key]);
            EXPECT(value->kind == FUSION_JSON_NULL || (value->number >= 0 && value->number <= 100),
                   "percent range");
        }
    }
    bool native = false;
    for (size_t index = 0; index < features->count; ++index) {
        native |= !strcmp(text_of(&features->items[index], "source"), "data/native_parity/annotations.tsv");
        for (size_t other = index + 1; other < features->count; ++other)
            EXPECT(strcmp(text_of(&features->items[index], "id"), text_of(&features->items[other], "id")),
                   "duplicate feature id");
    }
    EXPECT(native, "native features present");
    const FusionJson *aos = fusion_json_member(fusion_json_member(fusion_json_member(data, "inventory"), "games"), "aos");
    EXPECT(aos && integer_at(aos, "routines_attached_to_features") + integer_at(aos, "unassigned_routines") ==
                      integer_at(aos, "routines"), "inventory totals");
    char *report = read_file("data/native_parity/aos_unassigned_routines.tsv", NULL);
    int64_t rows = 0;
    for (const char *c = report ? report : ""; *c; ++c) rows += *c == '\n';
    free(report);
    EXPECT(aos && integer_at(aos, "unassigned_routines") == rows - 1, "unassigned rows");
    EXPECT(rows - 1 > 2900, "unassigned report size");
    EXPECT(!fusion_json_member(fusion_json_member(data, "tests"), "ran")->boolean, "tests not run");
}

static void test_bestiary(const FusionJson *data) {
    const FusionJson *games = fusion_json_member(fusion_json_member(data, "bestiary"), "games");
    const FusionJson *mzm = fusion_json_member(games, "mzm"), *aos = fusion_json_member(games, "aos");
    const FusionJson *mzm_monsters = fusion_json_member(mzm, "monsters");
    const FusionJson *mzm_items = fusion_json_member(mzm, "items");
    const FusionJson *aos_monsters = fusion_json_member(aos, "monsters");
    const FusionJson *aos_items = fusion_json_member(aos, "items");
    EXPECT(mzm_monsters->count >= 90, "mzm monsters");
    EXPECT(aos_monsters->count == 0x71, "aos monsters");
    const FusionJson *all[] = {mzm_monsters, mzm_items, aos_monsters, aos_items};
    for (size_t list = 0; list < 4; ++list)
        for (size_t index = 0; index < all[list]->count; ++index) {
            const FusionJson *item = &all[list]->items[index];
            EXPECT(is_status(text_of(item, "status")), "row status");
            const FusionJson *evidence_value = fusion_json_member(item, "evidence");
            EXPECT(evidence_value && evidence_value->count == 4 &&
                   fusion_json_member(evidence_value, "unit_tests") &&
                   fusion_json_member(evidence_value, "in_game_check") &&
                   fusion_json_member(evidence_value, "emulator_compared") &&
                   fusion_json_member(evidence_value, "played_through"), "row evidence");
        }
    static const char *const done[] = {"Zoomer Yellow", "Sidehopper", "Mother Brain", "Acid Worm",
                                       "Tangle Vine Red Garuta", "Crocomire", "Gekitai Machine"};
    for (size_t index = 0; index < COUNT(done); ++index) {
        const FusionJson *monster = row_named(mzm_monsters, done[index]);
        EXPECT(monster && strcmp(text_of(monster, "status"), "not_started"), "%s started", done[index]);
    }
    static const char *const functional[] = {
        "Small Energy Drop", "Missile Drop", "Multiple Large Energy", "Morph Ball", "Power Grip",
        "Charge Beam", "Long Beam (Chozo statue)", "Plasma Beam (Chozo statue)", "Energy Tank"};
    for (size_t index = 0; index < COUNT(functional); ++index) {
        const FusionJson *item = row_named(mzm_items, functional[index]);
        EXPECT(item && !strcmp(text_of(item, "status"), "functional"), "%s functional", functional[index]);
    }
    EXPECT(!row_named(mzm_items, "Long Beam Hint (Chozo statue)"), "hint statue listed");
    EXPECT(!row_named(mzm_items, "Water Drop"), "water drop listed");
    static const char *const partial[] = {"Skeleton", "Peeping Eye", "Killer Fish", "Bone Pillar", "Buer",
                                          "White Dragon", "Ghost", "Rock Armor", "Giant Ghost",
                                          "Winged Skeleton"};
    static const char *const working[] = {
        "Bat", "Zombie", "Blue Crow", "Zombie Soldier", "Axe Armor", "Skull Archer", "Needles",
        "Medusa Head", "Zombie Officer", "Gargoyle", "Ghost Dancer", "Curly", "Durga", "Valkyrie",
        "Erinys", "Gremlin", "Skeleton Knight", "Dead Warrior", "Fleaman", "Red Crow", "Mimic",
        "Dead Crusader", "Cagnazzo", "Skull Millione", "Lubicant", "Killer Doll", "Lightning Doll",
        "Nemesis", "Beam Skeleton", "Siren", "Catoblepas", "Gorgon", "Harpy", "Tiny Devil", "Imp",
        "Merman", "Ripper"};
    size_t partial_count = 0, working_count = 0;
    for (size_t index = 0; index < aos_monsters->count; ++index) {
        const FusionJson *monster = &aos_monsters->items[index];
        const char *status = text_of(monster, "status"), *name = text_of(monster, "name");
        if (!strcmp(status, "partial")) {
            ++partial_count;
            EXPECT(known(name, partial, COUNT(partial)), "unexpected partial %s", name);
        } else if (!strcmp(status, "functional")) {
            ++working_count;
            EXPECT(known(name, working, COUNT(working)), "unexpected functional %s", name);
        } else if (!strcmp(status, "not_started")) {
            EXPECT(!strncmp(name, "Enemy 0x", 8), "unnamed %s", name);
        }
    }
    EXPECT(partial_count == COUNT(partial), "partial Aria count %zu", partial_count);
    EXPECT(working_count == COUNT(working), "functional Aria count %zu", working_count);
    EXPECT(aos_items->count == COUNT(aria_item_families), "Aria item families");
    for (size_t index = 0; index < aos_items->count; ++index)
        /* Each family follows its pickup feature. */
        EXPECT(strcmp(text_of(&aos_items->items[index], "status"), "not_started") &&
                   *text_of(&aos_items->items[index], "feature"),
               "Aria item status");
}

static void test_engine_switch(void) {
    FusionJson annotations, mapping;
    EXPECT(load_annotations(&annotations), "annotations: %s", failure);
    EXPECT(sprite_annotations(&annotations, &mapping), "sprite annotations: %s", failure);
    static const char *const expected[][2] = {
        {"ZOOMER_YELLOW", "mzm.enemies.zoomer"},
        {"DESSGEEGA_AFTER_LONG_BEAM", "mzm.enemies.dessgeega"},
        {"MOTHER_BRAIN", "mzm.enemies.mother_brain"},
        {"ZEB", "mzm.enemies.pipe_bugs"},
        {"ZEBETITE_ONE_AND_THREE", "mzm.enemies.tourian_defences"},
        {"CHOZO_STATUE_LONG", "mzm.items.chozo_statues"},
        {"MORPH_BALL", "mzm.items.item_sprites"},
        {"MAP_STATION", "mzm.environment.room_gadgets"},
        {"ACID_WORM", "mzm.enemies.acid_worm"},
        {"GADORA_KRAID", "mzm.enemies.gadora"},
        {"GADORA_RIDLEY", "mzm.enemies.gadora"},
        {"IMAGO_LARVA_RIGHT", "mzm.enemies.imago_larva"},
        {"IMAGO_LARVA_LEFT", "mzm.enemies.imago_larva"},
        {"IMAGO_LARVA_RIGHT_SIDE", "mzm.enemies.imago_larva"},
        {"IMAGO_COCOON_AFTER_FIGHT", "mzm.enemies.imago_cocoon"},
        {"IMAGO_COCOON", "mzm.enemies.imago_cocoon"},
        {"DISCOVERED_IMAGO_PASSAGE_EVENT_TRIGGER", "mzm.enemies.imago_cocoon"},
        {"TANGLE_VINE_RED_GARUTA", "mzm.enemies.tangle_vine"},
        {"CROCOMIRE", "mzm.enemies.crocomire"},
        {"GEKITAI_MACHINE", "mzm.enemies.crocomire"}};
    for (size_t index = 0; index < COUNT(expected); ++index)
        EXPECT(!strcmp(text_of(&mapping, expected[index][0]), expected[index][1]), "%s mapping",
               expected[index][0]);
    EXPECT(!fusion_json_member(&mapping, "SMALL_ENERGY_DROP"), "drops are not case labels");
    for (size_t index = 0; index < mapping.count; ++index)
        EXPECT(feature_by_id(&annotations, mapping.items[index].string), "unknown %s",
               mapping.items[index].string);
    fusion_json_free(&mapping);
    FusionJson empty = fusion_json_object();
    const char *engine = "int mzm_sprite_family(int type) {\n    switch (type) {\n"
                         "    case MZM_PSPRITE_SAMPLE:\n        return MZM_FAMILY_SAMPLE;\n"
                         "    default:\n        return MZM_FAMILY_NONE;\n    }\n}";
    EXPECT(!sprite_annotations_of(engine, &empty, &mapping) &&
           strstr(failure, "unknown parity annotation"), "unregistered family accepted");
    fusion_json_free(&empty);
    fusion_json_free(&annotations);
}

static void test_public_outputs(const FusionJson *data) {
    char *directory = temporary_directory(), path[512];
    EXPECT(write_data(data, directory), "write: %s", failure);
    static const char *const names[] = {"data.json", "data.js"};
    for (size_t index = 0; index < 2; ++index) {
        snprintf(path, sizeof path, "%s/%s", directory, names[index]);
        char *text = read_absolute(path);
        EXPECT(text && !strstr(text, "assets/extracted") && !strstr(text, root) &&
               !strstr(text, "data:image"), "%s holds private data", names[index]);
        free(text);
    }
    snprintf(path, sizeof path, "%s/data.js", directory);
    char *script = read_absolute(path);
    snprintf(path, sizeof path, "%s/data.json", directory);
    char *json = read_absolute(path);
    const char *payload = strstr(script, "window.METROIDVANIA_DASHBOARD = ");
    EXPECT(payload, "data.js assignment");
    if (payload) {
        payload += strlen("window.METROIDVANIA_DASHBOARD = ");
        size_t length = strlen(payload);
        while (length && (isspace((unsigned char)payload[length - 1]) || payload[length - 1] == ';')) --length;
        FusionJson left, right;
        char error[256];
        EXPECT(fusion_json_parse(payload, length, &left, error, sizeof error) &&
               fusion_json_parse(json, strlen(json), &right, error, sizeof error) &&
               fusion_json_equal(&left, &right), "data.js carries data.json");
        fusion_json_free(&left);
        fusion_json_free(&right);
    }
    free(script);
    free(json);
    FusionJson leak = parse("{\"leak\": \"assets/extracted/aria/x.bmp\"}");
    EXPECT(!write_data(&leak, directory) && strstr(failure, "assets/extracted"), "leak written");
    fusion_json_free(&leak);
    remove_directory(directory);
}

static void test_static_page(void) {
    static const char *const allowed[] = {"index.html", "style.css", "app.js", "data.js", "data.json"};
    char directory[8192];
    snprintf(directory, sizeof directory, "%.8000s/dashboard", root);
    DIR *handle = opendir(directory);
    size_t found = 0;
    struct dirent *entry;
    while (handle && (entry = readdir(handle)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        ++found;
        EXPECT(known(entry->d_name, allowed, COUNT(allowed)), "unexpected dashboard/%s", entry->d_name);
        char path[8700];
        struct stat status;
        snprintf(path, sizeof path, "%s/%s", directory, entry->d_name);
        EXPECT(!stat(path, &status) && status.st_size < 2 * 1024 * 1024, "%s too large", entry->d_name);
    }
    if (handle) closedir(handle);
    EXPECT(found == COUNT(allowed), "dashboard files");
    char *html = read_file("dashboard/index.html", NULL), *app = read_file("dashboard/app.js", NULL);
    char *css = read_file("dashboard/style.css", NULL);
    EXPECT(html && app && css, "dashboard pages");
    if (!html || !app || !css) return;
    /* The scripts, in order: data.js then app.js. */
    const char *first = strstr(html, "<script src=\"");
    const char *second = first ? strstr(first + 1, "<script src=\"") : NULL;
    EXPECT(first && !strncmp(first + 13, "data.js\"", 8) && second &&
           !strncmp(second + 13, "app.js\"", 7) && !strstr(second + 1, "<script src=\""),
           "dashboard scripts");
    const char *pages[] = {html, css, app};
    for (size_t index = 0; index < 3; ++index) {
        EXPECT(!strstr(pages[index], "src=\"http://") && !strstr(pages[index], "src=\"https://") &&
               !strstr(pages[index], "href=\"http://") && !strstr(pages[index], "href=\"https://"),
               "remote resource");
        EXPECT(!strstr(pages[index], "@import"), "@import");
    }
    EXPECT(!strstr(app, "fetch(") && !strstr(app, "XMLHttpRequest"), "network access");
    EXPECT(strstr(app, "location.protocol === \"file:\"") != NULL, "file protocol");
    EXPECT(strstr(app, "https://github.com/wozt/metroidvania") != NULL, "repository link");
    static const char *const paths[] = {"docs/PROJECT_STATUS.md", "docs/NATIVE_PARITY_CHECKLIST.md",
                                        "data/native_parity/annotations.tsv"};
    for (size_t index = 0; index < 3; ++index) {
        char attribute[256];
        snprintf(attribute, sizeof attribute, "data-repo-path=\"%s\"", paths[index]);
        EXPECT(strstr(html, attribute) != NULL, "%s link", paths[index]);
    }
    char *script = read_file("dashboard/data.js", NULL);
    EXPECT(script && strstr(script, "\"schema\": \"metroidvania-dashboard-v1\""), "data.js schema");
    free(script);
    free(html);
    free(app);
    free(css);
}

static int ctest_calls = 0;
static bool ctest_fails = false;

static FusionJson fake_ctest(const char *build) {
    (void)build;
    ++ctest_calls;
    FusionJson results = fusion_json_object();
    put_string(&results, "debug_menu", ctest_fails ? "failed" : "passed");
    return results;
}

static void rewrite(const char *directory, FusionJson *data) {
    EXPECT(write_data(data, directory), "write: %s", failure);
}

static bool has_problem(const FusionJson *problems, const char *text) {
    return contains(problems, text);
}

static void test_check(void) {
    run_ctest = fake_ctest;
    ctest_calls = 0;
    FusionJson data;
    EXPECT(build(false, "", 1767312000, &data), "build: %s", failure);
    char *directory = temporary_directory();
    rewrite(directory, &data);
    FusionJson problems;
    EXPECT(check_problems(directory, &problems) && problems.count == 0, "fresh data reported stale");
    fusion_json_free(&problems);
    /* Volatile facts (time, git, outcomes) are not compared. */
    FusionJson volatile_data = copy_of(&data);
    put_string(&volatile_data, "generated_at", "2000-01-01T00:00:00+00:00");
    put_string(fusion_json_member_mut(&volatile_data, "git"), "commit", "0000000");
    put_string(fusion_json_member_mut(&fusion_json_member_mut(&volatile_data, "features")->items[0],
                                      "verification"), "verdict", "passing");
    rewrite(directory, &volatile_data);
    EXPECT(check_problems(directory, &problems) && problems.count == 0, "volatile facts compared");
    fusion_json_free(&problems);
    /* A declared field or a status count that changed is stale. */
    FusionJson *first = &fusion_json_member_mut(&volatile_data, "features")->items[0];
    char limitations[8192], expected[512];
    snprintf(limitations, sizeof limitations, "%s (old)", text_of(first, "limitations"));
    put_string(first, "limitations", limitations);
    FusionJson *overall = &fusion_json_member_mut(&volatile_data, "indicators")->items[0];
    put_int(overall, "functional_or_better", integer_at(overall, "functional_or_better") + 1);
    rewrite(directory, &volatile_data);
    EXPECT(check_problems(directory, &problems), "check: %s", failure);
    snprintf(expected, sizeof expected, "%s: stale limitations", text_of(first, "id"));
    EXPECT(has_problem(&problems, expected), "stale limitations not reported");
    snprintf(expected, sizeof expected, "indicator %s: stale status counts", text_of(overall, "id"));
    EXPECT(has_problem(&problems, expected), "stale counts not reported");
    fusion_json_free(&problems);
    /* A data.js that does not carry data.json is stale. */
    rewrite(directory, &data);
    char path[512];
    snprintf(path, sizeof path, "%s/data.js", directory);
    write_file(path, "// x\nwindow.METROIDVANIA_DASHBOARD = {};\n");
    EXPECT(check_problems(directory, &problems) &&
           has_problem(&problems, "data.js does not carry data.json"), "data.js mismatch");
    fusion_json_free(&problems);
    EXPECT(!ctest_calls, "tests run without being asked");
    fusion_json_free(&volatile_data);
    fusion_json_free(&data);
    remove_directory(directory);
}

static void test_run_tests(void) {
    char *directory = temporary_directory(), path[512];
    snprintf(path, sizeof path, "%s/CTestTestfile.cmake", directory);
    write_file(path, "");
    for (int fails = 0; fails < 2; ++fails) {
        ctest_fails = fails;
        ctest_calls = 0;
        FusionJson data;
        EXPECT(build(true, directory, time(NULL), &data), "build: %s", failure);
        EXPECT(ctest_calls == 1, "the suite runs once");
        const FusionJson *summary = fusion_json_member(fusion_json_member(&data, "tests"), "summary");
        EXPECT(summary && fusion_json_member(summary, "failed")->integer == fails, "failed outcome counted");
        fusion_json_free(&data);
    }
    remove_directory(directory);
}

int main(void) {
    test_registry();
    test_indicators();
    test_verification();
    run_ctest = fake_ctest;
    FusionJson data;
    if (!build(false, "", 1791590400, &data)) {
        fprintf(stderr, "FAIL: build: %s\n", failure);
        return 1;
    }
    test_output(&data);
    test_bestiary(&data);
    test_engine_switch();
    test_public_outputs(&data);
    fusion_json_free(&data);
    test_static_page();
    test_check();
    test_run_tests();
    if (failures) {
        fprintf(stderr, "%d dashboard failure(s)\n", failures);
        return 1;
    }
    puts("dashboard generator: ok");
    return 0;
}
