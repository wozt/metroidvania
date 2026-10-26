/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "audit.h"
#include "backend.h"
#include "object_catalog.h"
#include "project_room.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif

static void put_string(FusionJson *object, const char *key, const char *value) {
    assert(fusion_json_set(object, key, fusion_json_string(value)));
}

static void put_bool(FusionJson *object, const char *key, bool value) {
    assert(fusion_json_set(object, key, fusion_json_bool(value)));
}

static const FusionJson *member(const FusionJson *object, const char *key,
                                FusionJsonKind kind) {
    const FusionJson *value = fusion_json_member(object, key);
    assert(value != NULL);
    assert(value->kind == kind);
    return value;
}

static const FusionJson *item_with_id(const FusionJson *array, const char *key, int64_t id) {
    assert(array->kind == FUSION_JSON_ARRAY);
    for (size_t index = 0; index < array->count; ++index) {
        const FusionJson *value = fusion_json_member(&array->items[index], key);
        if (value && value->kind == FUSION_JSON_NUMBER && value->integer == id)
            return &array->items[index];
    }
    assert(false);
    return NULL;
}

static FusionJson room_scope(void) {
    FusionJson options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    put_string(&options, "width", "128");
    put_string(&options, "height", "64");
    return options;
}

static FusionJson scoped_room(const char *room, const char *width, const char *height) {
    FusionJson options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", room);
    put_string(&options, "width", width);
    put_string(&options, "height", height);
    return options;
}

static FusionJson execute_ok(const char *command, FusionJson options,
                             const char *root, bool dry_run) {
    FusionJson result = {0};
    FebError error = {0};
    bool ok = feb_execute(command, &options, root, FUSION_SOURCE_DIR, dry_run,
                          &result, &error);
    if (!ok) fprintf(stderr, "%s failed: %s\n", command, error.message);
    assert(ok);
    assert(error.message[0] == '\0');
    fusion_json_free(&options);
    return result;
}

static FusionJson create_door(const char *root, const char *room, const char *x,
                              const char *y, const char *facing) {
    FusionJson options = scoped_room(room, "128", "128");
    put_string(&options, "x", x);
    put_string(&options, "y", y);
    put_string(&options, "door_width", "16");
    put_string(&options, "door_height", "32");
    put_string(&options, "label", !strcmp(room, "7") ? "A" : "B");
    put_string(&options, "door_type", "normal");
    put_string(&options, "facing", facing);
    return execute_ok("door-create", options, root, false);
}

static void execute_error(const char *command, FusionJson options, const char *root,
                          const char *message) {
    FusionJson result = {0};
    FebError error = {0};
    assert(!feb_execute(command, &options, root, FUSION_SOURCE_DIR, false, &result, &error));
    assert(strstr(error.message, message) != NULL);
    fusion_json_free(&result);
    fusion_json_free(&options);
}

static char *read_text(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(!fseek(file, 0, SEEK_END));
    long size = ftell(file);
    assert(size >= 0);
    assert(!fseek(file, 0, SEEK_SET));
    char *text = malloc((size_t)size + 1);
    assert(text != NULL);
    assert(fread(text, 1, (size_t)size, file) == (size_t)size);
    text[size] = '\0';
    assert(!fclose(file));
    return text;
}

static void make_directory(const char *path) {
    char copy[4096];
    assert(strlen(path) < sizeof copy);
    strcpy(copy, path);
    for (char *part = copy + 1; *part; ++part) {
        if (*part != '/') continue;
        *part = '\0';
        assert(!mkdir(copy, 0777) || errno == EEXIST);
        *part = '/';
    }
    assert(!mkdir(copy, 0777) || errno == EEXIST);
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(!fclose(file));
}

static void write_json(const char *path, const FusionJson *value) {
    char *text = NULL;
    size_t size = 0;
    assert(fusion_json_write(value, 2, &text, &size));
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(text, 1, size, file) == size);
    assert(fwrite("\n", 1, 1, file) == 1);
    assert(!fclose(file));
    free(text);
}

static bool option_has_token(const FusionJson *options, const char *token) {
    for (size_t index = 0; index < options->count; ++index) {
        const FusionJson *value = fusion_json_member(&options->items[index], "native_type");
        if (value && value->kind == FUSION_JSON_STRING && !strcmp(value->string, token))
            return true;
    }
    return false;
}

static void remove_tree(const char *path) {
    DIR *directory = opendir(path);
    if (!directory) {
        assert(!unlink(path));
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[4096];
        int length = snprintf(child, sizeof child, "%s/%s", path, entry->d_name);
        assert(length > 0 && (size_t)length < sizeof child);
        struct stat status;
        assert(!lstat(child, &status));
        if (S_ISDIR(status.st_mode) && !S_ISLNK(status.st_mode)) remove_tree(child);
        else assert(!unlink(child));
    }
    assert(!closedir(directory));
    assert(!rmdir(path));
}

enum {
    TEST_GBA_BASE = 0x08000000,
    TEST_ARIA_TEXT_POINTERS = 0x08506B38,
    TEST_ARIA_PALETTE = 0x082099FC,
};

