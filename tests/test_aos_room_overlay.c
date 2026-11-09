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

int main(void) {
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
