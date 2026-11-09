/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's room overlays in the Zero Mission runtime: applied when the
 * document fits the room, ignored otherwise, absent without a document. */
#include "mzm_room_overlay.h"

#include "core/native_map.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static NativeMap map;

static void write_map(const char *path, const char *room, unsigned tileset, unsigned w, unsigned h) {
    memset(&map, 0, sizeof map);
    snprintf(map.room_id, sizeof map.room_id, "%s", room);
    snprintf(map.atlas, sizeof map.atlas, "rooms/metroid/tilesets/17_atlas.bmp");
    map.tileset = tileset;
    map.tile_count = 528;
    for (unsigned l = 0; l < 2; ++l) {
        map.width[l] = w;
        map.height[l] = h;
        for (unsigned k = 0; k < w * h; ++k) map.blocks[l][k] = (uint16_t)(l ? 7 : k);
    }
    char error[128];
    assert(native_map_save(&map, path, error, sizeof error));
}

typedef struct { int x[32], y[32]; unsigned short raw[32]; unsigned char type[32]; int count; } Set;

static void record(void *context, int x, int y, unsigned short raw, unsigned char clip_type) {
    Set *set = context;
    set->x[set->count] = x;
    set->y[set->count] = y;
    set->raw[set->count] = raw;
    set->type[set->count++] = clip_type;
}

static void write_document(const char *path, const char *room_fields, const char *cells) {
    FILE *f = fopen(path, "w");
    fprintf(f, "{\"schema\":\"metroidvania.project-room-data\",\"version\":4,%s,"
               "\"next_id\":1,\"entities\":[],\"collision\":{\"resolution_px\":16,"
               "\"cells\":[%s]}}", room_fields, cells);
    fclose(f);
}

static void collision_tests(void) {
    /* Values 0-6: air, solid, left and right steep slopes, pass-through
     * bottom, then water and lava air blocks; 7 a second solid. */
    static const unsigned char types[8] = {0, 1, 2, 3, 12, 0, 0, 1};
    static const unsigned short behaviors[8] = {0, 0, 0, 0, 0, 0x40, 0x41, 0};
    MzmClipTable table = {types, behaviors, 8};
    unsigned short raw;
    unsigned char type;
    static const unsigned short expected_raw[7] = {1, 4, 6, 3, 2, 5, 0};
    for (int t = 0; t < MZM_PROJECT_COLLISION_TYPES; ++t)
        assert(mzm_project_collision_native(&table, t, &raw, &type) && raw == expected_raw[t] &&
               type == types[raw]);
    MzmClipTable no_lava = {types, behaviors, 6};
    assert(!mzm_project_collision_native(&no_lava, 2, &raw, &type));

    char path[256], message[512];
    assert(mzm_room_document_path(path, sizeof path, ".", "Brinstar", 33) &&
           !strcmp(path, "./entities/brinstar_033.json"));
    snprintf(path, sizeof path, "test-mzm-room-collision.json");
    remove(path);
    Set set = {0};
    int changed = -1;
    assert(mzm_room_collision_apply(path, "Brinstar", 33, 64, 48, &table, record, &set, &changed,
                                    message, sizeof message) == MZM_OVERLAY_NONE);
    const char *room = "\"world\":\"mzm\",\"area\":\"Brinstar\",\"room\":33,"
                       "\"width_px\":64,\"height_px\":48";
    write_document(path, room, "{\"x\":1,\"y\":2,\"type\":\"solid\"},"
                               "{\"x\":3,\"y\":0,\"type\":\"water\"},"
                               "{\"x\":0,\"y\":0,\"type\":\"slope_up\"}");
    assert(mzm_room_collision_apply(path, "Brinstar", 33, 64, 48, &table, record, &set, &changed,
                                    message, sizeof message) == MZM_OVERLAY_APPLIED);
    assert(changed == 3 && set.count == 3 && set.x[0] == 1 && set.y[0] == 2 && set.raw[0] == 1 &&
           set.raw[1] == 5 && set.type[1] == 0 && set.raw[2] == 3 && set.type[2] == 3 &&
           strstr(message, "1 solid") && strstr(message, "1 water"));
    /* Rejected without any cell set: another room, other dimensions, a cell
     * outside, an unknown type, a type the tables cannot express. */
    const char *cases[][2] = {
        {"\"world\":\"mzm\",\"area\":\"Brinstar\",\"room\":34,\"width_px\":64,"
         "\"height_px\":48", "{\"x\":1,\"y\":2,\"type\":\"solid\"}"},
        {"\"world\":\"mzm\",\"area\":\"Brinstar\",\"room\":33,\"width_px\":80,"
         "\"height_px\":48", "{\"x\":1,\"y\":2,\"type\":\"solid\"}"},
        {"\"world\":\"mzm\",\"area\":\"Brinstar\",\"room\":33,\"width_px\":64,"
         "\"height_px\":48", "{\"x\":4,\"y\":0,\"type\":\"solid\"}"},
        {"\"world\":\"mzm\",\"area\":\"Brinstar\",\"room\":33,\"width_px\":64,"
         "\"height_px\":48", "{\"x\":0,\"y\":0,\"type\":\"ice\"}"},
    };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; ++k) {
        set.count = 0;
        write_document(path, cases[k][0], cases[k][1]);
        assert(mzm_room_collision_apply(path, "Brinstar", 33, 64, 48, &table, record, &set,
                                        &changed, message, sizeof message) ==
                   MZM_OVERLAY_REJECTED && set.count == 0 && strstr(message, "ignored"));
    }
    write_document(path, room, "{\"x\":0,\"y\":0,\"type\":\"hazard\"}");
    assert(mzm_room_collision_apply(path, "Brinstar", 33, 64, 48, &no_lava, record, &set,
                                    &changed, message, sizeof message) == MZM_OVERLAY_REJECTED &&
           set.count == 0);
    /* A version-1 document (entities only) holds no collision. */
    FILE *f = fopen(path, "w");
    fputs("{\"schema\":\"metroidvania.project-room-entities\",\"version\":1,\"entities\":[]}", f);
    fclose(f);
    assert(mzm_room_collision_apply(path, "Brinstar", 33, 64, 48, &table, record, &set, &changed,
                                    message, sizeof message) == MZM_OVERLAY_NONE);
    remove(path);
}