static void put_u16(uint8_t *data, size_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32(const uint8_t *data, size_t offset) {
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
}

static FusionRom synthetic_aria_catalog_rom(void) {
    static const uint32_t graphics[] = {0x081C5E00, 0x081C7E04, 0x081C9E08};
    static const struct {
        uint32_t pointer;
        int count, stride;
    } tables[] = {{0x08505B3C, 32, 16}, {0x08505D3C, 59, 28}, {0x085063B0, 45, 20}};
    static const struct {
        int first, count, start;
    } ranges[] = {{0x5B, 32, 0}, {0x7B, 59, 0}, {0xB6, 45, 0}, {0xE3, 55, 1},
                  {0x11B, 24, 1}, {0x133, 35, 1}, {0x156, 6, 0}};
    FusionRom rom = {.size = 0x518000, .game = FUSION_ASSET_GAME_ARIA};
    rom.data = calloc(1, rom.size);
    assert(rom.data != NULL);
    for (size_t index = 0; index < sizeof graphics / sizeof graphics[0]; ++index) {
        size_t offset = graphics[index] - TEST_GBA_BASE;
        memcpy(rom.data + offset, "\x00\x04\x00\x10", 4);
        memset(rom.data + offset + 4, 0x11, 0x2000);
    }
    size_t palette = TEST_ARIA_PALETTE - TEST_GBA_BASE;
    memcpy(rom.data + palette, "\x00\x00\x05\x00", 4);
    put_u16(rom.data, palette + 6, 0x001F);
    for (size_t table = 0; table < sizeof tables / sizeof tables[0]; ++table)
        for (int item = 0; item < tables[table].count; ++item)
            put_u16(rom.data, tables[table].pointer - TEST_GBA_BASE +
                                    (size_t)item * (size_t)tables[table].stride + 2,
                    0x0401);

    size_t pointer_table = TEST_ARIA_TEXT_POINTERS - TEST_GBA_BASE;
    size_t text = pointer_table + 4 * 0xB50;
    for (size_t range = 0; range < sizeof ranges / sizeof ranges[0]; ++range) {
        for (int item = ranges[range].start;
             item < ranges[range].start + ranges[range].count; ++item) {
            int text_id = ranges[range].first + item - ranges[range].start;
            put_u32(rom.data, pointer_table + (size_t)text_id * 4,
                    TEST_GBA_BASE + (uint32_t)text);
            rom.data[text++] = 1;
            rom.data[text++] = 0;
            int length = snprintf((char *)rom.data + text, rom.size - text,
                                  "Name %03X", text_id);
            assert(length > 0);
            text += (size_t)length;
            rom.data[text++] = 0x0A;
        }
    }
    return rom;
}

static void test_native_aria_item_decoders(void) {
    FusionRom rom = synthetic_aria_catalog_rom();
    char name[96];
    assert(feb_aria_item_name_from_rom(&rom, 2, 0, name, sizeof name));
    assert(!strcmp(name, "Name 05B"));
    assert(feb_aria_item_name_from_rom(&rom, 3, 58, name, sizeof name));
    assert(!strcmp(name, "Name 0B5"));
    assert(feb_aria_item_name_from_rom(&rom, 8, 5, name, sizeof name));
    assert(!strcmp(name, "Name 15B"));
    assert(!feb_aria_item_name_from_rom(&rom, 3, 59, name, sizeof name));
    assert(!feb_aria_item_name_from_rom(&rom, 5, 0, name, sizeof name));
    size_t pointer_table = TEST_ARIA_TEXT_POINTERS - TEST_GBA_BASE;
    size_t first_name = get_u32(rom.data, pointer_table + 0x5B * 4) - TEST_GBA_BASE;
    static const uint8_t controlled_name[] = {
        1, 0, 8, 2, 4, 'P', 'o', 't', 'i', 'o', 'n', ' ', 0xE9, 0x0A};
    memcpy(rom.data + first_name, controlled_name, sizeof controlled_name);
    assert(feb_aria_item_name_from_rom(&rom, 2, 0, name, sizeof name));
    assert(!strcmp(name, "Potion é"));
    put_u32(rom.data, pointer_table + 0x5B * 4, 0x0A000000);
    assert(!feb_aria_item_name_from_rom(&rom, 2, 0, name, sizeof name));
    put_u32(rom.data, pointer_table + 0x5B * 4, TEST_GBA_BASE + (uint32_t)first_name);

    FusionJson options = {0};
    FebError error = {0};
    assert(feb_aria_named_item_options_from_rom(&rom, &options, &error));
    assert(options.count == 771);
    assert(!strcmp(member(&options.items[1], "name", FUSION_JSON_STRING)->string,
                   "Potion é"));
    assert(option_has_token(&options, "hard-mode-pickup:07"));
    fusion_json_free(&options);

    uint8_t page[0x2000];
    assert(feb_aria_icon_page(&rom, 0x081C5E00, page));
    assert(page[0] == 0x11 && page[0x1FFF] == 0x11);
    rom.data[0x1C5E00] = 4;
    assert(!feb_aria_icon_page(&rom, 0x081C5E00, page));
    size_t wrapper = 0x1C5E00;
    size_t packed_at = rom.size - 2048;
    memcpy(rom.data + wrapper, "\x01\x04\x00\x00", 4);
    put_u32(rom.data, wrapper + 4, TEST_GBA_BASE + (uint32_t)packed_at);
    size_t cursor = packed_at;
    memcpy(rom.data + cursor, "\x10\x00\x20\x00", 4);
    cursor += 4;
    int token = 0;
    while (token < 457) {
        size_t flag_at = cursor++;
        uint8_t flags = 0;
        for (int bit = 7; bit >= 0 && token < 457; --bit, ++token) {
            if (token < 2) {
                rom.data[cursor++] = 0;
            } else {
                flags |= (uint8_t)(1 << bit);
                rom.data[cursor++] = 0xF0;
                rom.data[cursor++] = 0;
            }
        }
        rom.data[flag_at] = flags;
    }
    assert(cursor < rom.size);
    assert(feb_aria_icon_page(&rom, 0x081C5E00, page));
    for (size_t index = 0; index < sizeof page; ++index) assert(page[index] == 0);
    memcpy(rom.data + wrapper, "\x00\x04\x00\x10", 4);
    memset(rom.data + wrapper + 4, 0x11, 0x2000);

    char root[] = "/tmp/fusion-aria-icons-XXXXXX";
    assert(mkdtemp(root) != NULL);
    int written = 0;
    assert(feb_aria_item_thumbnails_from_rom(root, &rom, &written));
    assert(written > 130);
    char path[4096];
    assert(snprintf(path, sizeof path,
                    "%s/assets/extracted/sprite_previews/aria/items/03_000.png", root) > 0);
    char *png = read_text(path);
    assert(!memcmp(png, "\x89PNG\r\n\x1a\n", 8));
    free(png);
    assert(feb_aria_item_thumbnails_from_rom(root, &rom, &written));
    assert(written == 0);
    remove_tree(root);
    free(rom.data);
}

static void test_command_registry(void) {
    static const char *const native[] = {
        "capabilities", "project-info", "room-create", "room-export",
        "entity-create", "collision-fill", "event-create", "door-link",
        "tile-fill", "room-audit", "project-audit",
    };
    for (size_t index = 0; index < sizeof native / sizeof native[0]; ++index) {
        bool known = false;
        assert(feb_is_native(native[index]));
        assert(feb_command_fields(native[index], &known) != NULL);
        assert(known);
    }
    assert(!feb_is_native("missing-command"));
    assert(feb_capability("audio-list") != NULL);
    assert(!strcmp(feb_capability("audio-list"),
                   "unavailable_native_inventory_pending"));
}

static void test_audit_contract(void) {
    assert(!strcmp(feb_audit_diagnostic_status("unsupported affine"), "unsupported"));
    assert(!strcmp(feb_audit_diagnostic_status("truncated resource"), "error"));

    FusionJson result = fusion_json_object();
    put_string(&result, "status", "PARTIAL_NATIVE_BG_RENDER");
    assert(fusion_json_set(&result, "width", fusion_json_int(320)));
    assert(fusion_json_set(&result, "height", fusion_json_int(160)));
    FusionJson limitations = fusion_json_array();
    assert(fusion_json_push(&limitations, fusion_json_string("diagnostic only")));
    assert(fusion_json_set(&result, "limitations", limitations));
    FusionJson layer = fusion_json_object(), layers = fusion_json_object();
    assert(fusion_json_set(&layer, "width_blocks", fusion_json_int(20)));
    assert(fusion_json_set(&layer, "height_blocks", fusion_json_int(10)));
    assert(fusion_json_set(&layer, "unresolved_pixels_or_cells", fusion_json_int(7)));
    assert(fusion_json_set(&layers, "Bg1", layer));
    assert(fusion_json_set(&result, "layers", layers));
    FusionJson record = fusion_json_object();
    assert(feb_audit_finish_record(&record, &result, NULL));
    assert(!strcmp(member(&record, "status", FUSION_JSON_STRING)->string, "partial"));
    const FusionJson *expected = member(&record, "expected_render", FUSION_JSON_OBJECT);
    assert(member(expected, "width_px", FUSION_JSON_NUMBER)->integer == 320);
    assert(member(expected, "height_px", FUSION_JSON_NUMBER)->integer == 160);
    const FusionJson *invalid = member(&record, "invalid_graphics_references",
                                       FUSION_JSON_ARRAY);
    assert(invalid->count == 1);
    assert(member(&invalid->items[0], "unresolved_count", FUSION_JSON_NUMBER)->integer == 7);
    fusion_json_free(&record);
    fusion_json_free(&result);

    record = fusion_json_object();
    assert(feb_audit_finish_record(&record, NULL, "unsupported affine"));
    assert(!strcmp(member(&record, "status", FUSION_JSON_STRING)->string, "unsupported"));
    fusion_json_free(&record);
}

static void test_catalog_and_item_contract(void) {
    assert(!strcmp(feb_catalog_role("mzm", "PSPRITE_ZOOMER_RED", "Enemy / actor"),
                   "ENEMY"));
    assert(!strcmp(feb_catalog_role("mzm", "PSPRITE_MISSILE_TANK", "Item / pickup"),
                   "ITEM"));
    assert(!strcmp(feb_catalog_role("mzm", "PSPRITE_POWER_GRIP", "Upgrade / ability"),
                   "ITEM"));
    assert(!strcmp(feb_catalog_role("mzm", "PSPRITE_SAVE_PLATFORM", "World object"),
                   "OBJECT"));
    assert(feb_catalog_role("mzm", "../../ROM", "Enemy / actor") == NULL);
    assert(!strcmp(feb_catalog_role("aria", "enemy:07", "native"), "ENEMY"));
    assert(!strcmp(feb_catalog_role("aria", "special-object:0A", "native"), "ENEMY"));
    assert(!strcmp(feb_catalog_role("aria", "pickup:03", "native"), "ITEM"));
    assert(!strcmp(feb_catalog_role("aria", "special-object:00", "native"), "DOOR"));
    assert(!strcmp(feb_catalog_role("aria", "generic-candle:00", "native"), "OBJECT"));

    FusionJson options = fusion_json_array();
    FebError error = {0};
    assert(feb_aria_item_family_options(&options, &error));
    assert(options.count == 27);
    for (int subtype = 1; subtype <= 8; ++subtype) {
        char token[64];
        snprintf(token, sizeof token, "pickup:%02X", subtype);
        assert(option_has_token(&options, token));
        snprintf(token, sizeof token, "hard-mode-pickup:%02X", subtype);
        assert(option_has_token(&options, token));
        snprintf(token, sizeof token, "all-souls-reward:%02X", subtype);
        assert(option_has_token(&options, token));
    }
    assert(feb_aria_item_family_options(&options, &error));
    assert(options.count == 27);
    fusion_json_free(&options);

    FusionJson type = fusion_json_string("pickup:03"), settings;
    assert(feb_item_settings(&type, 58, 513, 999, 255, &settings, &error));
    assert(member(&settings, "item_id", FUSION_JSON_NUMBER)->integer == 58);
    fusion_json_free(&settings);
    fusion_json_free(&type);
    static const struct {
        const char *type;
        int64_t item, parameter;
    } invalid[] = {{"pickup:03", 59, 0}, {"pickup:08", 6, 0},
                   {"enemy:02", 1, 0}, {"pickup:04", 0, 65536}};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index) {
        error = (FebError){0};
        type = fusion_json_string(invalid[index].type);
        assert(!feb_item_settings(&type, invalid[index].item, invalid[index].parameter, 0, 0,
                                  &settings, &error));
        assert(error.message[0]);
        fusion_json_free(&type);
    }
}

