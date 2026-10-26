/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free safety tests of the native raw database import and its JSON
 * reader. */
#define _XOPEN_SOURCE 700
#include "../src/assets/core/raw_import.c"

#include <assert.h>
#ifndef _WIN32
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>

static int remove_entry(const char *path, const struct stat *info, int flag,
                        struct FTW *walk) {
    (void)info; (void)flag; (void)walk;
    return remove(path);
}

static void remove_tree(const char *path) {
    nftw(path, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}
#endif

static uint8_t metroid_data[16], aria_data[16];
static FusionRom metroid = {metroid_data, sizeof metroid_data, FUSION_ASSET_GAME_METROID};
static FusionRom aria = {aria_data, sizeof aria_data, FUSION_ASSET_GAME_ARIA};

static bool import(const char *metroid_db, const char *aria_db, const char *root,
                   char *error, size_t error_size) {
    FusionRawImportSummary summary;
    return fusion_raw_import(&metroid, &aria, metroid_db, strlen(metroid_db), aria_db,
                             strlen(aria_db), root, &summary, error, error_size);
}

static size_t read_all(const char *path, uint8_t *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    if (!file) return SIZE_MAX;
    size_t read = fread(buffer, 1, size, file);
    fclose(file);
    return read;
}

static void test_json_reader(void) {
    FusionJson root;
    char error[128];
    const char text[] = " [{\"a\": \"x\\u00e9\\n\", \"b\": -12, \"c\": [true, null, 1.5]}] ";
    assert(fusion_json_parse(text, strlen(text), &root, error, sizeof error));
    assert(root.kind == FUSION_JSON_ARRAY && root.count == 1);
    const FusionJson *object = &root.items[0];
    assert(!strcmp(fusion_json_member(object, "a")->string, "x\xc3\xa9\n"));
    assert(fusion_json_member(object, "b")->integer == -12);
    const FusionJson *list = fusion_json_member(object, "c");
    assert(list->count == 3 && list->items[0].boolean && !list->items[2].integral);
    assert(!fusion_json_member(object, "missing"));
    fusion_json_free(&root);
    static const char *const invalid[] = {"[1,]", "{\"a\" 1}", "[\"open", "[1] 2", "{"};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index)
        assert(!fusion_json_parse(invalid[index], strlen(invalid[index]), &root, error,
                                  sizeof error));
}

static void test_offsets_bytes_roots_and_manifest(const char *root) {
    for (size_t index = 0; index < 16; ++index) {
        metroid_data[index] = (uint8_t)index;
        aria_data[index] = (uint8_t)(0x80 + index);
    }
    char error[256] = "";
    assert(import("[{\"dir\": \"sound\", \"path\": \"sample.bin\", \"addr\": {\"us\": \"0x2\"},"
                  " \"count\": \"0x3\", \"size\": 1},"
                  " {\"dir\": \"data\", \"path\": \"rooms/brinstar.bin\","
                  " \"addr\": {\"us\": \"0x6\"}, \"count\": {\"us\": \"0x2\"}, \"size\": 2},"
                  " {\"path\": \"no_us.bin\", \"addr\": {\"eu\": \"0x0\"}, \"count\": \"0x1\","
                  " \"size\": 1}]",
                  "[{\"path\": \"block.bin\", \"addr\": {\"us\": \"0x1\"}, \"count\": \"0x1\","
                  " \"size\": 1}]", root, error, sizeof error));
    char path[512];
    uint8_t bytes[4096];
    snprintf(path, sizeof path, "%s/metroid/raw/sound/sample.bin", root);
    assert(read_all(path, bytes, sizeof bytes) == 3 && !memcmp(bytes, "\x02\x03\x04", 3));
    snprintf(path, sizeof path, "%s/metroid/raw/data/rooms/brinstar.bin", root);
    assert(read_all(path, bytes, sizeof bytes) == 4 && bytes[0] == 6 && bytes[3] == 9);
    snprintf(path, sizeof path, "%s/aria/raw/data/block.bin", root);
    assert(read_all(path, bytes, sizeof bytes) == 1 && bytes[0] == 0x81);
    snprintf(path, sizeof path, "%s/metroid/raw/data/no_us.bin", root);
    assert(read_all(path, bytes, sizeof bytes) == SIZE_MAX);
    snprintf(path, sizeof path, "%s/shared/manifests/import_catalog.json", root);
    size_t size = read_all(path, bytes, sizeof bytes - 1);
    assert(size != SIZE_MAX);
    bytes[size] = '\0';
    assert(strstr((char *)bytes, "\"raw_blocks\": 3,\n    \"raw_bytes\": 8,"));
    assert(strstr((char *)bytes, "\"role\": \"raw_audio_or_sound_data\""));
    assert(strstr((char *)bytes, "\"path\": \"aria/raw/data/block.bin\""));
    /* The manifest never carries ROM bytes. */
    assert(!strstr((char *)bytes, "\x81"));
    assert(!strcmp(classify("data", "rooms/brinstar.bin"), "raw_room_or_background_data"));
    assert(!strcmp(classify("data", "sprites/zoomer.gfx"), "raw_sprite_or_object_data"));
    assert(!strcmp(classify("data", "block.bin"), "raw_unclassified_data"));
}