int main(void) {
    collision_tests();
    char path[256], message[512];
    assert(mzm_room_overlay_path(path, sizeof path, ".", "Brinstar", 7) &&
           !strcmp(path, "./brinstar_007.mvnative"));
    snprintf(path, sizeof path, "test-mzm-room-overlay.mvnative");
    remove(path);
    unsigned short bg1[4 * 3], bg2[4 * 3];
    for (int k = 0; k < 12; ++k) bg1[k] = bg2[k] = 7;
    MzmOverlayLayer layers[2] = {{true, 4, 3, bg1}, {true, 4, 3, bg2}};
    int changed = -1;
    /* No document: the native room as it is. */
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_NONE && changed == 0);
    /* The room's own document: its blocks replace the native ones. */
    write_map(path, "mzm:brinstar:007", 17, 4, 3);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_APPLIED);
    assert(changed == 11 && bg1[0] == 0 && bg1[11] == 11 && bg2[5] == 7 &&
           strstr(message, "11 BG1 and 0 BG2"));
    /* Another room, another tileset, other dimensions: ignored, untouched. */
    for (int k = 0; k < 12; ++k) bg1[k] = 9;
    write_map(path, "mzm:brinstar:008", 17, 4, 3);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_REJECTED && bg1[0] == 9 &&
           strstr(message, "another room"));
    write_map(path, "mzm:brinstar:007", 18, 4, 3);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_REJECTED && bg1[0] == 9);
    write_map(path, "mzm:brinstar:007", 17, 5, 3);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_REJECTED && bg1[0] == 9 &&
           strstr(message, "dimensions"));
    /* A layer the room does not have is left out. */
    write_map(path, "mzm:brinstar:007", 17, 4, 3);
    layers[1].present = false;
    for (int k = 0; k < 12; ++k) bg2[k] = 1;
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_APPLIED && bg2[0] == 1);
    /* A damaged document is ignored. */
    FILE *f = fopen(path, "w");
    fputs("MVNATIVE 1\nROOM mzm:brinstar:007\n", f);
    fclose(f);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_REJECTED);
    remove(path);
    puts("mzm_room_overlay: ok");
    return 0;
}