static void create_unassigned_entity(FusionJson *document, const char *kind_text,
                                     int64_t x, int64_t y, const char *label_text) {
    FusionJson kind = fusion_json_string(kind_text), label = fusion_json_string(label_text);
    FusionJson type = fusion_json_string("unassigned"), created;
    FebError error = {0};
    assert(feb_entity_create(document, FUSION_SOURCE_DIR, &kind, x, y, &label, &type, NULL,
                             &created, &error));
    fusion_json_free(&created);
    fusion_json_free(&kind);
    fusion_json_free(&label);
    fusion_json_free(&type);
}

static void test_project_room_storage_and_migrations(void) {
    char root_template[] = "/tmp/fusion-editor-rooms-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    static const struct {
        const char *world, *area;
    } worlds[] = {{"mzm", "Brinstar"}, {"aria", "0"}};
    for (size_t world = 0; world < 2; ++world) {
        FebRoomScope scope = {.world = worlds[world].world, .room = 23,
                              .width = 480, .height = 320};
        strcpy(scope.area, worlds[world].area);
        FusionJson document;
        FebError error = {0};
        assert(feb_room_load(root, &scope, &document, &error));
        create_unassigned_entity(&document, "ENEMY", 0, 16, "Custom enemy");
        create_unassigned_entity(&document, "ITEM", world ? 8 : 16, 16, "Custom item");
        create_unassigned_entity(&document, "OBJECT", world ? 16 : 32, 16, "Custom object");
        char path[4096];
        assert(feb_room_save(root, &document, path, &error));
        assert(strstr(path, "assets/extracted/overrides/") != NULL);
        fusion_json_free(&document);
        assert(feb_room_load(root, &scope, &document, &error));
        assert(member(&document, "entities", FUSION_JSON_ARRAY)->count == 3);
        assert(feb_entity_move(&document, 1, world ? 48 : 48, world ? 96 : 96, &error));
        assert(feb_entity_delete(&document, 2, &error));
        assert(feb_room_save(root, &document, path, &error));
        fusion_json_free(&document);
        assert(feb_room_load(root, &scope, &document, &error));
        const FusionJson *entities = member(&document, "entities", FUSION_JSON_ARRAY);
        assert(entities->count == 2);
        assert(member(&entities->items[0], "id", FUSION_JSON_NUMBER)->integer == 1);
        assert(member(&entities->items[0], "x", FUSION_JSON_NUMBER)->integer == 48);
        assert(member(&entities->items[1], "id", FUSION_JSON_NUMBER)->integer == 3);
        fusion_json_free(&document);
    }

    FebRoomScope legacy_scope = {.world = "mzm", .room = 2, .width = 64, .height = 32};
    strcpy(legacy_scope.area, "Brinstar");
    char legacy_path[4096], parent[4096];
    FebError error = {0};
    assert(feb_room_path(root, &legacy_scope, legacy_path, &error));
    strcpy(parent, legacy_path);
    *strrchr(parent, '/') = '\0';
    make_directory(parent);
    write_text(legacy_path,
               "{\"schema\":\"metroidvania.project-room-entities\",\"version\":1,"
               "\"world\":\"mzm\",\"area\":\"Brinstar\",\"room\":2,"
               "\"width_px\":64,\"height_px\":32,\"next_id\":1,\"entities\":[]}");
    FusionJson document;
    assert(feb_room_load(root, &legacy_scope, &document, &error));
    assert(!strcmp(member(&document, "schema", FUSION_JSON_STRING)->string,
                   "metroidvania.project-room-data"));
    assert(member(&document, "version", FUSION_JSON_NUMBER)->integer == 4);
    assert(member(&document, "doors", FUSION_JSON_ARRAY)->count == 0);
    char *legacy_source = read_text(legacy_path);
    assert(strstr(legacy_source, "metroidvania.project-room-entities") != NULL);
    free(legacy_source);
    fusion_json_free(&document);

    FebRoomScope v2_scope = {.world = "aria", .room = 4, .width = 64, .height = 32};
    strcpy(v2_scope.area, "2");
    assert(feb_room_load(root, &v2_scope, &document, &error));
    assert(fusion_json_set(&document, "version", fusion_json_int(2)));
    fusion_json_remove(&document, "next_event_id");
    fusion_json_remove(&document, "events");
    char path[4096];
    assert(feb_room_path(root, &v2_scope, path, &error));
    strcpy(parent, path);
    *strrchr(parent, '/') = '\0';
    make_directory(parent);
    write_json(path, &document);
    fusion_json_free(&document);
    assert(feb_room_load(root, &v2_scope, &document, &error));
    assert(member(&document, "version", FUSION_JSON_NUMBER)->integer == 4);
    assert(member(&document, "next_event_id", FUSION_JSON_NUMBER)->integer == 1);
    assert(member(&document, "events", FUSION_JSON_ARRAY)->count == 0);
    char *source = read_text(path);
    assert(strstr(source, "\"version\": 2") != NULL);
    free(source);
    fusion_json_free(&document);

    FebRoomScope v3_scope = {.world = "mzm", .room = 4, .width = 64, .height = 32};
    strcpy(v3_scope.area, "Brinstar");
    FusionJson create = scoped_room("4", "64", "32");
    put_string(&create, "event_kind", "EVENT");
    put_string(&create, "x", "0");
    put_string(&create, "y", "0");
    put_string(&create, "region_width", "16");
    put_string(&create, "region_height", "16");
    put_string(&create, "label", "Legacy event");
    put_string(&create, "trigger_type", "enter");
    put_string(&create, "action_type", "checkpoint");
    put_string(&create, "action_ref", "checkpoint:legacy");
    put_bool(&create, "once", false);
    FusionJson result = execute_ok("event-create", create, root, false);
    fusion_json_free(&result);
    assert(feb_room_load(root, &v3_scope, &document, &error));
    assert(fusion_json_set(&document, "version", fusion_json_int(3)));
    FusionJson *event = &fusion_json_member_mut(&document, "events")->items[0];
    fusion_json_remove(event, "condition_mode");
    fusion_json_remove(event, "conditions");
    assert(feb_room_path(root, &v3_scope, path, &error));
    write_json(path, &document);
    fusion_json_free(&document);
    assert(feb_room_load(root, &v3_scope, &document, &error));
    event = &fusion_json_member_mut(&document, "events")->items[0];
    assert(!strcmp(member(event, "condition_mode", FUSION_JSON_STRING)->string, "all"));
    assert(member(event, "conditions", FUSION_JSON_ARRAY)->count == 0);
    source = read_text(path);
    assert(strstr(source, "\"version\": 3") != NULL);
    free(source);
    fusion_json_free(&document);
    remove_tree(root);
}

static bool try_entity_create(FusionJson *document, const char *kind_text, int64_t x, int64_t y,
                              const char *label_text, const char *type_text, FebError *error) {
    FusionJson kind = fusion_json_string(kind_text), label = fusion_json_string(label_text);
    FusionJson type = fusion_json_string(type_text), created = fusion_json_null();
    bool ok = feb_entity_create(document, FUSION_SOURCE_DIR, &kind, x, y, &label, &type, NULL,
                                &created, error);
    fusion_json_free(&created);
    fusion_json_free(&kind);
    fusion_json_free(&label);
    fusion_json_free(&type);
    return ok;
}

