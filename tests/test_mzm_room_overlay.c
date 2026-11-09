/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's room overlays in the Zero Mission runtime: applied when the
 * document fits the room, ignored otherwise, absent without a document. */
#include "mzm_room_overlay.h"

#include "core/native_map.h"

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
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
    FILE *f;
    /* The entities referencing native sprites, at their block; unassigned
     * ones skipped; any entity outside or not a sprite: all ignored. */
    f = fopen(path, "w");
    fprintf(f, "{\"schema\":\"metroidvania.project-room-data\",\"version\":4,%s,"
               "\"next_id\":4,\"entities\":[{\"id\":1,\"kind\":\"ENEMY\",\"x\":32,\"y\":16,"
               "\"label\":\"a\",\"native_type\":\"PSPRITE_ZOOMER_YELLOW\"},{\"id\":2,"
               "\"kind\":\"ENEMY\",\"x\":0,\"y\":0,\"label\":\"b\",\"native_type\":"
               "\"unassigned\"}],\"collision\":{\"resolution_px\":16,\"cells\":[]}}", room);
    fclose(f);
    MzmProjectSprite sprites[4];
    assert(mzm_room_project_sprites(path, "Brinstar", 33, 64, 48, sprites, 4, message,
                                    sizeof message) == 1 &&
           sprites[0].x == 2 && sprites[0].y == 1 && !strcmp(sprites[0].name, "ZOOMER_YELLOW") &&
           strstr(message, "1 unassigned"));
    assert(mzm_room_project_sprites(path, "Brinstar", 34, 64, 48, sprites, 4, message,
                                    sizeof message) == -1);
    f = fopen(path, "w");
    fprintf(f, "{\"schema\":\"metroidvania.project-room-data\",\"version\":4,%s,"
               "\"next_id\":2,\"entities\":[{\"id\":1,\"kind\":\"ITEM\",\"x\":32,\"y\":16,"
               "\"label\":\"a\",\"native_type\":\"ITEM_MISSILE_TANK\"}],"
               "\"collision\":{\"resolution_px\":16,\"cells\":[]}}", room);
    fclose(f);
    assert(mzm_room_project_sprites(path, "Brinstar", 33, 64, 48, sprites, 4, message,
                                    sizeof message) == -1 && strstr(message, "not a Zero Mission sprite"));
    remove(path);
    assert(mzm_room_project_sprites(path, "Brinstar", 33, 64, 48, sprites, 4, message,
                                    sizeof message) == 0);
    /* A version-1 document (entities only) holds no collision. */
    f = fopen(path, "w");
    fputs("{\"schema\":\"metroidvania.project-room-entities\",\"version\":1,\"entities\":[]}", f);
    fclose(f);
    assert(mzm_room_collision_apply(path, "Brinstar", 33, 64, 48, &table, record, &set, &changed,
                                    message, sizeof message) == MZM_OVERLAY_NONE);
    remove(path);
}

/* A room document with doors and transitions, in <directory>/entities. */
static void write_doors(const char *path, const char *header, const char *doors,
                        const char *transitions) {
    FILE *f = fopen(path, "w");
    assert(f);
    fprintf(f, "{\"schema\":\"metroidvania.project-room-data\",\"version\":4,%s,"
               "\"next_id\":1,\"entities\":[],\"collision\":{\"resolution_px\":16,\"cells\":[]},"
               "\"next_door_id\":9,\"doors\":[%s],\"next_transition_id\":9,"
               "\"transitions\":[%s],\"next_event_id\":1,\"events\":[]}",
            header, doors, transitions);
    fclose(f);
}

