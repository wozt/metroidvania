/* SPDX-License-Identifier: GPL-3.0-only */
/* The native source inventory writer (formerly tested by
 * tests/test_native_parity.py): lexical discovery on a synthetic pair of
 * pinned trees, the tracked inventory rebuilt byte for byte, its known
 * relations, and the checklist's refusal of malformed annotations and
 * corrupt shards. */
#define _POSIX_C_SOURCE 200809L
#include "core/json.h"
#include "fusion/pipeline/inventory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif
#ifndef FUSION_BINARY_DIR
#error "FUSION_BINARY_DIR must identify the build directory"
#endif

static int failures = 0;

#define EXPECT(condition, ...)                                   \
    do {                                                         \
        if (!(condition)) {                                      \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fputc('\n', stderr);                                 \
            ++failures;                                          \
        }                                                        \
    } while (0)

static char *read_all(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    char *text = malloc((size_t)length + 1);
    if (!text) exit(2);
    size_t read = fread(text, 1, (size_t)length, file);
    fclose(file);
    text[read] = '\0';
    if (size) *size = read;
    return text;
}

static void write_file(const char *root, const char *relative, const char *text) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", root, relative);
    for (char *slash = strchr(path + 1, '/'); slash; slash = strchr(slash + 1, '/')) {
        *slash = '\0';
        mkdir(path, 0777);
        *slash = '/';
    }
    FILE *file = fopen(path, "w");
    if (!file) exit(2);
    fputs(text, file);
    fclose(file);
}