static void test_entity_validation_items_and_staging(void) {
    char root_template[] = "/tmp/fusion-editor-entities-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    FebRoomScope scope = {.world = "mzm", .room = 0, .width = 240, .height = 160};
    strcpy(scope.area, "Brinstar");
    FusionJson document;
    FebError error = {0};
    assert(feb_room_load(root, &scope, &document, &error));
    static const struct {
        const char *kind, *label, *type;
        int64_t x;
    } invalid[] = {{"DOOR", "No", "unassigned", 0},
                   {"ENEMY", "No", "unassigned", 1},
                   {"ITEM", "No", "unassigned", 240},
                   {"OBJECT", "bad\nline", "unassigned", 0},
                   {"OBJECT", "Name", "../../rom", 0}};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index) {
        error = (FebError){0};
        assert(!try_entity_create(&document, invalid[index].kind, invalid[index].x, 0,
                                  invalid[index].label, invalid[index].type, &error));
        assert(member(&document, "entities", FUSION_JSON_ARRAY)->count == 0);
    }
    assert(try_entity_create(&document, "ENEMY", 0, 0, "Enemy", "unassigned", &error));
    char path[4096];
    assert(feb_room_save(root, &document, path, &error));
    FusionJson before;
    assert(fusion_json_clone(&document, &before));
    FusionJson wrong = fusion_json_string("PSPRITE_MISSILE_TANK");
    error = (FebError){0};
    assert(!feb_entity_assign(&document, FUSION_SOURCE_DIR, 1, &wrong, NULL, &error));
    assert(fusion_json_equal(&document, &before));
    fusion_json_free(&wrong);
    FusionJson zoomer = fusion_json_string("PSPRITE_ZOOMER_RED");
    error = (FebError){0};
    assert(feb_entity_assign(&document, FUSION_SOURCE_DIR, 1, &zoomer, NULL, &error));
    const FusionJson *entity = &member(&document, "entities", FUSION_JSON_ARRAY)->items[0];
    assert(!strcmp(member(entity, "native_type", FUSION_JSON_STRING)->string,
                   "PSPRITE_ZOOMER_RED"));
    FusionJson label = fusion_json_string("Patrol Zoomer");
    assert(feb_entity_update(&document, FUSION_SOURCE_DIR, 1, 32, 48, &label, &zoomer, NULL,
                             &error));
    fusion_json_free(&before);
    assert(fusion_json_clone(&document, &before));
    error = (FebError){0};
    assert(!feb_entity_update(&document, FUSION_SOURCE_DIR, 1, 17, 48, &label, &zoomer, NULL,
                              &error));
    assert(fusion_json_equal(&document, &before));
    fusion_json_free(&before);
    fusion_json_free(&label);
    fusion_json_free(&zoomer);
    fusion_json_free(&document);

    FebRoomScope wrong_geometry = scope;
    wrong_geometry.width = 320;
    error = (FebError){0};
    assert(!feb_room_load(root, &wrong_geometry, &document, &error));

    char *saved_text = read_text(path), detail[256];
    FusionJson tampered;
    assert(fusion_json_parse(saved_text, strlen(saved_text), &tampered, detail, sizeof detail));
    free(saved_text);
    FusionJson *saved_entity = &fusion_json_member_mut(&tampered, "entities")->items[0];
    assert(fusion_json_set(saved_entity, "id", fusion_json_int(9)));
    write_json(path, &tampered);
    fusion_json_free(&tampered);
    error = (FebError){0};
    assert(!feb_room_load(root, &scope, &document, &error));

    FebRoomScope symlink_scope = {.world = "mzm", .room = 9, .width = 64, .height = 64};
    strcpy(symlink_scope.area, "Brinstar");
    assert(feb_room_path(root, &symlink_scope, path, &error));
    char symlink_parent[4096];
    strcpy(symlink_parent, path);
    *strrchr(symlink_parent, '/') = '\0';
    make_directory(symlink_parent);
    assert(!symlink("/tmp/fusion-editor-nonexistent-room.json", path));
    error = (FebError){0};
    assert(!feb_room_load(root, &symlink_scope, &document, &error));
    assert(strstr(error.message, "symlink") != NULL);

    FebRoomScope aria = {.world = "aria", .room = 7, .width = 512, .height = 512};
    strcpy(aria.area, "3");
    assert(feb_room_load(root, &aria, &document, &error));
    create_unassigned_entity(&document, "ENEMY", 8, 16, "Saved");
    assert(feb_room_save(root, &document, path, &error));
    char live_path[4096];
    strcpy(live_path, path);
    char *live_before = read_text(live_path);
    fusion_json_free(&document);
    assert(!setenv("MV_EDITOR_ROOM_STAGE", "00000000-0000-4000-8000-000000000109", 1));
    assert(feb_room_load(root, &aria, &document, &error));
    assert(feb_entity_move(&document, 1, 24, 32, &error));
    assert(feb_room_save(root, &document, path, &error));
    assert(strstr(path, ".editor_staging") != NULL);
    fusion_json_free(&document);
    assert(feb_room_load(root, &aria, &document, &error));
    assert(member(&member(&document, "entities", FUSION_JSON_ARRAY)->items[0], "x",
                  FUSION_JSON_NUMBER)->integer == 24);
    fusion_json_free(&document);
    char *live_after = read_text(live_path);
    assert(!strcmp(live_before, live_after));
    free(live_before);
    free(live_after);
    assert(!unsetenv("MV_EDITOR_ROOM_STAGE"));
    assert(feb_room_load(root, &aria, &document, &error));
    assert(member(&member(&document, "entities", FUSION_JSON_ARRAY)->items[0], "x",
                  FUSION_JSON_NUMBER)->integer == 8);
    fusion_json_free(&document);
    assert(!setenv("MV_EDITOR_ROOM_STAGE", "../outside", 1));
    error = (FebError){0};
    assert(!feb_room_load(root, &aria, &document, &error));
    assert(strstr(error.message, "stage token") != NULL);
    assert(!unsetenv("MV_EDITOR_ROOM_STAGE"));

    FebRoomScope item_scope = {.world = "aria", .room = 2, .width = 320, .height = 160};
    strcpy(item_scope.area, "0");
    assert(feb_room_load(root, &item_scope, &document, &error));
    create_unassigned_entity(&document, "ITEM", 16, 32, "Gold");
    FusionJson item_type = fusion_json_string("pickup:03"), settings;
    assert(feb_item_settings(&item_type, 58, 513, 999, 255, &settings, &error));
    FusionJson *item = &fusion_json_member_mut(&document, "entities")->items[0];
    assert(fusion_json_set(item, "native_type", item_type));
    assert(fusion_json_set(item, "settings", settings));
    assert(feb_room_validate_scope(&document, &item_scope, &error));
    assert(feb_room_save(root, &document, path, &error));
    fusion_json_free(&document);
    assert(feb_room_load(root, &item_scope, &document, &error));
    item = &fusion_json_member_mut(&document, "entities")->items[0];
    assert(member(member(item, "settings", FUSION_JSON_OBJECT), "item_id",
                  FUSION_JSON_NUMBER)->integer == 58);
    fusion_json_free(&document);

    FebRoomScope mzm_item = {.world = "mzm", .room = 11, .width = 320, .height = 160};
    strcpy(mzm_item.area, "Brinstar");
    assert(feb_room_load(root, &mzm_item, &document, &error));
    create_unassigned_entity(&document, "ITEM", 0, 0, "Invalid typed item");
    item = &fusion_json_member_mut(&document, "entities")->items[0];
    assert(fusion_json_set(item, "settings", fusion_json_object()));
    FusionJson *invalid_settings = fusion_json_member_mut(item, "settings");
    assert(fusion_json_set(invalid_settings, "item_id", fusion_json_int(0)));
    assert(fusion_json_set(invalid_settings, "parameter_0", fusion_json_int(0)));
    assert(fusion_json_set(invalid_settings, "parameter_1", fusion_json_int(0)));
    assert(fusion_json_set(invalid_settings, "flags", fusion_json_int(0)));
    error = (FebError){0};
    assert(!feb_room_validate_scope(&document, &mzm_item, &error));
    fusion_json_free(&document);
    remove_tree(root);
}

