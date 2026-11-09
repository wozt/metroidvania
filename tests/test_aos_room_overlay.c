/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's collision in Aria's runtime: applied when the document fits
 * the room, ignored otherwise, absent without a document. */
#include "aos_room_overlay.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_document(const char *path, const char *room_fields, int resolution,
                           const char *cells) {
    FILE *f = fopen(path, "w");
    assert(f);
    fprintf(f, "{\"schema\":\"metroidvania.project-room-data\",\"version\":4,%s,"
               "\"next_id\":1,\"entities\":[],\"collision\":{\"resolution_px\":%d,"
               "\"cells\":[%s]}}",
            room_fields, resolution, cells);
    fclose(f);
}

static void write_entities(const char *path, const char *room_fields, const char *entities) {
    FILE *f = fopen(path, "w");
    assert(f);
    fprintf(f, "{\"schema\":\"metroidvania.project-room-data\",\"version\":4,%s,"
               "\"next_id\":9,\"entities\":[%s],\"collision\":{\"resolution_px\":8,\"cells\":[]}}",
            room_fields, entities);
    fclose(f);
}

static void entity_tests(void) {
    const char *path = "test-aos-room-entities.json";
    const char *room = "\"world\":\"aria\",\"area\":\"3\",\"room\":17,"
                       "\"width_px\":256,\"height_px\":256";
    char message[512];
    AosProjectEntity e[4];
    remove(path);
    assert(aos_room_project_entities(path, 3, 17, 32, 32, e, 4, message, sizeof message) == 0);
    write_entities(path, room,
                   "{\"id\":1,\"kind\":\"ENEMY\",\"x\":32,\"y\":160,\"label\":\"a\","
                   "\"native_type\":\"enemy:0F\"},"
                   "{\"id\":2,\"kind\":\"ITEM\",\"x\":64,\"y\":168,\"label\":\"b\","
                   "\"native_type\":\"pickup:04\",\"settings\":{\"item_id\":38,"
                   "\"parameter_0\":71,\"parameter_1\":38,\"flags\":0}},"
                   "{\"id\":3,\"kind\":\"OBJECT\",\"x\":0,\"y\":0,\"label\":\"c\","
                   "\"native_type\":\"unassigned\"},"
                   "{\"id\":4,\"kind\":\"OBJECT\",\"x\":200,\"y\":96,\"label\":\"d\","
                   "\"native_type\":\"generic-candle:01\"}");
    assert(aos_room_project_entities(path, 3, 17, 32, 32, e, 4, message, sizeof message) == 3 &&
           strstr(message, "1 unassigned"));
    assert(e[0].kind == 1 && e[0].id == 0x0F && e[0].x == 40 && e[0].y == 168 &&
           e[0].param0 == 0 && e[0].param1 == 0);
    assert(e[1].kind == 4 && e[1].id == 4 && e[1].x == 72 && e[1].y == 176 &&
           e[1].param0 == 71 && e[1].param1 == 38 && e[1].flags == 0);
    assert(e[2].kind == 3 && e[2].id == 1 && e[2].x == 208 && e[2].y == 104);
    assert(aos_room_project_entities(path, 3, 17, 32, 32, e, 2, message, sizeof message) == -1 &&
           strstr(message, "entity slots"));
    assert(aos_room_project_entities(path, 3, 18, 32, 32, e, 4, message, sizeof message) == -1);
    const char *rejected[] = {
        "{\"id\":1,\"kind\":\"ENEMY\",\"x\":256,\"y\":0,\"label\":\"a\",\"native_type\":\"enemy:01\"}",
        "{\"id\":1,\"kind\":\"ENEMY\",\"x\":0,\"y\":0,\"label\":\"a\",\"native_type\":\"PSPRITE_ZOOMER\"}",
        "{\"id\":1,\"kind\":\"ENEMY\",\"x\":0,\"y\":0,\"label\":\"a\",\"native_type\":\"enemy:1\"}",
        "{\"id\":1,\"kind\":\"ITEM\",\"x\":0,\"y\":0,\"label\":\"a\",\"native_type\":\"pickup:02\","
        "\"settings\":{\"item_id\":-1,\"parameter_0\":0,\"parameter_1\":0,\"flags\":0}}",
    };
    for (size_t k = 0; k < sizeof rejected / sizeof rejected[0]; ++k) {
        write_entities(path, room, rejected[k]);
        assert(aos_room_project_entities(path, 3, 17, 32, 32, e, 4, message, sizeof message) ==
                   -1 && strstr(message, "ignored"));
    }
    remove(path);
}