static void door_tests(void) {
    const char *directory = "test-mzm-room-doors";
    char path[256], target[256], message[640];
    mkdir(directory, 0755);
    snprintf(path, sizeof path, "%s/entities", directory);
    mkdir(path, 0755);
    assert(mzm_room_document_path(path, sizeof path, directory, "Brinstar", 33));
    assert(mzm_room_document_path(target, sizeof target, directory, "Kraid", 5));
    remove(path);
    remove(target);
    MzmProjectDoor doors[4];
    assert(mzm_room_project_doors(path, directory, "Brinstar", 33, 64, 48, doors, 4, message,
                                  sizeof message) == 0);
    const char *room = "\"world\":\"mzm\",\"area\":\"Brinstar\",\"room\":33,"
                       "\"width_px\":64,\"height_px\":48";
    /* A new door with a spawn point, a door to Aria skipped. */
    write_doors(path, room,
                "{\"id\":1,\"x\":48,\"y\":0,\"width\":16,\"height\":48,\"label\":\"a\","
                "\"door_type\":\"normal\",\"facing\":\"right\"},"
                "{\"id\":2,\"x\":0,\"y\":0,\"width\":16,\"height\":32,\"label\":\"b\","
                "\"door_type\":\"portal\",\"facing\":\"left\"}",
                "{\"id\":1,\"source_door_id\":1,\"target_world\":\"mzm\",\"target_area\":\"Kraid\","
                "\"target_room\":5,\"target_door_id\":0,\"spawn_x\":40,\"spawn_y\":95},"
                "{\"id\":2,\"source_door_id\":2,\"target_world\":\"aria\",\"target_area\":\"00\","
                "\"target_room\":3,\"target_door_id\":0,\"spawn_x\":8,\"spawn_y\":8}");
    assert(mzm_room_project_doors(path, directory, "Brinstar", 33, 64, 48, doors, 4, message,
                                  sizeof message) == 1 &&
           strstr(message, "1 to another world skipped"));
    assert(doors[0].id == 1 && doors[0].native_index == -1 && doors[0].x0 == 3 &&
           doors[0].x1 == 3 && doors[0].y0 == 0 && doors[0].y1 == 2 && doors[0].facing == 1 &&
           !strcmp(doors[0].destination, "kraid_005") && doors[0].spawn_x == 40 &&
           doors[0].spawn_y == 95);
    /* An adopted native door without a spawn point arrives beside the saved
     * target door (facing left: one block right of it, at its bottom). */
    const char *adopted =
        "{\"id\":3,\"x\":0,\"y\":16,\"width\":16,\"height\":32,\"label\":\"n\","
        "\"door_type\":\"normal\",\"facing\":\"left\",\"native_source\":{\"index\":71,"
        "\"variant\":\"native\",\"native_type\":\"71\"}}";
    const char *to_target =
        "{\"id\":1,\"source_door_id\":3,\"target_world\":\"mzm\",\"target_area\":\"Kraid\","
        "\"target_room\":5,\"target_door_id\":4,\"spawn_x\":0,\"spawn_y\":0}";
    write_doors(path, room, adopted, to_target);
    assert(mzm_room_project_doors(path, directory, "Brinstar", 33, 64, 48, doors, 4, message,
                                  sizeof message) == -1 && strstr(message, "neither a spawn"));
    write_doors(target, "\"world\":\"mzm\",\"area\":\"Kraid\",\"room\":5,\"width_px\":320,"
                        "\"height_px\":160",
                "{\"id\":4,\"x\":0,\"y\":64,\"width\":16,\"height\":64,\"label\":\"t\","
                "\"door_type\":\"normal\",\"facing\":\"left\"}", "");
    assert(mzm_room_project_doors(path, directory, "Brinstar", 33, 64, 48, doors, 4, message,
                                  sizeof message) == 1 &&
           doors[0].native_index == 71 && doors[0].x0 == 0 && doors[0].y0 == 1 &&
           doors[0].y1 == 2 && doors[0].spawn_x == 32 && doors[0].spawn_y == 128);
    /* Off the block grid, outside, or another room: all ignored. */
    write_doors(path, room,
                "{\"id\":1,\"x\":8,\"y\":0,\"width\":16,\"height\":16,\"label\":\"a\","
                "\"door_type\":\"normal\",\"facing\":\"left\"}",
                "{\"id\":1,\"source_door_id\":1,\"target_world\":\"mzm\",\"target_area\":\"Kraid\","
                "\"target_room\":5,\"target_door_id\":0,\"spawn_x\":40,\"spawn_y\":40}");
    assert(mzm_room_project_doors(path, directory, "Brinstar", 33, 64, 48, doors, 4, message,
                                  sizeof message) == -1 && strstr(message, "block rectangle"));
    write_doors(path, room, adopted, to_target);
    assert(mzm_room_project_doors(path, directory, "Brinstar", 34, 64, 48, doors, 4, message,
                                  sizeof message) == -1);
    assert(mzm_room_project_doors(path, directory, "Brinstar", 33, 64, 48, doors, 0, message,
                                  sizeof message) == -1 && strstr(message, "door slots"));
    remove(path);
    remove(target);
    snprintf(path, sizeof path, "%s/entities", directory);
    rmdir(path);
    rmdir(directory);
}

int main(void) {
    collision_tests();
    door_tests();
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