static void test_collision_model_contract(void) {
    char root_template[] = "/tmp/fusion-editor-collision-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    static const struct {
        const char *world, *area;
        int64_t resolution;
    } worlds[] = {{"mzm", "Brinstar", 16}, {"aria", "3", 8}};
    for (size_t world = 0; world < 2; ++world) {
        FebRoomScope scope = {.world = worlds[world].world, .room = 5,
                              .width = 256, .height = 256};
        strcpy(scope.area, worlds[world].area);
        FusionJson document;
        FebError error = {0};
        assert(feb_room_load(root, &scope, &document, &error));
        assert(member(member(&document, "collision", FUSION_JSON_OBJECT), "resolution_px",
                      FUSION_JSON_NUMBER)->integer == worlds[world].resolution);
        for (size_t index = 0; index < 7; ++index) {
            FusionJson type = fusion_json_string(feb_collision_types[index]);
            int64_t changed = 0;
            assert(feb_collision_fill(&document, (int64_t)index, 1, 1, 1, &type, &changed,
                                      &error));
            assert(changed == 1);
            const char *stored = NULL;
            assert(feb_collision_get(&document, (int64_t)index, 1, &stored, &error));
            assert(!strcmp(stored, feb_collision_types[index]));
            fusion_json_free(&type);
        }
        int64_t changed = 0;
        assert(feb_collision_clear(&document, 6, 1, 1, 1, &changed, &error));
        assert(changed == 1);
        const char *stored = NULL;
        assert(feb_collision_get(&document, 6, 1, &stored, &error));
        assert(!strcmp(stored, "empty"));
        FusionJson capabilities;
        assert(feb_collision_capabilities(worlds[world].world, &capabilities));
        assert(member(&capabilities, "resolution_px", FUSION_JSON_NUMBER)->integer ==
               worlds[world].resolution);
        assert(member(&capabilities, "types", FUSION_JSON_ARRAY)->count == 7);
        fusion_json_free(&capabilities);
        fusion_json_free(&document);
    }

    FebRoomScope scope = {.world = "mzm", .room = 6, .width = 256, .height = 256};
    strcpy(scope.area, "Brinstar");
    FusionJson document;
    FebError error = {0};
    assert(feb_room_load(root, &scope, &document, &error));
    FusionJson solid = fusion_json_string("solid"), water = fusion_json_string("water");
    FusionJson air = fusion_json_string("air"), platform = fusion_json_string("one_way");
    int64_t changed = 0;
    assert(feb_collision_fill(&document, 1, 1, 3, 2, &solid, &changed, &error));
    const int64_t points[][2] = {{1, 1}, {2, 1}, {2, 2}, {2, 2}};
    assert(feb_collision_stroke(&document, points, 4, &water, &changed, &error));
    assert(changed == 3);
    const char *stored = NULL;
    assert(feb_collision_get(&document, 3, 2, &stored, &error));
    assert(!strcmp(stored, "solid"));
    assert(feb_collision_stroke(&document, points, 4, &air, &changed, &error));
    assert(changed == 3);
    const int64_t last[][2] = {{3, 2}};
    assert(feb_collision_stroke(&document, last, 1, &platform, &changed, &error));
    assert(changed == 1);
    FusionJson before;
    assert(fusion_json_clone(&document, &before));
    FusionJson invented = fusion_json_string("invented");
    error = (FebError){0};
    assert(!feb_collision_fill(&document, 0, 0, 1, 1, &invented, &changed, &error));
    assert(fusion_json_equal(&document, &before));
    fusion_json_free(&before);
    fusion_json_free(&invented);
    fusion_json_free(&solid);
    fusion_json_free(&water);
    fusion_json_free(&air);
    fusion_json_free(&platform);
    fusion_json_free(&document);
    remove_tree(root);
}

static FusionJson event_options(const char *room, const char *kind, const char *x,
                                const char *y, const char *label, const char *action_ref) {
    FusionJson options = scoped_room(room, "64", "64");
    put_string(&options, "event_kind", kind);
    put_string(&options, "x", x);
    put_string(&options, "y", y);
    put_string(&options, "region_width", "16");
    put_string(&options, "region_height", "16");
    put_string(&options, "label", label);
    put_string(&options, "trigger_type", !strcmp(kind, "EVENT") ? "enter" : "interact");
    put_string(&options, "action_type", "checkpoint");
    put_string(&options, "action_ref", action_ref);
    put_bool(&options, "once", !strcmp(kind, "TRIGGER"));
    return options;
}

static void test_event_and_dependency_contract(void) {
    char root_template[] = "/tmp/fusion-editor-events-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    FusionJson options = scoped_room("6", "64", "64");
    put_string(&options, "kind", "ENEMY");
    put_string(&options, "x", "0");
    put_string(&options, "y", "0");
    put_string(&options, "label", "Condition entity");
    put_string(&options, "native_type", "unassigned");
    FusionJson result = execute_ok("entity-create", options, root, false);
    fusion_json_free(&result);

    options = scoped_room("6", "64", "64");
    put_string(&options, "x", "0");
    put_string(&options, "y", "0");
    put_string(&options, "door_width", "16");
    put_string(&options, "door_height", "16");
    put_string(&options, "label", "Condition door");
    put_string(&options, "door_type", "normal");
    put_string(&options, "facing", "left");
    result = execute_ok("door-create", options, root, false);
    fusion_json_free(&result);

    options = scoped_room("6", "64", "64");
    put_string(&options, "source_door_id", "1");
    put_string(&options, "target_world", "aria");
    put_string(&options, "target_area", "0");
    put_string(&options, "target_room", "1");
    put_string(&options, "spawn_x", "0");
    put_string(&options, "spawn_y", "0");
    result = execute_ok("door-link", options, root, false);
    fusion_json_free(&result);

    options = scoped_room("6", "64", "64");
    put_string(&options, "id", "1");
    put_string(&options, "source_door_id", "1");
    put_string(&options, "target_world", "aria");
    put_string(&options, "target_area", "0");
    put_string(&options, "target_room", "1");
    put_string(&options, "spawn_x", "24");
    put_string(&options, "spawn_y", "0");
    result = execute_ok("transition-update", options, root, false);
    assert(member(member(&result, "transition", FUSION_JSON_OBJECT), "spawn_x",
                  FUSION_JSON_NUMBER)->integer == 24);
    fusion_json_free(&result);

    result = execute_ok("event-create",
                        event_options("6", "EVENT", "0", "0", "Prerequisite",
                                      "checkpoint:prerequisite"),
                        root, false);
    fusion_json_free(&result);
    options = event_options("6", "TRIGGER", "16", "16", "Guarded",
                            "checkpoint:guarded");
    put_string(&options, "condition_mode", "any");
    put_string(&options, "conditions",
               "[{\"type\":\"entity_present\",\"ref\":\"entity:1\","
               "\"negated\":false},{\"type\":\"event_complete\","
               "\"ref\":\"event:1\",\"negated\":true},{\"type\":"
               "\"transition_ready\",\"ref\":\"transition:1\","
               "\"negated\":false}]");
    result = execute_ok("event-create", options, root, false);
    const FusionJson *guarded = member(&result, "event", FUSION_JSON_OBJECT);
    assert(!strcmp(member(guarded, "condition_mode", FUSION_JSON_STRING)->string, "any"));
    assert(member(guarded, "conditions", FUSION_JSON_ARRAY)->count == 3);
    fusion_json_free(&result);

    options = scoped_room("6", "64", "64");
    put_string(&options, "id", "1");
    put_bool(&options, "confirm", true);
    execute_error("entity-delete", options, root, "event condition");
    options = scoped_room("6", "64", "64");
    put_string(&options, "id", "1");
    put_bool(&options, "confirm", true);
    execute_error("transition-delete", options, root, "event condition");
    options = scoped_room("6", "64", "64");
    put_string(&options, "id", "1");
    put_bool(&options, "confirm", true);
    execute_error("event-delete", options, root, "event condition");
    options = scoped_room("6", "64", "64");
    put_string(&options, "id", "1");
    put_bool(&options, "confirm", true);
    execute_error("door-delete", options, root, "linked transition");

    options = scoped_room("6", "64", "64");
    put_string(&options, "id", "2");
    put_string(&options, "x", "16");
    put_string(&options, "y", "16");
    put_string(&options, "region_width", "16");
    put_string(&options, "region_height", "16");
    put_string(&options, "label", "Guarded");
    put_string(&options, "trigger_type", "interact");
    put_string(&options, "action_type", "checkpoint");
    put_string(&options, "action_ref", "checkpoint:guarded");
    put_bool(&options, "once", true);
    put_string(&options, "condition_mode", "none");
    execute_error("event-update", options, root, "condition mode");

    options = scoped_room("6", "64", "64");
    result = execute_ok("event-list", options, root, false);
    assert(member(&result, "events", FUSION_JSON_ARRAY)->count == 2);
    fusion_json_free(&result);

    options = fusion_json_object();
    put_string(&options, "world", "aria");
    put_string(&options, "area", "3");
    put_string(&options, "room", "9");
    put_string(&options, "width", "64");
    put_string(&options, "height", "64");
    put_string(&options, "event_kind", "EVENT");
    put_string(&options, "x", "8");
    put_string(&options, "y", "8");
    put_string(&options, "region_width", "16");
    put_string(&options, "region_height", "8");
    put_string(&options, "label", "Aria encounter");
    put_string(&options, "trigger_type", "enter");
    put_string(&options, "action_type", "checkpoint");
    put_string(&options, "action_ref", "checkpoint:aria9");
    put_bool(&options, "once", true);
    result = execute_ok("event-create", options, root, false);
    fusion_json_free(&result);
    options = fusion_json_object();
    put_string(&options, "world", "aria");
    put_string(&options, "area", "3");
    put_string(&options, "room", "9");
    put_string(&options, "width", "64");
    put_string(&options, "height", "64");
    result = execute_ok("event-list", options, root, false);
    assert(member(&result, "events", FUSION_JSON_ARRAY)->count == 1);
    fusion_json_free(&result);
    remove_tree(root);
}