int main(void) {
    entity_tests();
    char path[256], message[512];
    assert(aos_room_document_path(path, sizeof path, ".", 3, 17) &&
           !strcmp(path, "./entities/area_03_room_017.json"));
    uint8_t byte = 0xEE;
    static const int expected[7] = {0x03, 0x01, -1, 0x45, 0x41, -1, 0x00};
    for (int t = 0; t < 7; ++t) {
        bool ok = aos_project_collision_byte(t, &byte);
        assert(ok == (expected[t] >= 0) && (!ok || byte == expected[t]));
    }
    assert(!aos_project_collision_byte(7, &byte));

    snprintf(path, sizeof path, "test-aos-room-collision.json");
    remove(path);
    uint8_t cells[4 * 3];
    memset(cells, 0x08, sizeof cells);
    int changed = -1;
    assert(aos_room_collision_apply(path, 3, 17, 4, 3, cells, &changed, message, sizeof message) ==
               AOS_OVERLAY_NONE && changed == 0);
    const char *room = "\"world\":\"aria\",\"area\":\"3\",\"room\":17,"
                       "\"width_px\":32,\"height_px\":24";
    write_document(path, room, 8,
                   "{\"x\":0,\"y\":0,\"type\":\"solid\"},{\"x\":1,\"y\":0,\"type\":\"one_way\"},"
                   "{\"x\":2,\"y\":1,\"type\":\"slope_up\"},{\"x\":3,\"y\":1,\"type\":\"slope_down\"},"
                   "{\"x\":3,\"y\":2,\"type\":\"air\"}");
    assert(aos_room_collision_apply(path, 3, 17, 4, 3, cells, &changed, message, sizeof message) ==
               AOS_OVERLAY_APPLIED && changed == 5 && strstr(message, "5 cells"));
    assert(cells[0] == 0x03 && cells[1] == 0x01 && cells[6] == 0x45 && cells[7] == 0x41 &&
           cells[11] == 0x00 && cells[2] == 0x08 && cells[4] == 0x08);
    /* Anything that does not fit leaves every cell native. */
    const char *rejected[][3] = {
        {"\"world\":\"aria\",\"area\":\"3\",\"room\":18,\"width_px\":32,\"height_px\":24", "8",
         "{\"x\":0,\"y\":0,\"type\":\"solid\"}"},
        {"\"world\":\"mzm\",\"area\":\"3\",\"room\":17,\"width_px\":32,\"height_px\":24", "8",
         "{\"x\":0,\"y\":0,\"type\":\"solid\"}"},
        {"\"world\":\"aria\",\"area\":\"3\",\"room\":17,\"width_px\":40,\"height_px\":24", "8",
         "{\"x\":0,\"y\":0,\"type\":\"solid\"}"},
        {"\"world\":\"aria\",\"area\":\"3\",\"room\":17,\"width_px\":32,\"height_px\":24", "16",
         "{\"x\":0,\"y\":0,\"type\":\"solid\"}"},
        {"\"world\":\"aria\",\"area\":\"3\",\"room\":17,\"width_px\":32,\"height_px\":24", "8",
         "{\"x\":4,\"y\":0,\"type\":\"solid\"}"},
        {"\"world\":\"aria\",\"area\":\"3\",\"room\":17,\"width_px\":32,\"height_px\":24", "8",
         "{\"x\":0,\"y\":0,\"type\":\"lava\"}"},
        {"\"world\":\"aria\",\"area\":\"3\",\"room\":17,\"width_px\":32,\"height_px\":24", "8",
         "{\"x\":0,\"y\":0,\"type\":\"air\"},{\"x\":1,\"y\":1,\"type\":\"water\"}"},
    };
    for (size_t k = 0; k < sizeof rejected / sizeof rejected[0]; ++k) {
        memset(cells, 0x08, sizeof cells);
        int resolution = 0;
        sscanf(rejected[k][1], "%d", &resolution);
        write_document(path, rejected[k][0], resolution, rejected[k][2]);
        assert(aos_room_collision_apply(path, 3, 17, 4, 3, cells, &changed, message,
                                        sizeof message) == AOS_OVERLAY_REJECTED &&
               changed == 0 && strstr(message, "ignored") && cells[0] == 0x08 && cells[4] == 0x08);
    }
    /* A version-1 document (entities only) holds no collision. */
    FILE *f = fopen(path, "w");
    fputs("{\"schema\":\"metroidvania.project-room-entities\",\"version\":1,\"entities\":[]}", f);
    fclose(f);
    assert(aos_room_collision_apply(path, 3, 17, 4, 3, cells, &changed, message, sizeof message) ==
           AOS_OVERLAY_NONE);
    remove(path);
    puts("aos_room_overlay: ok");
    return 0;
}