static void test_rejections(const char *root) {
    static const char *const unsafe[] = {"../escape.bin", "/tmp/leak.bin", "../../rom.gba",
                                         "folder/../x", "a\\\\b.bin", "a//b.bin", ""};
    char error[256], database[256];
    for (size_t index = 0; index < sizeof unsafe / sizeof unsafe[0]; ++index) {
        snprintf(database, sizeof database,
                 "[{\"path\": \"%s\", \"addr\": {\"us\": \"0x0\"}, \"count\": \"0x1\","
                 " \"size\": 1}]", unsafe[index]);
        assert(!import("[]", database, root, error, sizeof error));
    }
    /* A bad offset fails before any byte of the world is written. */
    assert(!import("[]", "[{\"path\": \"valid.bin\", \"addr\": {\"us\": \"0x0\"},"
                         " \"count\": \"0x2\", \"size\": 1},"
                         " {\"path\": \"bad.bin\", \"addr\": {\"us\": \"0x9\"},"
                         " \"count\": \"0x100\", \"size\": 1}]", root, error, sizeof error));
    assert(strstr(error, "exceeds ROM boundaries"));
    char path[512];
    uint8_t byte;
    snprintf(path, sizeof path, "%s/aria/raw/data/valid.bin", root);
    assert(read_all(path, &byte, 1) == SIZE_MAX);
    const char *duplicate = "[{\"path\": \"same.bin\", \"addr\": {\"us\": \"0x0\"},"
                            " \"count\": \"0x1\", \"size\": 1},"
                            " {\"path\": \"same.bin\", \"addr\": {\"us\": \"0x1\"},"
                            " \"count\": \"0x1\", \"size\": 1}]";
    assert(!import("[]", duplicate, root, error, sizeof error));
    assert(strstr(error, "duplicate"));
    assert(!fusion_write_private_file(root, "../outside.bin", &byte, 1, error, sizeof error));
}

#ifndef _WIN32
static void test_symlink_redirect(const char *root) {
    char link[512], target[512], error[256];
    snprintf(target, sizeof target, "%s-elsewhere", root);
    snprintf(link, sizeof link, "%s/linked", root);
    assert(mkdir(target, 0777) == 0 && symlink(target, link) == 0);
    uint8_t byte = 1;
    assert(!fusion_write_private_file(root, "linked/block.bin", &byte, 1, error,
                                      sizeof error));
    char path[1024];
    snprintf(path, sizeof path, "%s/block.bin", target);
    assert(read_all(path, &byte, 1) == SIZE_MAX);
}
#endif

int main(void) {
#ifdef _WIN32
    char root[] = "fusion-raw-import-test";
#else
    char root[] = "fusion-raw-import-XXXXXX";
    assert(mkdtemp(root));
#endif
    test_json_reader();
    test_offsets_bytes_roots_and_manifest(root);
    test_rejections(root);
#ifndef _WIN32
    test_symlink_redirect(root);
    char elsewhere[64];
    snprintf(elsewhere, sizeof elsewhere, "%s-elsewhere", root);
    remove_tree(root);
    remove_tree(elsewhere);
#endif
    puts("fusion_raw_import tests passed");
    return 0;
}