static void test_workspace_source_contract(void) {
    char path[4096];
    assert(snprintf(path, sizeof path, "%s/editor/native_workspace.c", FUSION_SOURCE_DIR) > 0);
    char *source = read_text(path);
    static const char *const terms[] = {
        "project_door_list_targets_0114(", "project_door_target_chosen_0114(",
        "List saved target doors", "--command=door-target-list", "project_door_link_0102(",
        "form->target_choice_ids", "PATCH_0115_RECIPROCAL_GTK", "Prepare return link",
        "--command=door-return-plan", "pending_reverse_0115",
        "project_reverse_apply_0115(doc);", "project_command(doc, \"door-link\", opts, NULL)",
        "First save the forward link with the source room diskette.",
        "item->project_owned", "Project entity", "fusion_editor_cli_program()",
        "\"entity-create\"", "\"collision-set\"", "\"door-create\"",
        "draw_project_collision", "doc->tool_id == TOOL_SELECT",
        "PATCH_0085_ROOM_SPRITE_OVERLAYS", "item->native_overridden) continue;",
        "room_draw_sprite(doc, cr, item, x, y, width, height);",
        "gtk_box_append(GTK_BOX(page), room_color_legend());",
        "\"Enemies\", \"Items\", \"Objects\", \"Doors\", \"Events\"",
        "g_hash_table_destroy(doc->entity_sprite_cache);",
        "PATCH_0110_OPTIONAL_ROOM_STAGING", "if (!has_json && !has_ini)",
        "room_stage_commit_0109(doc)", "MV_EDITOR_ROOM_STAGE",
        "room_stage_path_0109(doc, \"ini\")", "room_history_push_0112(doc->undo",
        "if (success && mutation)", "doc->unsaved ? \" *\" : \"\"",
        "hand-symbolic", "TOOL_GRAB", "case GDK_KEY_m:", "grab_hit_0107(",
        "native_positions_apply_0107(doc);", "native_position_save_0107(doc, item, x, y)",
        "G_KEY_FILE_NONE", "doc->project_aria ? 8 : 16",
        "TOOL_WALL, TOOL_PLATFORM, TOOL_HAZARD, TOOL_SLOPE_UP",
        "gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2)", "collision_icon_toggle_0113(",
        "collision_commit_0113(", "collision_stroke_line_0117(",
        "project_command(doc, \"collision-stroke\", args, NULL)",
        "doc->collision_dragging = FALSE;", "case GDK_KEY_u: doc->tool_id = TOOL_WATER;",
        "case GDK_KEY_p: doc->tool_id = TOOL_PLATFORM;",
        "case GDK_KEY_d: doc->tool_id = TOOL_HAZARD;",
        "case GDK_KEY_r: doc->tool_id = TOOL_SLOPE_UP;",
        "case GDK_KEY_t: doc->tool_id = TOOL_SLOPE_DOWN;",
        "doc->tool_id == TOOL_PLATFORM ? \"one_way\"",
        "doc->tool_id == TOOL_HAZARD ? \"hazard\"",
        "doc->tool_id == TOOL_SLOPE_UP ? \"slope_up\"",
        "doc->tool_id == TOOL_SLOPE_DOWN ? \"slope_down\"",
        "\"mv-room-tool-name\"", "if (!strcmp(name, \"water\")) return 6;",
        "if (!strcmp(name, \"air\")) return 7;", "if (cell->type == 7) continue;",
    };
    for (size_t index = 0; index < sizeof terms / sizeof terms[0]; ++index)
        assert(strstr(source, terms[index]) != NULL);
    assert(strstr(source, "\"input-mouse-symbolic\"") == NULL);
    assert(strstr(source, "GtkWidget *tools = gtk_flow_box_new();") == NULL);
    free(source);
    assert(snprintf(path, sizeof path, "%s/src/editor_backend/commands.c", FUSION_SOURCE_DIR) > 0);
    source = read_text(path);
    assert(strstr(source, "{\"door-target-list\",") != NULL);
    assert(strstr(source, "static const char *const worlds[] = {\"zero_mission\", \"aria\"}") !=
           NULL);
    assert(strstr(source, "feb_audit_write_report(repository, &report, file, path, error)") !=
           NULL);
    assert(strstr(source, "report must be a lowercase safe identifier") != NULL);
    free(source);

    assert(snprintf(path, sizeof path, "%s/editor/world_atlas.c", FUSION_SOURCE_DIR) > 0);
    source = read_text(path);
    static const char *const connection_terms[] = {
        "PATCH_0116_CONNECTION_VISUALIZATION",
        "gtk_overlay_add_overlay(GTK_OVERLAY(map_overlay), grid);",
        "gtk_widget_set_can_target(w->connection_canvas, FALSE);",
        "connection_draw_0116(", "connections_refresh_0116(",
        "All links in current map", "Show connections",
    };
    for (size_t index = 0; index < sizeof connection_terms / sizeof connection_terms[0]; ++index)
        assert(strstr(source, connection_terms[index]) != NULL);
    free(source);
}

static FusionJson adoption_options(const char *index) {
    FusionJson options = scoped_room("10", "640", "480");
    put_string(&options, "native_index", index);
    put_string(&options, "native_variant", "native");
    put_string(&options, "native_type", index);
    return options;
}

static void test_native_door_adoption(void) {
    char root_template[] = "/tmp/fusion-editor-adopt-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    execute_error("door-adopt", adoption_options("26"), root, "import original room annotations");
    char directory[4096], annotation[4096];
    assert(snprintf(directory, sizeof directory,
                    "%s/assets/extracted/rooms/metroid/annotations", root) > 0);
    make_directory(directory);
    assert(snprintf(annotation, sizeof annotation, "%s/brinstar_010.tsv", directory) > 0);
    write_text(annotation, "# private annotation\n"
                           "DOOR|26|32|80|16|64|native|26|Door 26|native original\n");
    FusionJson result = execute_ok("door-adopt", adoption_options("26"), root, false);
    const FusionJson *door = member(&result, "door", FUSION_JSON_OBJECT);
    assert(member(member(door, "native_source", FUSION_JSON_OBJECT), "index",
                  FUSION_JSON_NUMBER)->integer == 26);
    int64_t id = member(door, "id", FUSION_JSON_NUMBER)->integer;
    fusion_json_free(&result);
    result = execute_ok("door-adopt", adoption_options("26"), root, false);
    assert(member(member(&result, "door", FUSION_JSON_OBJECT), "id",
                  FUSION_JSON_NUMBER)->integer == id);
    fusion_json_free(&result);
    FusionJson options = scoped_room("10", "640", "480");
    result = execute_ok("door-list", options, root, false);
    assert(member(&result, "doors", FUSION_JSON_ARRAY)->count == 1);
    fusion_json_free(&result);
    execute_error("door-adopt", adoption_options("28"), root,
                  "source door missing or ambiguous");
    remove_tree(root);
}