static int shell(const char *format, ...) {
    char command[8192];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(command, sizeof command, format, arguments);
    va_end(arguments);
    int status = system(command);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Every record of one game and collection, from the written shards. */
static FusionJson records_of(const char *directory, const char *game, const char *collection) {
    char path[4096];
    snprintf(path, sizeof path, "%s/inventory.json", directory);
    char *text = read_all(path, NULL), error[256];
    FusionJson manifest, result = fusion_json_array();
    if (!text || !fusion_json_parse(text, strlen(text), &manifest, error, sizeof error)) {
        free(text);
        return result;
    }
    free(text);
    const FusionJson *files = fusion_json_member(
        fusion_json_member(fusion_json_member(&manifest, "games"), game), "record_files");
    for (size_t index = 0; files && index < files->count; ++index) {
        const FusionJson *descriptor = &files->items[index];
        if (strcmp(fusion_json_member(descriptor, "collection")->string, collection)) continue;
        snprintf(path, sizeof path, "%s/%s", directory, fusion_json_member(descriptor, "path")->string);
        text = read_all(path, NULL);
        FusionJson shard;
        if (text && fusion_json_parse(text, strlen(text), &shard, error, sizeof error)) {
            const FusionJson *records = fusion_json_member(&shard, "records");
            for (size_t record = 0; records && record < records->count; ++record) {
                FusionJson copy;
                if (!fusion_json_clone(&records->items[record], &copy) || !fusion_json_push(&result, copy))
                    exit(2);
            }
            fusion_json_free(&shard);
        }
        free(text);
    }
    fusion_json_free(&manifest);
    return result;
}

static const FusionJson *by_symbol(const FusionJson *records, const char *symbol) {
    for (size_t index = 0; index < records->count; ++index) {
        const FusionJson *value = fusion_json_member(&records->items[index], "symbol");
        if (value && !strcmp(value->string, symbol)) return &records->items[index];
    }
    return NULL;
}

static const char *text_at(const FusionJson *object, const char *key) {
    const FusionJson *value = object ? fusion_json_member(object, key) : NULL;
    return value && value->kind == FUSION_JSON_STRING ? value->string : "";
}

static bool has_string(const FusionJson *object, const char *key, const char *value) {
    const FusionJson *list = object ? fusion_json_member(object, key) : NULL;
    for (size_t index = 0; list && index < list->count; ++index)
        if (list->items[index].kind == FUSION_JSON_STRING && !strcmp(list->items[index].string, value))
            return true;
    return false;
}

static size_t count_of(const FusionJson *object, const char *key) {
    const FusionJson *list = object ? fusion_json_member(object, key) : NULL;
    return list ? list->count : 0;
}

static const char mzm_player[] =
    "/** @brief 1234 | Public update */\n"
    "void PlayerUpdate(void)\n{ Helper(); }\n"
    "static void Helper(void)\n{\n"
    "  const char *ignored = \"FakeCall()\";\n"
    "  (void)ignored;\n}\n";
static const char mzm_header[] =
    "#define EXAMPLE_LIMIT 4\n"
    "#define EXAMPLE_SCALE(value) ((value) * 2)\n"
    "struct Example { int value; void (*callback)(void); };\n"
    "typedef struct { int x, y; unsigned flags:3, mode:2; } Point;\n"
    "enum ExampleState { EXAMPLE_IDLE, EXAMPLE_ACTIVE = 3 };\n"
    "void PublicFunction(void);\n"
    "extern int ExternalData;\n";
static const char mzm_example[] =
    "typedef void (*ExampleFunc)(void);\n"
    "static ExampleFunc sFunctions[] = { First, Second };\n"
    "static int sFirst = 1, sSecond[2] = {2, 3}, *sThird;\n"
    "void First(void)\n{\n}\n"
    "void Second(void)\n{\n}\n";
static const char mzm_callbacks_header[] =
    "typedef void (*StepFunc)(void);\n"
    "struct Stage { int count; void (*pFunction)(void); StepFunc step; };\n";
static const char mzm_callbacks[] =
    "static const struct Stage sStage = { .pFunction = Fourth, .count = Limit };\n"
    "void Setup(struct Stage *stage)\n{\n"
    "  stage->step = (StepFunc)Fifth;\n"
    "  stage->count = Third;\n}\n"
    "void Third(void)\n{\n}\n"
    "void Fourth(void)\n{\n}\n"
    "void Fifth(void)\n{\n}\n";
static const char mzm_refs[] =
    "void Install(void)\n{\n"
    " CallbackSetVblank(VBlankMain);\n"
    " gState.Handler = Other;\n"
    " p -> Inner = 0;\n"
    " Direct ( 1 );\n"
    " if (sizeof(int)) return;\n}\n"
    "void CallbackSetVblank(void)\n{\n}\n"
    "void VBlankMain(void)\n{\n}\n"
    "void Other(void)\n{\n}\n"
    "void Handler(void)\n{\n}\n"
    "void Inner(void)\n{\n}\n"
    "void Direct(void)\n{\n}\n";
static const char aos_code[] =
    ".global exported_data\nexported_data:\n.word 1\n"
    "thumb_func_start NativeFunction\n"
    "NativeFunction: @ 0x08001000\n"
    "  bl OtherFunction\n"
    "thumb_func_start OtherFunction\n"
    "OtherFunction:\n"
    "  bx lr\n"
    "thumb_func_start EnemyThing\n"
    "EnemyThing: @ 0x08001000\n"
    "  ldr r1, _08001010 @ =sub_08002000\n"
    "  bl sub_08003000\n"
    "_08001010: .4byte sub_08002000\n"
    "_08001014: .4byte 0x0820BF10\n"
    "thumb_func_start sub_08002000\n"
    "sub_08002000:\n"
    "  bx lr\n"
    "thumb_func_start sub_08003000\n"
    "sub_08003000:\n"
    "  bx lr\n";
static const char aos_enemies[] =
    "thumb_func_start EnemyCrowCreate\nEnemyCrowCreate:\n  ldr r0, _p\n_p: .4byte sub_2\n"
    "thumb_func_start EnemyCrowUpdate\nEnemyCrowUpdate:\n  bl sub_1\n"
    "thumb_func_start Object0AUpdate\nObject0AUpdate:\n  bl sub_3\n"
    "thumb_func_start sub_1\nsub_1:\n  bl sub_4\n"
    "thumb_func_start sub_2\nsub_2:\n  bx lr\n"
    "thumb_func_start sub_3\nsub_3:\n  bx lr\n"
    "thumb_func_start sub_4\nsub_4:\n  bx lr\n"
    "thumb_func_start sub_5\nsub_5:\n  bx lr\n";
static const char aos_sound[] = "thumb_func_start SoundMain\nSoundMain:\n  bl sub_3\n";

static void test_synthetic_trees(void) {
    char root[] = "/tmp/fusion-inventory-test-XXXXXX";
    if (!mkdtemp(root)) exit(2);
    char mzm[4096], aos[4096];
    snprintf(mzm, sizeof mzm, "%s/third_party/mzm", root);
    snprintf(aos, sizeof aos, "%s/third_party/cvaos", root);
    write_file(mzm, "src/player.c", mzm_player);
    write_file(mzm, "include/example.h", mzm_header);
    write_file(mzm, "src/example.c", mzm_example);
    write_file(mzm, "include/callbacks.h", mzm_callbacks_header);
    write_file(mzm, "src/callbacks.c", mzm_callbacks);
    write_file(mzm, "src/refs.c", mzm_refs);
    write_file(aos, "asm/code.s", aos_code);
    write_file(aos, "asm/enemies.s", aos_enemies);
    write_file(aos, "asm/sound/main.s", aos_sound);
    const char *trees[] = {mzm, aos};
    for (int tree = 0; tree < 2; ++tree)
        EXPECT(!shell("cd '%s' && git init -q && git -c user.name=t -c user.email=t@t commit -q "
                      "--allow-empty -m fixture", trees[tree]), "git fixture");
    char output[4096], error[1024] = "";
    snprintf(output, sizeof output, "%s/out/inventory.json", root);
    snprintf(mzm, sizeof mzm, "%s/out", root);
    mkdir(mzm, 0777);
    FusionInventorySummary summary[2];
    EXPECT(fusion_inventory_build(root, output, true, summary, error, sizeof error), "build: %s", error);

    FusionJson routines = records_of(mzm, "mzm", "routines");
    const FusionJson *update = by_symbol(&routines, "PlayerUpdate");
    const FusionJson *helper = by_symbol(&routines, "Helper");
    EXPECT(update && helper, "C routines discovered");
    EXPECT(!strcmp(text_at(update, "address"), "0x08001234"), "brief address");
    EXPECT(!strcmp(text_at(update, "description"), "Public update"), "brief description");
    EXPECT(!strcmp(text_at(helper, "linkage"), "internal"), "static linkage");
    EXPECT(has_string(update, "calls", "mzm:c:src/player.c:Helper"), "static call");
    EXPECT(count_of(helper, "calls") == 0 && count_of(helper, "ambiguous_calls") == 0,
           "a string literal is not a call");
    const FusionJson *setup = by_symbol(&routines, "Setup");
    const FusionJson *callbacks = setup ? fusion_json_member(setup, "member_callbacks") : NULL;
    EXPECT(callbacks && callbacks->count == 1 &&
           !strcmp(text_at(&callbacks->items[0], "member"), "step") &&
           !strcmp(text_at(&callbacks->items[0], "target"), "mzm:c:src/callbacks.c:Fifth"),
           "function-pointer member write");
    const FusionJson *install = by_symbol(&routines, "Install");
    EXPECT(has_string(install, "address_references", "mzm:c:src/refs.c:VBlankMain") &&
           has_string(install, "address_references", "mzm:c:src/refs.c:Other"),
           "taken addresses");
    static const char *const excluded[] = {"CallbackSetVblank", "Handler", "Inner", "Direct"};
    for (size_t index = 0; index < 4; ++index) {
        char id[128];
        snprintf(id, sizeof id, "mzm:c:src/refs.c:%s", excluded[index]);
        EXPECT(!has_string(install, "address_references", id), "%s is not a taken address",
               excluded[index]);
    }
    fusion_json_free(&routines);

    FusionJson declarations = records_of(mzm, "mzm", "declarations");
    EXPECT(declarations.count == 1 && by_symbol(&declarations, "PublicFunction"),
           "header declarations");
    fusion_json_free(&declarations);
    FusionJson types = records_of(mzm, "mzm", "types");
    const FusionJson *example = by_symbol(&types, "Example"), *point = by_symbol(&types, "Point");
    const FusionJson *stage = by_symbol(&types, "Stage");
    EXPECT(example && point && by_symbol(&types, "ExampleState"), "types");
    EXPECT(count_of(example, "members") == 2 && has_string(example, "members", "value") &&
           has_string(example, "members", "callback"), "struct members");
    EXPECT(point && fusion_json_member(point, "anonymous") && count_of(point, "members") == 4,
           "anonymous typedef members");
    EXPECT(count_of(stage, "function_pointer_members") == 2 &&
           has_string(stage, "function_pointer_members", "pFunction") &&
           has_string(stage, "function_pointer_members", "step"), "function-pointer members");
    fusion_json_free(&types);
    FusionJson constants = records_of(mzm, "mzm", "constants");
    EXPECT(by_symbol(&constants, "EXAMPLE_LIMIT") && !by_symbol(&constants, "EXAMPLE_SCALE"),
           "object-like constants only");
    fusion_json_free(&constants);
    FusionJson data = records_of(mzm, "mzm", "data_symbols");
    const FusionJson *table = by_symbol(&data, "sFunctions");
    EXPECT(table && fusion_json_member(table, "pointer_table") &&
           has_string(table, "pointer_targets", "mzm:c:src/example.c:First") &&
           has_string(table, "pointer_targets", "mzm:c:src/example.c:Second"), "pointer table");
    EXPECT(by_symbol(&data, "sFirst") && by_symbol(&data, "sSecond") &&
           count_of(by_symbol(&data, "sSecond"), "pointer_targets") == 0, "data declarators");
    EXPECT(!strcmp(text_at(by_symbol(&data, "sThird"), "linkage"), "internal"), "data linkage");
    const FusionJson *staged = by_symbol(&data, "sStage");
    const FusionJson *inits = staged ? fusion_json_member(staged, "member_callbacks") : NULL;
    EXPECT(inits && inits->count == 1 && !strcmp(text_at(&inits->items[0], "member"), "pFunction"),
           "designated initializer callback");
    fusion_json_free(&data);

    snprintf(aos, sizeof aos, "%s/out", root);
    routines = records_of(aos, "aos", "routines");
    EXPECT(!by_symbol(&routines, "exported_data"), "exported data is not a routine");
    EXPECT(has_string(by_symbol(&routines, "NativeFunction"), "calls",
                      "aos:asm:asm/code.s:OtherFunction"), "assembly branch");
    const FusionJson *thing = by_symbol(&routines, "EnemyThing");
    EXPECT(count_of(thing, "address_references") == 1 &&
           has_string(thing, "address_references", "aos:asm:asm/code.s:sub_08002000"),
           "literal pool reference");
    EXPECT(count_of(thing, "calls") == 1 && has_string(thing, "calls", "aos:asm:asm/code.s:sub_08003000"),
           "assembly call");
    static const char *const by_name[][3] = {{"EnemyCrowCreate", "enemies", "name"},
                                             {"Object0AUpdate", "environment", "name"},
                                             {"SoundMain", "audio", "path"},
                                             {"sub_1", "enemies", "referrers"},
                                             {"sub_2", "enemies", "referrers"},
                                             {"sub_4", "enemies", "referrers"},
                                             {"sub_3", "unclassified", ""},
                                             {"sub_5", "unclassified", ""}};
    for (size_t index = 0; index < 8; ++index) {
        const FusionJson *routine = by_symbol(&routines, by_name[index][0]);
        EXPECT(routine && !strcmp(text_at(routine, "category"), by_name[index][1]) &&
               !strcmp(text_at(routine, "category_evidence"), by_name[index][2]),
               "%s classified %s by %s", by_name[index][0], by_name[index][1], by_name[index][2]);
    }
    fusion_json_free(&routines);
    EXPECT(!shell("rm -rf '%s'", root), "cleanup");
}

/* The tracked inventory, rebuilt byte for byte, and its known relations. */
static void test_tracked_inventory(void) {
    char root[] = "/tmp/fusion-inventory-real-XXXXXX";
    if (!mkdtemp(root)) exit(2);
    char output[4096], error[1024] = "";
    snprintf(output, sizeof output, "%s/inventory.json", root);
    FusionInventorySummary summary[2];
    EXPECT(fusion_inventory_build(FUSION_SOURCE_DIR, output, true, summary, error, sizeof error),
           "tracked build: %s", error);
    EXPECT(!shell("diff -r -q '%s' '%s/data/native_parity' -x annotations.tsv -x aos_monster_aspects.tsv -x "
                  "aos_unassigned_routines.tsv >/dev/null", root, FUSION_SOURCE_DIR),
           "the tracked inventory is not what the writer produces");
    EXPECT(summary[0].routines > 2500 && summary[1].routines > 3000, "routine counts");
    const char *tracked = FUSION_SOURCE_DIR "/data/native_parity";
    FusionJson mzm = records_of(tracked, "mzm", "routines"), aos = records_of(tracked, "aos", "routines");
    const FusionJson *handler = by_symbol(&mzm, "SamusExecutePoseHandler");
    const FusionJson *indirect = handler ? fusion_json_member(handler, "indirect_calls") : NULL;
    bool table = false;
    for (size_t index = 0; indirect && index < indirect->count; ++index)
        table |= !strcmp(text_at(&indirect->items[index], "table"),
                         "mzm:data:src/samus.c:sSamusPoseFunctionPointers");
    EXPECT(table, "Samus pose dispatch table");
    const FusionJson *sound = by_symbol(&aos, "SoundInit");
    const FusionJson *callbacks = sound ? fusion_json_member(sound, "member_callbacks") : NULL;
    bool callback = false;
    for (size_t index = 0; callbacks && index < callbacks->count; ++index)
        callback |= !strcmp(text_at(&callbacks->items[index], "member"), "CgbOscOff") &&
                    !strcmp(text_at(&callbacks->items[index], "target"), "aos:c:src/m4a.c:MP2K_event_null") &&
                    has_string(&callbacks->items[index], "types",
                               "aos:type:include/m4a_internal.h:struct:SoundMixerState");
    EXPECT(callback, "SoundInit callback");
    const FusionJson *crow = by_symbol(&aos, "EnemyBlueCrowCreate");
    EXPECT(count_of(crow, "address_references") == 2 &&
           has_string(crow, "address_references", "aos:asm:asm/code/code_08060B98.s:sub_0806E1B8") &&
           has_string(crow, "address_references", "aos:asm:asm/code/code_080C0A1C.s:sub_080CA030"),
           "Blue Crow references");
    const FusionJson *crow_ai = by_symbol(&aos, "sub_080C9AF4");
    EXPECT(!strcmp(text_at(crow_ai, "category"), "enemies") &&
           !strcmp(text_at(crow_ai, "category_evidence"), "referrers"), "Blue Crow AI category");
    EXPECT(has_string(by_symbol(&mzm, "DemoInit"), "address_references", "mzm:c:src/demo.c:DemoVBlank"),
           "DemoInit VBlank reference");
    fusion_json_free(&mzm);
    fusion_json_free(&aos);
    EXPECT(!shell("rm -rf '%s'", root), "cleanup");
}

/* fusion_checklist refuses malformed annotations and corrupt shards. */
static void test_checklist_refusals(void) {
    char root[] = "/tmp/fusion-checklist-test-XXXXXX";
    if (!mkdtemp(root)) exit(2);
    char *annotations = read_all(FUSION_SOURCE_DIR "/data/native_parity/annotations.tsv", NULL);
    char header[1024];
    snprintf(header, sizeof header, "%.*s", (int)(strchr(annotations, '\n') - annotations), annotations);
    free(annotations);
    const char *valid = "mzm\tmzm.player.x\tplayer\tpartial\tTitle\tSamusUpdate\tsrc/runtime/mzm_samus.c\t"
                        "tests/test_mzm_samus.c\t\t\tNotes\tNext\truntime capture\t\t";
    char text[4096];
    static const struct {
        const char *body, *message;
    } cases[] = {
        {"mzm\tonly.two.columns\n", "columns"},
        {"other\tmzm.player.x\tplayer\tpartial\tTitle\t\t\t\t\t\t\t\t\t\t\n", "unknown game"},
        {"mzm\taos.player.x\tplayer\tpartial\tTitle\t\t\t\t\t\t\t\t\t\t\n", "must start with"},
        {NULL, "out of order or duplicated"},
        {"mzm\tmzm.player.x\tplayer\tpartial\ta|b\t\t\t\t\t\t\t\t\t\t\n", "not allowed"},
        {"mzm\tmzm.player.x\tplayer\tpartial\tTitle\t\t\t\t\t\t\t\t\tmaybe\t\n", "empty or 'yes'"},
    };
    for (size_t index = 0; index < sizeof cases / sizeof *cases; ++index) {
        if (cases[index].body)
            snprintf(text, sizeof text, "%s\n%s", header, cases[index].body);
        else
            snprintf(text, sizeof text, "%s\n%s\n%s\n", header, valid,
                     "mzm\tmzm.player.a\tplayer\tpartial\tTitle\t\t\t\t\t\t\t\t\t\t");
        write_file(root, "annotations.tsv", text);
        EXPECT(shell("'%s/fusion_checklist' --annotations '%s/annotations.tsv' --output '%s/c.md' "
                     "--aos-unassigned-output '%s/u.tsv' 2>'%s/err' >/dev/null",
                     FUSION_BINARY_DIR, root, root, root, root) != 0,
               "malformed annotations accepted: %s", cases[index].message);
        char path[4200];
        snprintf(path, sizeof path, "%s/err", root);
        char *err = read_all(path, NULL);
        EXPECT(err && strstr(err, cases[index].message), "message %s: %s", cases[index].message,
               err ? err : "");
        free(err);
    }
    write_file(root, "annotations.tsv", "game\tid\n");
    EXPECT(shell("'%s/fusion_checklist' --annotations '%s/annotations.tsv' --output '%s/c.md' "
                 "--aos-unassigned-output '%s/u.tsv' 2>/dev/null >/dev/null",
                 FUSION_BINARY_DIR, root, root, root) != 0, "bad header accepted");
    /* A manifest whose recorded digest does not match its shard. */
    char *manifest = read_all(FUSION_SOURCE_DIR "/data/native_parity/inventory.json", NULL);
    char *digest = strstr(manifest, "\"sha256\":\"");
    if (digest) memset(digest + 10, '0', 64);
    write_file(root, "inventory.json", manifest);
    free(manifest);
    EXPECT(!shell("cp '%s'/data/native_parity/inventory.*.*.json '%s'/", FUSION_SOURCE_DIR, root), "copy");
    EXPECT(shell("'%s/fusion_checklist' --inventory '%s/inventory.json' --output '%s/c.md' "
                 "--aos-unassigned-output '%s/u.tsv' 2>'%s/err' >/dev/null",
                 FUSION_BINARY_DIR, root, root, root, root) == 2, "corrupt shard accepted");
    char path[4200];
    snprintf(path, sizeof path, "%s/err", root);
    char *err = read_all(path, NULL);
    EXPECT(err && strstr(err, "checksum mismatch"), "checksum message");
    free(err);
    EXPECT(!shell("rm -rf '%s'", root), "cleanup");
}

int main(void) {
    test_synthetic_trees();
    test_tracked_inventory();
    test_checklist_refusals();
    if (failures) {
        fprintf(stderr, "%d inventory failure(s)\n", failures);
        return 1;
    }
    puts("native inventory: ok");
    return 0;
}