static FusionJson link_options(const char *room, const char *source_door,
                               const char *target_room, const char *target_door,
                               const char *spawn_x, const char *spawn_y) {
    FusionJson options = scoped_room(room, "128", "128");
    put_string(&options, "source_door_id", source_door);
    put_string(&options, "target_world", "zero_mission");
    put_string(&options, "target_area", "Brinstar");
    put_string(&options, "target_room", target_room);
    put_string(&options, "target_door_id", target_door);
    put_string(&options, "spawn_x", spawn_x);
    put_string(&options, "spawn_y", spawn_y);
    return options;
}

static void test_target_and_reciprocal_links(void) {
    char root_template[] = "/tmp/fusion-editor-links-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    FusionJson result = create_door(root, "7", "16", "32", "right");
    fusion_json_free(&result);
    result = create_door(root, "8", "48", "64", "left");
    fusion_json_free(&result);

    FusionJson options = fusion_json_object();
    put_string(&options, "target_world", "zero_mission");
    put_string(&options, "target_area", "Brinstar");
    put_string(&options, "target_room", "8");
    result = execute_ok("door-target-list", options, root, false);
    const FusionJson *doors = member(&result, "doors", FUSION_JSON_ARRAY);
    assert(doors->count == 1);
    assert(!strcmp(member(&doors->items[0], "label", FUSION_JSON_STRING)->string, "B"));
    fusion_json_free(&result);

    result = create_door(root, "7", "64", "0", "left");
    fusion_json_free(&result);
    result = create_door(root, "7", "80", "0", "left");
    fusion_json_free(&result);
    result = create_door(root, "7", "96", "0", "left");
    fusion_json_free(&result);
    result = create_door(root, "8", "80", "0", "right");
    fusion_json_free(&result);

    execute_error("door-link", link_options("7", "1", "8", "99", "48", "64"), root,
                  "not present");
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    put_string(&options, "source_door_id", "1");
    execute_error("door-return-plan", options, root, "forward destination");

    result = execute_ok("door-link", link_options("7", "1", "8", "1", "48", "64"), root,
                        false);
    fusion_json_free(&result);
    execute_error("door-link", link_options("7", "1", "8", "1", "48", "64"), root,
                  "already linked");

    options = scoped_room("7", "128", "128");
    put_string(&options, "source_door_id", "3");
    put_string(&options, "target_world", "aria");
    put_string(&options, "target_area", "3");
    put_string(&options, "target_room", "5");
    put_string(&options, "spawn_x", "0");
    put_string(&options, "spawn_y", "0");
    result = execute_ok("door-link", options, root, false);
    fusion_json_free(&result);

    result = execute_ok("door-link", link_options("7", "4", "8", "2", "80", "0"), root,
                        false);
    fusion_json_free(&result);
    options = scoped_room("8", "128", "128");
    put_string(&options, "id", "2");
    put_bool(&options, "confirm", true);
    result = execute_ok("door-delete", options, root, false);
    fusion_json_free(&result);

    FebRoomScope source_scope = {.world = "mzm", .room = 7, .width = 128, .height = 128};
    strcpy(source_scope.area, "Brinstar");
    char source_path[4096];
    FebError path_error = {0};
    assert(feb_room_path(root, &source_scope, source_path, &path_error));
    char *source_before = read_text(source_path);
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("connection-list", options, root, false);
    const FusionJson *connections = member(&result, "connections", FUSION_JSON_ARRAY);
    assert(connections->count == 4);
    assert(!strcmp(member(item_with_id(connections, "door_id", 1), "state",
                          FUSION_JSON_STRING)->string, "simple"));
    assert(!strcmp(member(item_with_id(connections, "door_id", 2), "state",
                          FUSION_JSON_STRING)->string, "missing"));
    assert(!strcmp(member(item_with_id(connections, "door_id", 3), "state",
                          FUSION_JSON_STRING)->string, "interworld"));
    assert(!strcmp(member(item_with_id(connections, "door_id", 4), "state",
                          FUSION_JSON_STRING)->string, "invalid"));
    fusion_json_free(&result);
    char *source_after = read_text(source_path);
    assert(!strcmp(source_before, source_after));
    free(source_before);
    free(source_after);

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    put_string(&options, "source_door_id", "1");
    result = execute_ok("door-return-plan", options, root, false);
    assert(member(&result, "source_room", FUSION_JSON_NUMBER)->integer == 8);
    assert(member(&result, "source_door_id", FUSION_JSON_NUMBER)->integer == 1);
    assert(member(&result, "target_room", FUSION_JSON_NUMBER)->integer == 7);
    assert(member(&result, "target_door_id", FUSION_JSON_NUMBER)->integer == 1);
    assert(member(&result, "spawn_x", FUSION_JSON_NUMBER)->integer == 16);
    assert(member(&result, "spawn_y", FUSION_JSON_NUMBER)->integer == 32);
    fusion_json_free(&result);

    assert(!setenv("MV_EDITOR_ROOM_STAGE", "00000000-0000-4000-8000-000000000115", 1));
    result = execute_ok("door-link", link_options("8", "1", "7", "1", "16", "32"), root,
                        false);
    assert(member(member(&result, "transition", FUSION_JSON_OBJECT), "target_door_id",
                  FUSION_JSON_NUMBER)->integer == 1);
    fusion_json_free(&result);
    assert(!unsetenv("MV_EDITOR_ROOM_STAGE"));
    options = scoped_room("8", "128", "128");
    result = execute_ok("transition-list", options, root, false);
    assert(member(&result, "transitions", FUSION_JSON_ARRAY)->count == 0);
    fusion_json_free(&result);

    result = execute_ok("door-link", link_options("8", "1", "7", "1", "16", "32"), root,
                        false);
    fusion_json_free(&result);
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("connection-list", options, root, false);
    connections = member(&result, "connections", FUSION_JSON_ARRAY);
    const FusionJson *reciprocal = item_with_id(connections, "door_id", 1);
    assert(!strcmp(member(reciprocal, "state", FUSION_JSON_STRING)->string, "reciprocal"));
    assert(member(reciprocal, "reciprocal", FUSION_JSON_BOOL)->boolean);
    fusion_json_free(&result);
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    put_string(&options, "source_door_id", "1");
    execute_error("door-return-plan", options, root, "already has a transition");
    remove_tree(root);
}

static void test_backend_operations(const char *root) {
    FusionJson result = execute_ok("project-info", fusion_json_object(), root, false);
    assert(!strcmp(member(&result, "gameplay_status", FUSION_JSON_STRING)->string,
                   "not_implemented"));
    fusion_json_free(&result);

    FusionJson options = fusion_json_object();
    put_string(&options, "world", "aria");
    result = execute_ok("collision-capabilities", options, root, false);
    assert(member(&result, "resolution_px", FUSION_JSON_NUMBER)->integer == 8);
    assert(member(&result, "types", FUSION_JSON_ARRAY)->count == 7);
    fusion_json_free(&result);

    options = room_scope();
    put_string(&options, "x", "0");
    put_string(&options, "y", "0");
    put_string(&options, "fill_width", "2");
    put_string(&options, "fill_height", "1");
    put_string(&options, "collision_type", "solid");
    result = execute_ok("collision-fill", options, root, false);
    assert(member(&result, "changed_cells", FUSION_JSON_NUMBER)->integer == 2);
    fusion_json_free(&result);

    options = room_scope();
    result = execute_ok("collision-list", options, root, false);
    assert(member(&result, "cells", FUSION_JSON_ARRAY)->count == 2);
    fusion_json_free(&result);

    options = room_scope();
    put_string(&options, "kind", "ENEMY");
    put_string(&options, "x", "16");
    put_string(&options, "y", "16");
    put_string(&options, "label", "Native enemy");
    put_string(&options, "native_type", "unassigned");
    result = execute_ok("entity-create", options, root, false);
    assert(member(member(&result, "entity", FUSION_JSON_OBJECT), "id",
                  FUSION_JSON_NUMBER)->integer == 1);
    fusion_json_free(&result);

    options = room_scope();
    put_string(&options, "x", "0");
    put_string(&options, "y", "0");
    put_string(&options, "door_width", "16");
    put_string(&options, "door_height", "32");
    put_string(&options, "label", "Native gate");
    put_string(&options, "door_type", "normal");
    put_string(&options, "facing", "left");
    result = execute_ok("door-create", options, root, false);
    assert(member(member(&result, "door", FUSION_JSON_OBJECT), "id",
                  FUSION_JSON_NUMBER)->integer == 1);
    fusion_json_free(&result);

    options = room_scope();
    put_string(&options, "event_kind", "TRIGGER");
    put_string(&options, "x", "16");
    put_string(&options, "y", "16");
    put_string(&options, "region_width", "16");
    put_string(&options, "region_height", "16");
    put_string(&options, "label", "Native checkpoint");
    put_string(&options, "trigger_type", "enter");
    put_string(&options, "action_type", "checkpoint");
    put_string(&options, "action_ref", "checkpoint:native_test");
    put_bool(&options, "once", true);
    result = execute_ok("event-create", options, root, false);
    assert(member(member(&result, "event", FUSION_JSON_OBJECT), "id",
                  FUSION_JSON_NUMBER)->integer == 1);
    fusion_json_free(&result);

    options = room_scope();
    result = execute_ok("event-validate", options, root, false);
    const FusionJson *events = member(&result, "events", FUSION_JSON_ARRAY);
    assert(events->count == 1);
    assert(member(&events->items[0], "valid", FUSION_JSON_BOOL)->boolean);
    fusion_json_free(&result);

    FebRoomScope source_scope = {.world = "mzm", .room = 7, .width = 128, .height = 64};
    strcpy(source_scope.area, "Brinstar");
    char source_path[4096];
    struct stat status;
    FebError path_error = {0};
    assert(feb_room_path(root, &source_scope, source_path, &path_error));
    char *source_before = read_text(source_path);

    assert(!setenv("MV_EDITOR_ROOM_STAGE", "00000000-0000-4000-8000-000000000124", 1));
    options = room_scope();
    put_string(&options, "x", "2");
    put_string(&options, "y", "2");
    put_string(&options, "fill_width", "1");
    put_string(&options, "fill_height", "1");
    put_string(&options, "collision_type", "hazard");
    result = execute_ok("collision-fill", options, root, false);
    fusion_json_free(&result);
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("room-export", options, root, true);
    assert(!member(&result, "persisted", FUSION_JSON_BOOL)->boolean);
    fusion_json_free(&result);
    char export_root[4096];
    assert(snprintf(export_root, sizeof export_root, "%s/assets/extracted/exports", root) > 0);
    assert(stat(export_root, &status) != 0);

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("room-export", options, root, false);
    assert(member(member(&result, "counts", FUSION_JSON_OBJECT), "collision",
                  FUSION_JSON_NUMBER)->integer == 2);
    const char *package = member(&result, "package_dir", FUSION_JSON_STRING)->string;
    assert(!stat(package, &status) && S_ISDIR(status.st_mode));
    const char *digest_value = member(&result, "sha256", FUSION_JSON_STRING)->string;
    assert(strlen(digest_value) == 64);
    char digest[65], package_path[4096], preview_path[4096];
    strcpy(digest, digest_value);
    assert(strlen(package) < sizeof package_path);
    strcpy(package_path, package);
    assert(snprintf(preview_path, sizeof preview_path, "%s/preview.tsv", package) > 0);
    char *preview = read_text(preview_path);
    assert(strstr(preview, "MVROOM-PREVIEW\t1\tmzm\tBrinstar\t7\t128\t64\t16\n"));
    assert(strstr(preview, "C\t0\t0\t16\t16\t1\n"));
    assert(strstr(preview, "C\t16\t0\t16\t16\t1\n"));
    assert(strstr(preview, "C\t32\t32\t16\t16\t3\n") == NULL);
    assert(strstr(preview, "D\t0\t0\t16\t32\t0\n"));
    assert(strstr(preview, "E\t16\t16\t16\t16\t0\n"));
    assert(strstr(preview, "V\t16\t16\t16\t16\t0\n"));
    assert(strlen(preview) >= 4 && !strcmp(preview + strlen(preview) - 4, "END\n"));
    free(preview);
    fusion_json_free(&result);
    assert(!unsetenv("MV_EDITOR_ROOM_STAGE"));

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("room-export", options, root, false);
    assert(!strcmp(member(&result, "sha256", FUSION_JSON_STRING)->string, digest));
    assert(!strcmp(member(&result, "package_dir", FUSION_JSON_STRING)->string, package_path));
    fusion_json_free(&result);
    char *source_after = read_text(source_path);
    assert(!strcmp(source_before, source_after));
    free(source_before);
    free(source_after);

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "9");
    execute_error("room-export", options, root, "saved project room");
}

static int run_cli(int argc, char **argv, char *output, size_t capacity) {
    char capture[] = "/tmp/fusion-editor-output-XXXXXX";
    int descriptor = mkstemp(capture);
    assert(descriptor >= 0);
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    assert(saved >= 0);
    assert(dup2(descriptor, STDOUT_FILENO) >= 0);
    bool handled = false;
    int code = feb_cli_main(argc, argv, FUSION_SOURCE_DIR, &handled);
    fflush(stdout);
    assert(dup2(saved, STDOUT_FILENO) >= 0);
    assert(!close(saved));
    assert(handled);
    assert(lseek(descriptor, 0, SEEK_SET) == 0);
    ssize_t size = read(descriptor, output, capacity - 1);
    assert(size >= 0);
    output[size] = '\0';
    assert(!close(descriptor));
    assert(!unlink(capture));
    return code;
}

static void test_cli_and_batch(const char *root) {
    char output[16384], root_option[4096], batch_path[4096], batch_option[4096];
    assert(snprintf(root_option, sizeof root_option, "--root=%s", root) > 0);
    char *info[] = {"fusion_editor_cli", "--command=project-info", "--format=json",
                    root_option};
    assert(run_cli(4, info, output, sizeof output) == FEB_EXIT_SUCCESS);
    assert(strstr(output, "\"success\":true") != NULL);
    assert(strstr(output, "\"gameplay_status\":\"not_implemented\"") != NULL);

    char *unknown[] = {"fusion_editor_cli", "--command=project-info", "--world=aria",
                       "--format=json", root_option};
    assert(run_cli(5, unknown, output, sizeof output) == FEB_EXIT_USAGE);
    assert(strstr(output, "\"type\":\"usage\"") != NULL);

    char *unavailable[] = {"fusion_editor_cli", "--command=audio-list", "--format=json",
                           root_option};
    assert(run_cli(4, unavailable, output, sizeof output) == FEB_EXIT_UNAVAILABLE);
    assert(strstr(output, "\"type\":\"unavailable\"") != NULL);

    int length = snprintf(batch_path, sizeof batch_path, "%s/native-batch.json", root);
    assert(length > 0 && (size_t)length < sizeof batch_path);
    write_text(batch_path,
               "{\"operations\":[{\"command\":\"room-create\","
               "\"world\":\"aria\",\"area\":0,\"slug\":\"dry_room\","
               "\"name\":\"Dry room\",\"width_screens\":1,"
               "\"height_screens\":1}]}\n");
    assert(snprintf(batch_option, sizeof batch_option, "--batch=%s", batch_path) > 0);
    char *batch[] = {"fusion_editor_cli", batch_option, "--atomic=true", "--dry-run=true",
                     "--format=json", root_option};
    assert(run_cli(6, batch, output, sizeof output) == FEB_EXIT_SUCCESS);
    assert(strstr(output, "\"command\":\"room-create\"") != NULL);
    char assets[4096];
    assert(snprintf(assets, sizeof assets, "%s/assets", root) > 0);
    struct stat status;
    assert(stat(assets, &status) != 0);
}

int main(void) {
    char root_template[] = "/tmp/fusion-editor-backend-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    test_command_registry();
    test_audit_contract();
    test_catalog_and_item_contract();
    test_native_aria_item_decoders();
    test_project_room_storage_and_migrations();
    test_entity_validation_items_and_staging();
    test_collision_model_contract();
    test_event_and_dependency_contract();
    test_workspace_source_contract();
    test_native_door_adoption();
    test_target_and_reciprocal_links();
    test_backend_operations(root);

    char cli_template[] = "/tmp/fusion-editor-cli-XXXXXX";
    char *cli_root = mkdtemp(cli_template);
    assert(cli_root != NULL);
    test_cli_and_batch(cli_root);
    remove_tree(cli_root);
    remove_tree(root);
    return 0;
}
